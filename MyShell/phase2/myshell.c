/*
 * CSE4100: System Programming
 *
 * 20171683 임종환
 *
 * Project1: myshell-phase2
 */
#include "myshell.h"

extern char** environ;
int pid;
int pipeline;	//pipe 개수

int myshell_parseinput(char* input, char* cmdline_argv[MAXPIPE][MAXARGS]);	// command 입력 parsing
void myshell_execute(int bg, char* cmdline_argv[MAXPIPE][MAXARGS], char* input);// 입력받은 command 실행
int builtin_command(char** cmdline_argv);					// builtin command check
void sigint_handler(int sig);
void pipelining(int fd[MAXPIPE][2], int i);					// pipe 연결

int main()
{
    char input[MAXLINE];
    char* cmdline_argv[MAXPIPE][MAXARGS];
    int bg;

    Signal(SIGTSTP, SIG_IGN);				// SIGTSTP signal 무시
    do{
	// Reading
	Signal(SIGINT, SIG_IGN);			// 쉘에 명령어를 입력받는 동안 SIGINT siganl 무시
	printf("CSE4100-SP-Proj1-myshell> ");
	fgets(input, MAXLINE, stdin);			// 명령어 입력
	if(!strcmp(input, "\n"))
	    continue;
	// Parsing
	bg = myshell_parseinput(input, cmdline_argv);	// 명령어 parsing
	// Execute
	Signal(SIGINT, sigint_handler);			// 명령어를 실행중일 땐 sigint_handler를 통해 SIGINT signal 처리
	myshell_execute(bg, cmdline_argv, input);	// 명령어 실행
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

	    if(ptr[0] == '$')			// 환경변수 처리
		cmdline_argv[i][argc++] = getenv(ptr + 1);
	    else
                cmdline_argv[i][argc++] = ptr;
            ptr = strtok(NULL, " ");
    	}

    	if(argc == 0)			
	    return 1;

        cmdline_argv[i][argc] = NULL;		// 마지막 argument 뒤에 NULL
    }

    return bg;					// bg process: return 1, fg process: return 0
}

void myshell_execute(int bg, char* cmdline_argv[MAXPIPE][MAXARGS], char* input){
    pid_t process_id[MAXPIPE];
    int fd[MAXPIPE][2];

    for(int i = 0; i < pipeline; i++){					//입력받은 command 개수만큼 반복
	if(!builtin_command(cmdline_argv[i])){		    		//built-in-command check
	    if(pipe(fd[i]) == -1)
		unix_error("pipe error\n");

            if((process_id[i] = Fork()) == 0){  			//child process
                pipelining(fd, i);					//pipe를 통한 입출력 연결
                Execve(cmdline_argv[i][0], cmdline_argv[i], environ);	//excute command
                exit(0);
            }

            if(!bg){							//fg parent process
		close(fd[i][1]);
		pid = process_id[i];					//child process의 pid
                Waitpid(process_id[i], NULL, 0);			//Waiting for child process and reap it
            }

            else{							//bg parent process
	   	printf("%d %s\n", process_id[i], input);		//not wait
	    }
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

    if(!strcmp(cmdline_argv[0], "jobs")){	//command "jobs"
						//phase3 구현
	return 1;
    }

    if(!strcmp(cmdline_argv[0], "fg")){		//command "fg"
						//phase3 구현
	return 1;
    }

    if(!strcmp(cmdline_argv[0], "bg")){		//command "bg"
						//phase3 구현
	return 1;
    }

    if(!strcmp(cmdline_argv[0], "kill")){	//command "kill"
						//phase3 구현

    }

    //built-in-command가 아니라면 return 0
    return 0;
}

void pipelining(int fd[MAXPIPE][2], int i)
{
    if(pipeline <= 1)				//단일 명령어일 경우 pipe 처리 x
        return;

    if(i == 0){                 	   	//첫번째 process
        close(fd[i][0]);
        dup2(fd[i][1], 1);		     	//출력을 다음 process의 입력으로 연결
        close(fd[i][1]);
    }
    else if(i < pipeline - 1){  	   	//중간 명령어
        dup2(fd[i-1][0], 0);		    	//입력을 이전 process로부터 받고
        dup2(fd[i][1], 1);		     	//출력을 다음 process의 입력으로 연결
        close(fd[i-1][0]);
        close(fd[i][1]);
    }
    else{                       	   	//마지막 명령어
        dup2(fd[i-1][0], 0);		    	//입력을 이전 process로부터 받음
        close(fd[i-1][0]);
        close(fd[i-1][1]);
    }
}

void sigint_handler(int sig){
    Kill(pid, SIGINT);	// pid: 명령어 실행중인 child process의 pid
}
