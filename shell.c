#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>

#define COMMAND_NUM 10
#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

int numCommandsExecuted;
int end = 0;
int curr = -1;
char *commandsArr[COMMAND_NUM][MAX_LINE/2+1];
int background; /* equals 1 if a command is followed by '&' */
int backgroundArr[COMMAND_NUM];
int historyFile, totalCommands;

void writeSystemCallString(int fd, char buffer[], int length)
{
	int writeReturn;
	writeReturn = write(fd, buffer, length);
	if(writeReturn == -1){
		/* Something went wrong with the write command */
		perror("error writing to console the command");
		close(historyFile);
		exit(-1);
	}
}

void clearBuffer(char *buffer)
{
	int j;
	for (j = 0; buffer[j] != '\0'; j++) {
		buffer[j] = '\0';
	}
}

void readWrappedCall(int fd, char *buffer, int numBytes)
{
	int ret = read(fd, buffer, numBytes);
	if(ret == -1) {
		printf("There was an error in reading the file, exiting now.\n");
		close(historyFile);
		exit(1);
	}
}

void lseekWrappedCall(int fd, off_t offset, int whence)
{
	off_t ret = lseek(fd, offset, whence);
	if(ret == -1) {
		printf("There was an error in the lseek command, exiting now.\n");
		close(historyFile);
		exit(1);
	}
}

int findTotalCommands()
{
	int total, size;
	char num[17]; /* definetly can't store more than a 16 bit integer */
	char test[2] = "0";

	lseekWrappedCall(historyFile, 0, SEEK_SET);

	readWrappedCall(historyFile, num, 1);

	if(num[0] == 0) {
		return 0;
	}
	
	lseekWrappedCall(historyFile, 0, SEEK_END);
	num[0] = '0';

	size = 0;
	while(num[0] != 10){
		lseekWrappedCall(historyFile, -1 * size, SEEK_CUR);
		readWrappedCall(historyFile, num, size);
		size++;
	}
	total = atoi(num + 1);

	return total;
}

int numBytes(int pos)
{
	int j, length, size = 0;

	for(j = 0; commandsArr[pos][j] != NULL; j++) {
		length = strlen(commandsArr[pos][j]);
		size += length + (int) ceil(log10(((double)length))) + 1 + 1;
	}
	if (backgroundArr[pos] == 1) {
		size += 3; /* for the number, |, and ampersand */
	}
	return size;
}

void historyBufferDump()
{
	int i, j, pos, k;
	char newLine[] = "\n";
	char toWrite[MAX_LINE];
	char ampersand[] = "&";
	char pipe[] = "|";
	char num[MAX_LINE];
	char space[] = " ";
	int numLength, commandNum;

	if(numCommandsExecuted > totalCommands) {
		/* only execute if there are new commands */
		
		/* we need the starting position in the array based on the number of commands read in and the 
			number of commands executed during the program
			if numCommandsExecuted - totalCommands >= 10 then pos = end I think 
			else we have three cases
			1 -> totalCommands was 0 and finding the new place is easy
			2 -> totalCommands was > 0 and we have to find the oldest command that was executed during the program */

		
		if(numCommandsExecuted - totalCommands >= 10) {
			commandNum = numCommandsExecuted - totalCommands;
			pos = end;
		} else {
			if(totalCommands == 0) {
				commandNum = numCommandsExecuted - totalCommands;
				pos = 0;
			} else {
				commandNum = numCommandsExecuted - totalCommands;
				pos = COMMAND_NUM - (numCommandsExecuted - totalCommands) + 1 + curr;
				pos %= COMMAND_NUM;
			}
		}



		if(totalCommands > 0) {
			/* tack the one at the end because that will get us before the actual number and overwrite that number */
			if(totalCommands > 1) {
				lseekWrappedCall(historyFile, -1 * ((int)ceil(log10((double)totalCommands))), SEEK_END);
			} else {
				/* we know it is 1 so just put in a negative 1 
					only doing this because log base anything of 1 is zero */
				lseekWrappedCall(historyFile, -1, SEEK_END);
			}
		}

		/* prints the commands to the file in the following format: #bytes|[the array of strings with spaces in between each element]*/
		for(i = 0; i < commandNum && i < COMMAND_NUM; i++, pos++) {
			pos %= COMMAND_NUM;

			totalCommands++;
			
			numLength = sprintf(num, "%d", numBytes(pos)); /* prints the total number of bytes in the line about to be printed */
			
			writeSystemCallString(historyFile, num, numLength);

			clearBuffer(num);

			writeSystemCallString(historyFile, pipe, strlen(pipe));


			for(j = 0; commandsArr[pos][j] != NULL; j++) {
				/* This prints the num bytes in the string in the current position */
				numLength = sprintf(num, "%d", strlen(commandsArr[pos][j]));
			
				writeSystemCallString(historyFile, num, numLength);

				clearBuffer(num);
				writeSystemCallString(historyFile, pipe, strlen(pipe)); /* separates the number from the strings */

				/* Prints the actual string */
				writeSystemCallString(historyFile, commandsArr[pos][j], strlen(commandsArr[pos][j]));
				writeSystemCallString(historyFile, space, strlen(space));
			}
			if(backgroundArr[pos] == 1) {
				numLength = sprintf(num, "%d", 1);
			
				writeSystemCallString(historyFile, num, numLength);

				clearBuffer(num);

				writeSystemCallString(historyFile, pipe, strlen(pipe)); /* separates the number from the strings */
				writeSystemCallString(historyFile, ampersand, strlen(ampersand));
			}
			writeSystemCallString(historyFile, newLine, strlen(newLine));
		}

		numLength = sprintf(num, "%d", totalCommands);
			
		writeSystemCallString(historyFile, num, numLength);

		fflush(0);
	}
}

int readNum(char *num) 
{
	int counter;
	
	/* the bytes for each line definitly won't exceed 10 digits, right? */
	for(counter = 0; counter < 10; counter++) {
		readWrappedCall(historyFile, num + counter, 1);
		if(num[counter] == '|'){
			break;
		}
	}
	num[counter] = '\0';
	return atoi(num);
}

void readInHistoryFile()
{
	/* We have the open file descriptor and we have the amount of commands in it so we just keep reading until we have <= 10 commands left */

	char num[MAX_LINE];
	int j, i = totalCommands;
	int byteNum, totalBytes, counter = 0;

	lseekWrappedCall(historyFile, 0, SEEK_SET);

	/* This while loop skips through the file line by line */
	while(i > 10) {
		totalBytes = readNum(num);
		clearBuffer(num);

		/* printf("%d\n", totalBytes); -> shows how many bytes each line is */
		lseekWrappedCall(historyFile, totalBytes, SEEK_CUR); /* jump the exact number bytes in the line */
		i--;
	}

	/* Goes through the last 10 commands in the file */
	while(i > 0) {
		/* gets the total number of bytes in the line */
		totalBytes = readNum(num);
		clearBuffer(num);

		j = 0;
		while(totalBytes > 0){
			/* basically we read in num bytes for the string, allocate that amount of bytes, then give it to the commandsArr */
			byteNum = readNum(num);
			commandsArr[end][j] = calloc(byteNum + 1, sizeof(char));
			if(commandsArr[end][j] == NULL) {
				printf("Couldn't allocated enough memory, exiting now.\n");
				close(historyFile);
				exit(1);
			}

			/* read in actual string */
			readWrappedCall(historyFile, commandsArr[end][j], byteNum);

			/* For the ampersand case */
			if(byteNum == 1 && commandsArr[end][j][0] == '&') {
				backgroundArr[end] = 1;
				free(commandsArr[end][j]);
				commandsArr[end][j] = NULL;
			}

			/* The block that we are consuming is -> #|string  so this just calculating that length */
			byteNum += 2 + (int)ceil(log10((double)byteNum)); /* everything we are consuming in this go */
			totalBytes -= byteNum;
			j++;
		}

		/* updates the pointers to the circular queue */
		end += 1;
		curr += 1;
		end %= COMMAND_NUM;
		curr %= COMMAND_NUM;
		i--;
	}
}

/* The signal handler function */
void handle_SIGINT()
{
	int i, j;
	int commandNum = 0;
	int pos = 0;
	int writeReturn;
	char newLine[] = "\n";
	char toWrite[MAX_LINE];
	char initial[] = "The last ten commands from least to most recent:\n";
	char command[] = "Command ";
	char colon[] = ": ";
	char space[] = " ";
	char ampersand[] = "&";
	char num[MAX_LINE];
	int numLength;
	
	if(numCommandsExecuted > COMMAND_NUM) {
		commandNum = numCommandsExecuted - COMMAND_NUM;
		pos = end;
	}

	/* wrapped system calls */
	writeSystemCallString(STDOUT_FILENO, newLine, strlen(newLine));
	writeSystemCallString(STDOUT_FILENO, initial, strlen(initial));

	for(i = 0; i < numCommandsExecuted && i < COMMAND_NUM; i++, pos++) {
		pos %= COMMAND_NUM;
		writeSystemCallString(STDOUT_FILENO, command, strlen(command));

		numLength = sprintf(num, "%d", commandNum + 1 + i);
		
		writeSystemCallString(STDOUT_FILENO, num, numLength);
		writeSystemCallString(STDOUT_FILENO, colon, strlen(colon));
		for(j = 0; commandsArr[pos][j] != NULL; j++) {
			writeSystemCallString(STDOUT_FILENO, commandsArr[pos][j], strlen(commandsArr[pos][j]));
			writeSystemCallString(STDOUT_FILENO, space, strlen(space));
		}
		if(backgroundArr[pos] == 1) {
			writeSystemCallString(STDOUT_FILENO, ampersand, strlen(ampersand));
		}
		writeSystemCallString(STDOUT_FILENO, newLine, strlen(newLine));
	}

	background = 0;
	printf("COMMAND->");
	fflush(0);
}

int rCommand(char firstChar)
{
	int index = 0;
	int i;
	int found = 0;
	int retValue = -1;

	/* If they just do r */
	if(firstChar == '\0') {
		return curr;
	}

	if(numCommandsExecuted > COMMAND_NUM - 1) {
		index = end;
	}

	/* If there is r followed by an input */
	for(i = 0; i < numCommandsExecuted && i < COMMAND_NUM; i++, index++) {
		index %= COMMAND_NUM;
		if(commandsArr[index][0][0] == firstChar) {
			retValue = index;
		}
	}
	return retValue; /* didn't find the index of the command that starts with the character */
}

void freeCommandArr() {
	int i;
	/* First free the memory */
	for(i = 0; commandsArr[end][i] != NULL; i++) {
		free(commandsArr[end][i]);
		commandsArr[end][i] = NULL;
	}
}

void copyArgsToCommandsArr(char *args[])
{
	int i;

	freeCommandArr();
	/* Allocate the new memory for the array to store */
	for(i = 0; args[i] != NULL; i++) {
		commandsArr[end][i] = calloc(strlen(args[i]), sizeof(char));
		if(commandsArr[end][i] == NULL) {
			/* failed allocation */
			/* honestly lets just exit here */
			perror("Failed to allocate memory\n. Exiting program now.\n");
			close(historyFile);
			exit(-1);
		} else {
			strcpy(commandsArr[end][i], args[i]);
		}
	}
}

/**
 * setup() reads in the next command line, separating it into distinct tokens
 * using whitespace as delimiters. setup() sets the args parameter as a 
 * null-terminated string.
 */

void setup(char inputBuffer[], char *args[],int *background)
{
	int length, /* # of characters in the command line */
	i,      /* loop index for accessing inputBuffer array */
	start,  /* index where beginning of next command parameter is */
	ct;     /* index of where to place the next parameter into args[] */

	ct = 0;

	/* read what the user enters on the command line */
	length = read(STDIN_FILENO, inputBuffer, MAX_LINE);  

	start = -1;
	if (length == 0) {
		historyBufferDump();
		close(historyFile);
		exit(0);            /* ^d was entered, end of user command stream */
	}
	if (length < 0){
		perror("error reading the command");
		close(historyFile);
		exit(-1);           /* terminate with error code of -1 */
	}


	/* examine every character in the inputBuffer */
	for (i = 0; i < length; i++) { 
		switch (inputBuffer[i]){
		case ' ':
		case '\t' :               /* argument separators */
			if(start != -1){
				args[ct] = &inputBuffer[start];    /* set up pointer */
				ct++;
			}
			inputBuffer[i] = '\0'; /* add a null char; make a C string */
			start = -1;
		break;

		case '\n':                 /* should be the final char examined */
			if (start != -1){
				args[ct] = &inputBuffer[start];     
				ct++;
			}
			inputBuffer[i] = '\0';
			args[ct] = NULL; /* no more arguments to this command */
		break;

		case '&':
			*background = 1;
			inputBuffer[i] = '\0';
		break;

		default :             /* some other character */
		if (start == -1)
			start = i;
		} 
	}    
	args[ct] = NULL; /* just in case the input line was > 80 */
} 

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	char *args[MAX_LINE/2+1];/* command line (of 80) has max of 40 arguments */
	pid_t x, y;
	int returnCode;
	int execReturnCode;
	struct sigaction handler;
	int i, index;

	/* symbolic links -> you can reference folders that may not be on the same computer */
	/* dirent = directory entry */
	/* linuxcommand.org*/

	handler.sa_handler = handle_SIGINT;
	handler.sa_flags = SA_RESTART;
	sigaction(SIGINT, &handler, NULL);

	/* 0 = file, 6 = read and write for owner, 44 = read only for group and other */
	historyFile = open("rao.462_history.txt", O_RDWR);
	if(historyFile == -1) {
		historyFile = creat("rao.462_history.txt", 0644);
		totalCommands = 0;
		if (historyFile == -1) {
			printf("Unable to open the history file, exiting now.\n");
			exit(1);
		}
	} else {
		totalCommands = findTotalCommands();
		if(totalCommands > 0) {
			readInHistoryFile();
			numCommandsExecuted = totalCommands;
		}
	}

	while (1) {            /* Program terminates normally inside setup */
		background = 0;
		printf("COMMAND->");
		fflush(0);


		/* can add the index of the thingy here instead of args */
		setup(inputBuffer, args, &background);       /* get next command */

		if(args[0] != NULL) {
			if(args[0][0] == 'r' && args[0][1] == '\0') {
				if(args[1] == NULL) {
					index = rCommand('\0');
				} else {
					index = rCommand(args[1][0]);
				}

				if(index != -1 && index != end) {
					/* the command does in fact exist */
					copyArgsToCommandsArr(commandsArr[index]);
					background = backgroundArr[index];
				} else if (index == -1) {
					continue;
				} else {
					background = backgroundArr[index];
				}
				/* if the index == end, then we just execute the command again (lol) */
			} else {
				copyArgsToCommandsArr(args);
			}
		} else {
			continue;
		}


		x = fork(); /* forks the parent */
		if(x < 0){
			fprintf(stderr, "Fork failed");
			close(historyFile);
			exit(-1);
		} else if(x == 0) {
			execReturnCode = execvp(commandsArr[end][0], commandsArr[end]); /* We know the process is the child process so we execute the command */

			/* if there is a problem in exec, it is most likely that there is a problem with finding the file to execute
			because it most probably does not exist*/
			if (execReturnCode < 0) {
				printf("\nAn error occured while executing the command \"%s %s\". The command probably does not exist. \n", commandsArr[end][0], commandsArr[end][1]);
				exit(-1);
			}
		} else {

			/* check to see if it running concurrently or not */
			if(background == 0) {
				y = waitpid(x, &returnCode, 0);

				/* make sure the waitpid command executed correctly and that the returnCode is 0 indicating success
				or else it will say there was an error */
				if(y == -1 || returnCode != 0) {
					printf("\nAn error occured while executing the command \"%s %s\". \nPerhaps the command doesn't exist, or the arguements for the command are incorrect\n\n", commandsArr[end][0], commandsArr[end][1]);
					freeCommandArr();
					continue;
				}
			}

			/* this happens if background == 1 or the returnCode == 0 and y != -1 */
			backgroundArr[end] = background;
			end += 1;
			curr += 1;
			numCommandsExecuted += 1;
			end %= COMMAND_NUM;
			curr %= COMMAND_NUM;
		}
	}
}
