/*
 * CSE4100: System Programming
 *
 * 20171683 임종환
 *
 * Project1: myshell-phase3
 */
#include "myshell.h"

extern char** environ;
int pipeline;	//pipe의 개수
sigset_t mask, prev;
int fg_run;	//fg로 실행중인 경우 1
int job_no;	//JOB추가할 때 부여할 job_id
JOBS* head;	//JOB list의 첫번째
JOBS* tail;	//JOB list의 마지막 

int myshell_parseinput(char* input, char* cmdline_argv[MAXPIPE][MAXARGS]);	// command 입력 parsing
void myshell_execute(int bg, char* cmdline_argv[MAXPIPE][MAXARGS], char* input);// 입력받은 command 실행
int builtin_command(char** cmdline_argv);					// built in command check
void pipelining(int fd[MAXPIPE][2], int i);					// pipe 연결
void sigchld_handler(int sig);							// SIGCHLD handler
void sigint_handler(int sig);							// SIGINT handler
void sigtstp_handler(int sig);							// SIGTSTP handler
void cmd_jobs();								// command "jobs"
void cmd_fg(char* percentjob);							// command "fg %x"
void cmd_bg(char* percentjob);							// command "bg %x"
void cmd_kill(char* percentjob);						// command "kill %x"
void init_job();								// JOBS initialization
void add_job(pid_t process_id, char* input, int bg, int state);			// add JOB
void delete_job(pid_t process_id);						// delete JOB
void print_end_bg();								// 종료된 bg process 출력

int main()
{
    char input[MAXLINE];
    char* cmdline_argv[MAXPIPE][MAXARGS];
    int bg;

    Sigemptyset(&mask);
    Sigaddset(&mask, SIGCHLD);				// SIGCLHD signal BLOCK 하기 위한 mask
    Sigemptyset(&prev);
    Signal(SIGCHLD, sigchld_handler);			// sigchld_handler를 통해 SIGCHLD signal 처리

    do{
	Signal(SIGINT, SIG_IGN);			// 쉘에 명령어를 입력받는 동안 SIGINT signal 무시
	Signal(SIGTSTP, SIG_IGN);			// 쉘에 명령어를 입력받는 동안 SIGTSTP signal 무시
	// Reading
	printf("CSE4100-SP-Proj1-myshell> ");
	fgets(input, MAXLINE, stdin);			// 명령어 입력
	if(!strcmp(input, "\n"))
	    continue;
	// Parsing
	bg = myshell_parseinput(input, cmdline_argv);	// 명령어 parsing
	// Execute
	Signal(SIGINT, sigint_handler);			// 명령어를 실행중일 땐 sigint_handler를 통해 SIGINT signal 처리
	Signal(SIGTSTP, sigtstp_handler);		// 명령어를 실행중일 땐 sigtstp_handler를 통해 SIGTSTP signal 처리
	myshell_execute(bg, cmdline_argv, input);	// 명령어 실행
	print_end_bg();					// 종료된 bg process 출력

    }while(1);
}

int myshell_parseinput(char* input, char* cmdline_argv[MAXPIPE][MAXARGS])
{
    char buf[MAXLINE];
    char* pipe[MAXPIPE];
    int argc;
    int bg = 0;
    char* ptr;

    if(strstr(input, "&") != NULL){		// bg check
	ptr = strtok(input, "&");
	bg = 1;
	input = ptr;
	strcat(input, " ");
    }
    else
	input[strlen(input) - 1] = ' ';

    strcpy(buf, input);

    pipeline = 0;				// pipe라인(|)의 개수
    ptr = strtok(buf, "|");			// pipe라인(|) 단위로 input command 분리
    while(ptr != NULL){
	pipe[pipeline++] = ptr;
	ptr = strtok(NULL, "|");
    }

    for(int i = 0; i < pipeline; i++){		// 각 command의 argument 분리
        ptr = strtok(pipe[i], " ");
        argc = 0;
        while(ptr != NULL){
            if(ptr[0] == '"'){                  // 큰 따옴표로 입력된 argument 처리
                ptr = ptr + 1;
                if(strstr(ptr, "\"") != NULL)
                    ptr[strlen(ptr) - 1] = '\0';
                else{
                    char* ptr2 = strtok(NULL, "\"");
                    strcat(ptr, ptr2);
                }
            }

            else if(ptr[0] == '\''){            // 작은 따옴표로 입력된 argument  처리
                ptr = ptr + 1;
                if(strstr(ptr, "'") != NULL)
                    ptr[strlen(ptr) - 1] = '\0';
                else{
                    char* ptr2 = strtok(NULL, "'");
                    strcat(ptr, ptr2);
                }
            }

            if(ptr[0] == '$')
                cmdline_argv[i][argc++] = getenv(ptr + 1);
            else
                cmdline_argv[i][argc++] = ptr;
            ptr = strtok(NULL, " ");
        }

        if(argc == 0)
            return 1;

        cmdline_argv[i][argc] = NULL;           // 마지막 argument 뒤에 NULL
    }

    return bg;					// bg process: return 1, fg process: return 0
}

void myshell_execute(int bg, char* cmdline_argv[MAXPIPE][MAXARGS], char* input){
    pid_t process_id[MAXPIPE];
    int fd[MAXPIPE][2];
    int background = bg;

    for(int i = 0; i < pipeline; i++){
	if(!builtin_command(cmdline_argv[i])){    			//built-in-command check
	    if(pipe(fd[i]) == -1)
		unix_error("pipe error\n");
	    
	    Sigprocmask(SIG_BLOCK, &mask, &prev);			//Block SIGCLHD signal	    	    

	    if((process_id[i] = Fork()) == 0){				//child process
		setpgid(0, 0);						//생성된 process의 group 자기 자신으로 setting
		pipelining(fd, i);					//파이프를 통한 입출력 연결
		Execve(cmdline_argv[i][0], cmdline_argv[i], environ);	//execute command
		exit(0);
     	    }
	    	
	    //parent

	    ///////////////////////////////////////////////
	    // JOB list에 1개의 job만 추가하면 SIGCHLD signal 처리가 어렵고
	    // JOB list에 모든 process를 추가하면 정상적으로 실행되지만 
	    // process 개수만큼 JOB list에 중복저장되는 문제가 있다
	    //if(i==0)
	    add_job(process_id[i], input, background, RUN);		//JOB list에 모든  process 추가
	    ///////////////////////////////////////////////

	    close(fd[i][1]);
	    if(!background){						//fg parent process
		fg_run = 1;
		while(fg_run)						//wait for child process( exit(0), SIGINT, SIGTSTP )
		    Sigsuspend(&prev);
	    }

	    else							//bg parent process
		printf("[%d] %d\n", job_no, process_id[i]);		//not wait
	    
	    Sigprocmask(SIG_SETMASK, &prev, NULL);		
	}
    }
}

int builtin_command(char** cmdline_argv){
    if(!strcmp(cmdline_argv[0], "exit"))	//command "exit"
	exit(0);				//myshell 종료

    //built-in-command라면, 실행 후 return 1

    if(!strcmp(cmdline_argv[0], "cd")){		//command "cd ~"
	if(chdir(cmdline_argv[1]) == -1)	//directory 변경
	    printf("working directory change error\n");		
	return 1;
    }

    if(!strcmp(cmdline_argv[0], "jobs")){	//command "jobs ~"
	cmd_jobs();				//job list 출력
	return 1;
    }

    if(!strcmp(cmdline_argv[0], "fg")){		//command "fg %num"
	cmd_fg(cmdline_argv[1]);		//job_id == num 인 process를 fg로 실행
	return 1;
    }

    if(!strcmp(cmdline_argv[0], "bg")){		//command "bg %num"
	cmd_bg(cmdline_argv[1]);		//job_id == num 인 process를 bg로 실행
	return 1;
    }

    if(!strcmp(cmdline_argv[0], "kill")){	//command "kill %num"
	cmd_kill(cmdline_argv[1]);		//job_id == num 인 process 종료
	return 1;
    }

    //built-in-command가 아니라면 return 0
    return 0;
}

void pipelining(int fd[MAXPIPE][2], int i)
{
    if(pipeline <= 1)				// 단일 명령어 일 경우 pipe 처리 x       
	return;
    
    if(i == 0){                 		//첫번째 process
    	close(fd[i][0]);
      	dup2(fd[i][1], 1);    			//출력을 다음 process의 입력으로 연결
    	close(fd[i][1]);
    }
    else if(i < pipeline - 1){  		//중간 process
     	dup2(fd[i-1][0], 0);    		//입력을 이전 process로부터 받고
      	dup2(fd[i][1], 1);      		//출력을 다음 process의 입력으로 연결
      	close(fd[i-1][0]);
   	close(fd[i][1]);
    }
    else{                       		//마지막 process
        dup2(fd[i-1][0], 0);    		//입력을 이전 process로부터 받음
        close(fd[i-1][0]);
        close(fd[i-1][1]);
    }
}

void sigchld_handler(int sig){		//SIGCHLD signal handler
    pid_t pid;				//SIGCHLD signal 보낸 process의 pid
    int status;
    while((pid = Waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
	int suc = 0;
	JOBS* ptr = head;
	while(ptr){			//process_id가 pid 인 job 찾기
	    if(ptr->pid == pid){
		suc = 1;
		break;
	    }
	    ptr = ptr->link;
	}

	if(!suc)
	    continue;
	
	if(!ptr->bg)
	    fg_run = 0;			// fg process 종료된 경우 부모 process 대기 종료

	if(WIFEXITED(status)){  	// process 정상 종료 signal
	    ptr->state = DONE;
	    if(!ptr->bg)	
		delete_job(pid);	// fg process일 경우 job list에서 제거, bg process는 print_end_bg()함수에서 처리
	}

        else if(WIFSIGNALED(status)){   // SIGINT signal
	    ptr->state = TERMINATE;
	    delete_job(pid);
        }

	else if(WIFSTOPPED(status)){	// SIGTSTP signal
	    ptr->state = SUSPEND;	// state를 suspended로 변경
	    ptr->bg = 1;		// process를 bg process로 전환
	}
    }
}

void sigint_handler(int sig){
    printf("\n");
    JOBS* ptr = head;
    while(ptr){
	if(!ptr->bg && ptr->state == RUN){ // fg process에 SIGINT siganl
	    Kill(ptr->pid, SIGINT);
	    return;    
	}
	ptr = ptr->link;
    }
}

void sigtstp_handler(int sig){
    printf("\n");
    JOBS* ptr = head;
    while(ptr){
	if(!ptr->bg && ptr->state == RUN){ // fg process에 SIGTSTP signal
	    printf("[%d] Suspended %s\n", ptr->job_id, ptr->command);
	    Kill(ptr->pid, SIGTSTP);
	    return;
	}
	ptr = ptr->link;
    }
}

void cmd_jobs(){
   JOBS* ptr = head;

   while(ptr){	// JOBS list에서 실행중이거나 중단된 process 출력
	if(ptr->state == SUSPEND || ptr->state == RUN){
	    printf("[%d] ", ptr->job_id);
	    switch(ptr->state){
	        case SUSPEND:
		    printf("Suspended ");
		    break;
	        case RUN:
		    printf("Running ");
		    break;
	        default:
		    break;
	    }
	    printf("%s\n", ptr->command);
	}
	ptr = ptr->link;
   }
}

void cmd_fg(char* percentjob){
    int job_id;
    int suc = 0;

    if(percentjob == NULL || percentjob[0] != '%'){
	printf("You should input correct job spec\n");
	return;
    }    

    job_id = atoi(&percentjob[1]);	//fg로 실행시킬 job_id
    JOBS* ptr = head;

    while(ptr){
	if(ptr->job_id == job_id){
	    suc = 1;
	    break;
	}
	ptr = ptr->link;
    }

    if(!suc)				//job_id가 존재하지 않은 경우
	printf("No Such Job\n");

    else{				//존재할 경우
	Sigprocmask(SIG_BLOCK, &mask, &prev);
	ptr->bg = 0;
	ptr->state = RUN;		//해당 job의 process를 foreground로 실행
	printf("[%d] Running %s\n", job_id, ptr->command);
	fg_run = 1;
	Kill(ptr->pid, SIGCONT);	//send SIGCONT signal
	while(fg_run)			//wait for fg process
	    Sigsuspend(&prev);
	Sigprocmask(SIG_SETMASK, &prev, NULL);
    }
}

void cmd_bg(char* percentjob){
    int job_id;
    int suc = 0;

    if(percentjob == NULL || percentjob[0] != '%'){
        printf("You should input correct job spec\n");
        return;
    }

    job_id = atoi(&percentjob[1]);	//bg로 실행시킬 job_id
    JOBS* ptr = head;

    while(ptr){
        if(ptr->job_id == job_id){
            suc = 1;
            break;
        }
	ptr = ptr->link;
    }

    if(!suc)				//job_id가 존재하지 않은 경우
        printf("No Such Job\n");

    else{				//존재할 경우
	ptr->bg = 1;
	ptr->state = RUN;		//해당 job의 process를 background로 실행
	printf("[%d] Running %s\n", job_id, ptr->command);
	Kill(ptr->pid, SIGCONT);	//send SIGCONT signal
    }
}

void cmd_kill(char* percentjob){
    int job_id;
    int suc = 0;

    if(percentjob == NULL || percentjob[0] != '%'){
	printf("You should input correct job spec\n");
	return;
    }

    job_id = atoi(&percentjob[1]);	//kill을 할 job_id
    JOBS* ptr = head;
    while(ptr){
	if(ptr->job_id == job_id){
	    suc = 1;
	    break;
	}
	ptr = ptr->link;
    }

    if(!suc)				//job_id가 존재하지 않은 경우
	printf("No Such Job\n");

    else{				//존재할 경우
	ptr->state = TERMINATE;		//해당 job의 process를 terminate로 변경하고	
	delete_job(ptr->pid);		//job list에서 삭제
	Kill(ptr->pid, SIGKILL);	//process 종료
    }
}

void init_job(){
    head = NULL;
    tail = NULL;
    job_no = 0;
}

void add_job(pid_t process_id, char* input, int bg, int state){
    JOBS* new = (JOBS*)malloc(sizeof(JOBS));
    
    new->pid = process_id;
    strcpy(new->command, input);
    new->bg = bg;
    new->job_id = ++job_no;
    new->state = state;
    
    new->link = NULL;
    if(head == NULL){			//JOB list가 비어있는 경우
	head = new;
	tail = new;
    }
    else{				//JOB list가 비어있지 않은 경우 list의 끝에 새로운 job 추가
	tail->link = new;
	tail = new;
    }
}

void delete_job(pid_t process_id){
    JOBS* prev = head;
    JOBS* cur;
    int suc = 0;

    if(prev->pid == process_id){// 제거하려는 job이 head인 경우
	if(head-> link == NULL){	// JOB list에 1개의 job만 존재하는 경우
	    head = NULL;
	    tail = NULL;
	}
	else				// JOB list에 2개 이상의 job이 존재하는 경우
	    head = head->link;		// head는 2번째 job을 가르킴
	
	free(prev);			// JOB list에서 제거
    }

    else{			//제거하려는 job이 head가 아닌 경우
	cur = prev->link;
	while(cur){			// process_id와 같은 pid를 같은 process 찾기
	    if(cur->pid == process_id){
		suc = 1;
		break;
	    }
	    prev = prev->link;
	    cur = cur->link;
	}

	if(!suc){
	    printf("pid: %d not exits in job list\n", process_id);
	    return;
	}

	if(cur->link == NULL){	//제거하려는 job이 tail인 경우
	    tail = prev;	//tail은 마지막에서 2번째 job 가르킴
	}
	prev->link = cur->link;	
	free(cur);		//JOB list에서 제거
    }
}

void print_end_bg(){
    JOBS* ptr = head;
    int process_id;

    while(ptr){	//JOB list에서 종료된 job 출력 후 JOB list에서 삭제
	if(ptr->state == DONE){
	    printf("[%d] DONE %s\n", ptr->job_id, ptr->command);
	    process_id = ptr->pid;
	    ptr = ptr->link;
	    delete_job(process_id);	    
	}
	else if(ptr->state == TERMINATE){
            printf("[%d] TERMINATED %s\n", ptr->job_id, ptr->command);
            process_id = ptr->pid;
            ptr = ptr->link;
            delete_job(process_id);
	}
	else
	    ptr = ptr->link;
    }
}
