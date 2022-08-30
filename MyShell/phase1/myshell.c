/*
 * CSE4100: System Programming
 *
 * 20171683 임종환
 * 
 * Project1: myshell-phase1
 */
#include "myshell.h"

extern char** environ;
int pid;

int myshell_parseinput(char* input, char* cmdline_argv[MAXARGS]);	// command 입력 parsing
void myshell_execute(int bg, char* cmdline_argv[MAXARGS], char* input);	// 입력받은 command 실행
int builtin_command(char** cmdline_argv);				// builtin command check
void sigint_handler(int sig);						// SIGINT signal handler

int main()
{
    char input[MAXLINE];
    char* cmdline_argv[MAXARGS];
    int bg;

    Signal(SIGTSTP, SIG_IGN);				// SIGTSTP signal 무시

    do{
	// Reading
	Signal(SIGINT, SIG_IGN);			// 쉘에 명령어를 입력받는 동안 SIGINT signal 무시
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

int myshell_parseinput(char* input, char* cmdline_argv[MAXARGS])
{
    char buf[MAXLINE];
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

    ptr = strtok(buf, " ");		
    argc = 0;
    while(ptr != NULL){				// command의 argument parsing
	if(ptr[0] == '"'){			// 큰 따옴표로 입력된 argument 처리
	    ptr = ptr + 1;
	    if(strstr(ptr, "\"") != NULL)
		ptr[strlen(ptr) - 1] = '\0';
	    else{
		char* ptr2 = strtok(NULL, "\"");
		strcat(ptr, ptr2);
	    }
	}

	else if(ptr[0] == '\''){		// 작은 따옴표로 입력된 argument  처리
	    ptr = ptr + 1;
	    if(strstr(ptr, "'") != NULL)
		ptr[strlen(ptr) - 1] = '\0';
	    else{
		char* ptr2 = strtok(NULL, "'");
		strcat(ptr, ptr2);
	    }
	}

	if(ptr[0] == '$')			// 환경 변수 처리
	    cmdline_argv[argc++] = getenv(ptr + 1);
	else
            cmdline_argv[argc++] = ptr;
        ptr = strtok(NULL, " ");
    }

    if(argc == 0)			
        return 1;

    cmdline_argv[argc] = NULL;			// 마지막 argument 뒤에 NULL

    return bg;					// bg process: return 1, fg process: return 0
}

void myshell_execute(int bg, char* cmdline_argv[MAXARGS], char* input){
    pid_t process_id;

    if(!builtin_command(cmdline_argv)){    			//built-in-command check

        if((process_id = Fork()) == 0){  			//child process
            Execve(cmdline_argv[0], cmdline_argv, environ);	//excute command
            exit(0);
        }

        if(!bg){						//fg parent process
	    pid = process_id;					//child process의 pid
            Waitpid(process_id, NULL, 0);			//Waiting for child process and reap it
        }

        else{							//bg parent process
       	    printf("%d %s\n", process_id, input);		//not wait
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

void sigint_handler(int sig){
    Kill(pid, SIGINT);	// pid: 명령어 실행중인 child process의 pid
}
