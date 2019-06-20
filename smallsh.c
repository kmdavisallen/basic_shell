/************************************************************
**Author: Kevin Allen
**Date 7/29/18
**Description:  CS344 program 3: basic shell program
************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int checkBg = 0;
int childExitStatus = 0;	//store exit status of last forground process

void sigintHandle(int sig) {		//handler function for SIGINT
	waitpid(-1, &childExitStatus, 0);
	exitStatus(childExitStatus);
}

void sigstpHandle(int sig) {		//handler function for SIGTSTP
	checkBg++;
	char* newLine = "\n";
	write(STDOUT_FILENO, newLine, 1);
	if (checkBg %2 == 1) {
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 49);
	}
	else if (checkBg % 2 == 0) {
		char* message = "Exiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 29);
	}
}

int exitStatus(int childExit) {			//function to get exit status of child
	if (WIFEXITED(childExit)) {
		int status = WEXITSTATUS(childExit);
		printf("exit value %d\n", status);
		fflush(stdout);
	}
	else {
		int status = WTERMSIG(childExit);
		printf("terminated by signal number %d\n", status);
		fflush(stdout);
	}
}		
							

int main() {
	
	//getline variables
	int numInputChar = 0;
	size_t buffersize = 0;
	char* lineIn = NULL;

	int bgPids[50];  //store background processes
	int pidCount = 0;

	int bgExit = 0;		//store background process status
	char* args[512];  //array to hold arguments, max of 512
	
	while (1) {
		
		//handler for SIGINT
		struct sigaction SIGINT_action = { 0 };
		SIGINT_action.sa_handler = sigintHandle;
		sigfillset(&SIGINT_action.sa_mask);
		SIGINT_action.sa_flags = 0;
		sigaction(SIGINT, &SIGINT_action, NULL);

		//handler for SIGTSTP
		struct sigaction SIGTSTP_bgAction = { 0 };
		SIGTSTP_bgAction.sa_handler = sigstpHandle;
		sigfillset(&SIGTSTP_bgAction.sa_mask);
		SIGTSTP_bgAction.sa_flags = 0;
		sigaction(SIGTSTP, &SIGTSTP_bgAction, NULL);
		
		while (1) {
			printf(":");
			numInputChar = getline(&lineIn, &buffersize, stdin);	//get input, error handling adapted from lecture notes
			if (numInputChar == -1) {
				clearerr(stdin);
			}
			else {
				char* pidEx = strstr(lineIn, "$$");	//check for locations needing process id
				if (pidEx != NULL) {
					int curPid = getpid();		//get pid
					char cPid[10];
					sprintf(cPid, "%d", curPid);	//convert int to string
					strcpy(pidEx, cPid);	//replace $$ with pid
				}
				break;
			}
		}
		char* temp = strtok(lineIn, " ,\n");		//get first argument
		if (temp != NULL) {							//make sure there is input to test against
			if (strcmp(temp, "status") == 0) {
				//get the exit status of last foreground child
				exitStatus(childExitStatus);
			}
			else if (strcmp(temp, "exit") == 0) {	//exit built in
				exit(0);
			}
			else if (strcmp(temp, "cd") == 0) {		//cd built in
				temp = strtok(NULL, " \n");
				if (temp == NULL) {					//return to home if no arg
					int bigFail = chdir(getenv("HOME"));
					if (bigFail < 0) {
						printf("I'm sorry, but they do say you can never go home again");
					}
				}
				else {
					int dirFail = chdir(temp);		//go to specified path
					if (dirFail < 0) {
						printf("error, dir not found");
					}
				}
			}
			else if (strncmp(temp, "#", 1) == 0) {
				//comment line do nothing
			}
			else if (temp != NULL) {
				int isBackground = 0;		//flag if process is to be fore/back ground
				if (checkBg % 2 == 0) {		//check if foreground only mode enabled
					if ((lineIn[numInputChar - 2]) == '&') {
						isBackground = 1;
					}
				}
				int i = 0;   //index for args array
				pid_t spawnpid = fork();
				if (spawnpid == -1) {
					printf("error");
					exit(1);
				}
				else if (spawnpid == 0) {
					int sourceFD = -1;
					int targetFD = -1;
					while (temp != NULL) {
						if (strcmp(temp, ">") == 0) {		//redirect stdout
							temp = strtok(NULL, " ,\n");
							targetFD = open(temp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
							dup2(targetFD, 1);
							temp = strtok(NULL, " ,\n");
						}
						else if (strcmp(temp, "<") == 0) {	//redirect input
							temp = strtok(NULL, " ,\n");
							sourceFD = open(temp, O_RDONLY);
							if (sourceFD < 0) {
								printf("error, file not found");
								exit(1);
							}
							else {
								dup2(sourceFD, 0);
								temp = strtok(NULL, " ,\n");
							}
						}
						else {					//load as argument
							args[i] = temp;
							temp = strtok(NULL, " ,\n");
							i++;
						}
					}

					if (strcmp(args[i - 1], "&") == 0 && (checkBg % 2 == 0)) {
						//background process

						printf("background pid is %d\n", getpid());
						fflush(stdout);
						if (sourceFD == -1) {			//if sourceFD -1 then redirect to /dev/null
							sourceFD = open("/dev/null", O_RDONLY);
							dup2(sourceFD, 0);
						}
						if (targetFD == -1) {			//if target not specified then redirect to dev/null
							targetFD = open("/dev/null", O_WRONLY);
							dup2(targetFD, 1);
						}
						//SIG for interupt
						struct sigaction SIGINT_bgAction = { 0 };
						SIGINT_bgAction.sa_handler = SIG_IGN;		//ignore interrupt if background process
						sigfillset(&SIGINT_bgAction.sa_mask);
						SIGINT_bgAction.sa_flags = 0;
						sigaction(SIGINT, &SIGINT_bgAction, NULL);

						//SIG handle for stp
						struct sigaction SIGTSTP_bgAction = { 0 };
						SIGTSTP_bgAction.sa_handler = SIG_IGN;		//ignore signal
						sigfillset(&SIGTSTP_bgAction.sa_mask);
						SIGTSTP_bgAction.sa_flags = 0;
						sigaction(SIGTSTP, &SIGTSTP_bgAction, NULL);

						args[i - 1] = NULL;	//dont pass & into execvp
					}
					if (args[i - 1] != NULL && strcmp(args[i - 1], "&") == 0) {  //strip & off if in foreground only mode
						args[i - 1] = NULL;
					}
					args[i] = NULL;		//terminate array with NULL
					int fail = execvp(*args, args);
					printf("program not found\n");
					fflush(stdout);
					exit(1);
				}
				if (isBackground == 1) {
					bgPids[pidCount] = spawnpid;
					pidCount++;
				}
				if (isBackground == 0) {	//foreground process
					waitpid(spawnpid, &childExitStatus, 0);
				}
			}
		}
		
		//parent cleanup of background processes
		int j = 0;
		for (j = 0; j < pidCount; j++) {
			pid_t childpid = waitpid(bgPids[j], &bgExit, WNOHANG);
			if (childpid > 0) {
				printf("background pid %d is done: ", childpid);
				exitStatus(bgExit);
				fflush(stdout);
			}
		}
	
	}
	return 0;
}
