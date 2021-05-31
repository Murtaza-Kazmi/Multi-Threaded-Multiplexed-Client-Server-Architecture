#include <unistd.h>
#include <stdlib.h>
#include <strings.h>

//for socket
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>

//for strlen:
#include <cstring>

//for sprintf
#include <string>

//for signals
#include <stdlib.h>
#include <stdio.h>
//+ main API header file
#include <signal.h>

//therad
#include <pthread.h>

void *read_function( void *ptr );
void *write_function( void *ptr );
void handler (int signo);

int size = 1024;


int main(int argc, char *argv[]){

	int sock;
	struct sockaddr_in server;
	struct hostent *hp;
	char buf[1024];

	/* Create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening stream socket");
		exit(1);
	}
	/* Connect socket using name specified by command line. */
	server.sin_family = AF_INET;

	hp = gethostbyname(argv[1]);
	if (hp == 0) {
		fprintf(stderr, "%s: unknown host\n", argv[1]);
		exit(2);
	}
	bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
	server.sin_port = htons(atoi(argv[2]));

	
	if(connect(sock,(struct sockaddr *) &server,sizeof(server)) < 0) {
		perror("connecting stream socket");
		exit(1);
	}
	else{
		char buffer[30];
		sprintf(buffer, "Connection established.");
		puts(buffer);
	}
	
	pthread_t thread1, thread2;
    const int *s = &sock;
    int  iret1, iret2;

	pthread_attr_t attr;
    int initRes = pthread_attr_init(&attr);
	int setdetachRes = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    iret1 = pthread_create( &thread1, NULL, read_function, (void*) s);

    if(iret1){
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
        exit(EXIT_FAILURE);
    }

    iret2 = pthread_create( &thread2, NULL, write_function, (void*) s);

    if(iret2){
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
        exit(EXIT_FAILURE);
    }

    /* Wait till threads are complete before main continues. Unless we  */
    /* wait we run the risk of executing an exit which will terminate   */
    /* the process and all threads before the threads have completed.   */

	while(true){
		continue;
	}
	
}


void *read_function( void *s ){

	int *sockp = (int *) s;
	int sock = *sockp;

	char buff[1024];

	while(true){
		for(int i = 0; i < 1024; i++){
			buff[i] = '\0';
		}

		int readResponseResult = read(sock, buff, sizeof(buff)-1);

		if(readResponseResult == 0){
			sprintf(buff, "Connection broke.");
			puts(buff);
			close(sock);
			exit(0);
		}
		write(STDOUT_FILENO, buff, strlen(buff));
		char c = '\n';
		write(STDOUT_FILENO, &c, 1);
	}
}

void *write_function(void *s){
	int *sockp = (int *) s;

	int sock = *sockp;
	while(true){

		char buff[size];

		for(int i = 0; i < size; i++){
			buff[i] = '\0';
		}

		//reads until enter, so no garbage value, truncates until character
		int noOfBytesRead = read(0, buff, size);
		
		if(buff[0] == '\n'){
			sprintf(buff, "Invalid number of arguments. Please enter again.");
			puts(buff);
			continue;
		}

		if(noOfBytesRead == -1){
			perror(("Error in read."));
		}

		if(strcmp(buff, "exit\n") == 0){
			exit(1);
		}
		
		// send input to server (sends the bytes inputted)
		int writeResult = write(sock, buff, noOfBytesRead-1);
		if(writeResult < 0){
			perror("Error in writing from server to client.");
			exit(1);
		}
	}
}
