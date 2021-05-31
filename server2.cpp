 // preprocessor directives -------------------------------------------------------------------------------

//for sprintf
 #include <string>

//for strcmp, strtok
#include <cstring>

// for system calls: pipe(), exec()
#include <unistd.h>

//for close-on-exec: O_CLOEXEC (read man pipe2)      
#include <fcntl.h>              

//for time
#include <ctime>

//for kill()
#define SIGTERM 15

//for waitpid
#include <sys/types.h>
#include <sys/wait.h>

//for signals
#include <stdlib.h>
#include <stdio.h>
//+ main API header file
#include <signal.h>
 
//missing headers
#include <unistd.h>
#include <stdlib.h>


//socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#define TRUE 1

// for inet_aton(), inet_addr() and inet_ntoa()
#include<arpa/inet.h> 

// for multiplexed I/O
#include <poll.h>

using namespace std;

void handler (int signo);
void clientHandler(int sock, bool callAccept, int pid, int fdClientHandlerReads[2], int fdConnectionReads[2]);
void displayClientCommunicators();
void *client_handler_thread(void* obj);
void *serverInteractionThread(void *arg);
void *serverInteractionThreadReader(void *arg);


class Process {       
    public:            
		char name[100];
		int pid;
		char status[10];
		time_t startTime;
};

class Client {       
    public:           
		int pid;
		//check char*
		char* ip;
		int sockfd;
		int fdRead;
		int fdWrite;
		int port;
};

int noOfProcessesAllowed = 100;
Process processes[100];
Client clients[100];
int processIndex = 0;
int clientsIndex = 0;
int size = 1024;
int sock;
int indexOfAcceptedClients = -1;

//check 
int msgsock;//used in handler

struct sockaddr_in server;
int length;


int main(void){

	//initializing signal
	struct sigaction sa;
    sa.sa_handler = handler; 
	// SA_NODEFER to catch the same signal as the one currently handling
	// SA_RESTART to restart interrupted API calls
	sa.sa_flags = SA_NODEFER | SA_RESTART;

	if (sigaction(SIGCHLD, &sa, 0) == -1) {
            char buff1[30];
            sprintf (buff1, "Cannot handle SIGCHLD!\n");
            puts(buff1);
            exit(EXIT_FAILURE);
        }
	//check
	struct sigaction sa2;
    sa2.sa_handler = handler; 
	sa2.sa_flags = SA_NODEFER;
	if (sigaction(SIGINT, &sa2, 0) == -1) {
            char buff1[30];
            sprintf (buff1, "Cannot handle SIGINT!\n");
            puts(buff1);
            exit(EXIT_FAILURE);
        }
 
	char buf[1024];
	int rval;
	int i;

	/* Create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening stream socket");
		exit(1);
	}
	/* Name socket using wildcards */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(55000);
	if (bind(sock, (struct sockaddr *) &server, sizeof(server))) {
		perror("binding stream socket");
		server.sin_port = htons(55005);
		if (bind(sock, (struct sockaddr *) &server, sizeof(server))) {
			server.sin_port = htons(55006);
			if (bind(sock, (struct sockaddr *) &server, sizeof(server))){
				server.sin_port = htons(55007);
				if (bind(sock, (struct sockaddr *) &server, sizeof(server))){
				exit(0);
			}
			}
		}
	}
	/* Find out assigned port number and print it out */
	length = sizeof(server);
	if (getsockname(sock, (struct sockaddr *) &server, (socklen_t*) &length)) {
		perror("getting socket name");
		exit(1);
	}
	printf("Socket has port #%d\n", ntohs(server.sin_port));
	fflush(stdout);

	/* Start accepting connections */
	listen(sock, 100);

	int maxNoOfForks = 2;
	int forkCount = 0;
	int forkRes;
	bool callAccept;

	pthread_t thread1;
    const int *s = &sock;
    int  iret1;

	pthread_attr_t attr;
    int initRes = pthread_attr_init(&attr);

	
	int setdetachRes = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
    iret1 = pthread_create( &thread1, &attr, serverInteractionThread, (void*) sock);

    if(iret1){
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
        exit(EXIT_FAILURE);
    }

	pthread_t thread2;
	
	// thread to take inputs from clientHandlers and dump on the terminal
    pthread_create( &thread2, &attr, serverInteractionThreadReader, (void *) NULL);

	char buff[size];

	while(true){
					
			msgsock = accept(sock, (struct sockaddr *)&server,  (socklen_t*) &length);
				
			int fdClientHandlerReads[2];
			int fdConnectionReads[2];
			int res = pipe(fdClientHandlerReads);
			if(res < 0){
				perror("pipe 1");
			}
			res = pipe(fdConnectionReads);
			if(res < 0){
				perror("pipe 2");
			}

			sprintf(buff, "Accepted new client before fork().");
			puts(buff);

			forkRes = fork();
			
			if(forkRes > 0){
				indexOfAcceptedClients++;
				clients[clientsIndex].pid = forkRes;
				clients[clientsIndex].ip = inet_ntoa(server.sin_addr);
				clients[clientsIndex].port = ntohs(server.sin_port);
				clients[clientsIndex].sockfd = msgsock;
				clients[clientsIndex].fdRead = fdConnectionReads[0];
				clients[clientsIndex].fdWrite = fdClientHandlerReads[1];
				clientsIndex += 1;
			}
			else if(forkRes == 0){
				
				callAccept = false;
				clientHandler(msgsock, callAccept, forkRes, fdClientHandlerReads, fdConnectionReads);
			}
			else{
				perror("fork() in main() else()");				
			}
		// }
	}
	close(sock);
}

void *serverInteractionThreadReader(void *arg){
	char buff[10000];
	while (true)
	{
		struct pollfd fds[clientsIndex];
		int ret;
		
		char temp[100];

        int l = clientsIndex;
		for(int i = 0; i < l; i++){
			fds[i].fd = clients[i].fdRead;
			fds[i].events = POLLIN;
		}		

		ret = poll(fds,  l, 1000);

		if (ret == -1) {
			perror ("poll");
			continue;
		}	

		bool flag = false;
		for(int i= 0; i < clientsIndex; i++){
			if(fds[i].revents & POLLIN){
				int res = read(fds[i].fd, buff, 1024);
				if(res == -1){
					perror("serverThread read()");
				}
				else{
					//update list if first arg is Info, otherwise print what is received (list is printed)

					char* token = strtok(buff, " ");
					bool isInfo = true;
					int ind = 0;
					char *argv[20];
					while(token != NULL) {
						if(ind == 0){
							if(strcmp(token, "Info") != 0){
								isInfo = false;
								break;
							}
						}
						argv[ind] = token;
						ind += 1;
						token = strtok(NULL, " ");
					}				
					
					if(!isInfo){
						write(1, buff, res);
						flag = true;
					}
					else{
						//Info %d read %d write %d sock %d Port %u IP %s
						int index = atoi(argv[1]);
						int r = atoi(argv[3]);
						int w = atoi(argv[5]);
						int ms = atoi(argv[7]);
						int p = atoi(argv[9]);
                        sprintf(buff, "%s\0", argv[11]);
						clients[index].ip = buff;
						clients[index].port = p;
						clients[index].sockfd = ms;
						clients[index].fdRead = r;
						clients[index].fdWrite = w;
						
						sprintf(buff, "Receiving Info %d read %d write %d sock %d Port %u IP %s", clientsIndex,clients[index].fdRead, clients[index].fdWrite, clients[index].sockfd, clients[index].port, clients[index].ip);
						puts(buff);
					}
				}
			}
		}
		if(flag){
			char c = '\n';
			write(1, &c, 1);
		}

	}
	

}

void *serverInteractionThread(void *arg){

	char buff[size];
	int argc;
	char* token;
	char* argv[size];
	char* requirement;
	char temp[100];
	
	while (true){

		for(int i = 0; i < size; i++){
			buff[i] = '\0';
		}

		int readRes = read(0, buff, sizeof(buff));

		token = strtok(buff, " ");
		int ind = 0;
		while(token != NULL) {
			argv[ind] = token;
			ind += 1;
			token = strtok(NULL, " ");
		}

		// arg count:
		argc = ind;
		
		//multiplied by 2 for read and write fds
		struct pollfd fds[clientsIndex];
		int ret;

		//every alternate, starting from 0, for write
		//read for otherwise
		for(int i = 0; i < clientsIndex; i++){
			fds[i].fd = clients[i].fdWrite;
			fds[i].events = POLLOUT;
		}		

		ret = poll(fds,  clientsIndex, 1000);

		if (ret == -1) {
			perror ("poll");
			continue;
		}	
		
		char requirment[100];

		sprintf(buff, "%s", argv[0]);

		if(argc == 1){
			for(int i = 0; i < strlen(buff)-1; i++){
				requirment[i] = buff[i];
			}
			requirment[strlen(buff)-1] = '\0';
		}
		else{
			for(int i = 0; i < strlen(buff); i++){
				requirment[i] = buff[i];
			}
			requirment[strlen(buff)] = '\0';
		}
		

		if(strcmp(requirment, "exit") == 0){
			sprintf(buff, "Exiting");
			exit(1);
			puts(buff);
			
		}
		else if(strcmp(requirment, "list") == 0){
			//communicate with all client_communicators and get list
			if(argc > 2){
				sprintf(buff, "Invalid number of arguments for print");
				puts(buff);
				continue;
			}
			else if(argc == 1){
				sprintf(buff, "list");
				for(int i = 0; i < clientsIndex; i++){
					if(fds[i].revents & POLLOUT){
						write(fds[i].fd, buff, strlen(buff));
					}
				}
			}
			else if(argc == 2){
			sprintf(buff, "%s", argv[1]);
			if(buff[strlen(buff)-1] == '\n'){
				buff[strlen(buff)-1] = '\0';
			}
			bool digit = true;
			bool alpha = true;

			for(int j = 0; j < strlen(buff); j++){
				if(!isdigit(buff[j])){
					digit = false;
				}
				if(!isalpha(buff[j])){
					alpha = false;
				}
			}
			if(!digit && !alpha){
				sprintf(buff, "Invalid requirement");
				puts(buff);
				continue;
			}
			else if(alpha){
				sprintf(buff, "Invalid argument");
				puts(buff);
				continue;
			}
			else if(digit){
					char t[100];
					sprintf(t, "%s", "list");
					int index = atoi(argv[1]);
					if(index >= 0 && index < clientsIndex){
						if(clients[index].pid != -1){
							if(fds[index].revents & POLLOUT){
								write(fds[index].fd, t, strlen(t));
							}
						}
						else{
							sprintf(buff, "No more active");
							puts(buff);
							continue;
						}
					}	
					else{
							sprintf(buff, "Invalid index");
							puts(buff);
							continue;
						}
					
			}
			}
					else{
						sprintf(buff, "Invalid number of arguments");
						puts(buff);
						continue;
					}
		}
		else if(strcmp(requirment, "conn") == 0){
				displayClientCommunicators();
		}
		else if(strcmp(requirment, "print") == 0){
			if(argc < 2){
				sprintf(buff, "Invalid number of arguments for print");
				puts(buff);
				continue;
			}

			char firstWord[100];
			strcpy(firstWord, argv[1]);
			if(firstWord[strlen(firstWord)-1] == '\n'){
				firstWord[strlen(firstWord)-1] = '\0';
			}

			bool digit = true;
			bool alpha = true;

			for(int j = 0; j < strlen(firstWord); j++){
				if(!isdigit(firstWord[j])){
					digit = false;
				}
				if(!isalpha(firstWord[j])){
					alpha = false;
				}
			}
			if(!digit && !alpha){
				sprintf(buff, "Invalid requirement");
				puts(buff);
				continue;
			}
			if(alpha){
				char t[100];
				//send argv[0] (print command) to argv[argc-1] to clientHandler for printing
				
				sprintf(buff, "print");
				for(int i = 1; i < argc; i++){
					sprintf(buff, "%s %s", buff, argv[i]);
				}
				for(int i = 0; i < clientsIndex; i++){
					if(fds[i].revents & POLLOUT){
						write(fds[i].fd, buff, strlen(buff));
					}
				}
			}
			else if(digit){
				if(argc >= 3){
					char t[100];
					sprintf(t, "%s", "print");
					for(int i = 2; i < argc; i++){
						sprintf(t, "%s %s", t, argv[i]);
					}
					int index = atoi(argv[1]);

					if(index >= 0 && index < clientsIndex){
						if(clients[index].pid != -1){
							if(fds[index].revents & POLLOUT){
								write(fds[index].fd, t, strlen(t));
							}
						}
						else{
							sprintf(buff, "No more active");
							puts(buff);
							continue;
						}
					}	
					else{
							sprintf(buff, "Invalid index");
							puts(buff);
							continue;
						}
					}
					else{
						sprintf(buff, "Invalid number of arguments");
						puts(buff);
						continue;
					}
				
			}
		}
		else{
			sprintf(buff, "Invalid requirement");
			puts(buff);
		}
	
	}
}


void displayClientCommunicators(){
	char buff[1000];
	sprintf(buff, " ");
	for(int j = 0; j < clientsIndex; j++){
		sprintf(buff, "%s\nIndex: %d PID: %d IP: %s Port no: %u", buff, j, clients[j].pid, clients[j].ip, clients[j].port);
	}
	puts(buff);
	char duff[10];
	sprintf(duff, "--------");
	puts(duff);
}


void *clientHandlerThread(void *obj){

	char buff[1000];

	int *arrayPointer;
	
    arrayPointer = (int *) obj;

	//fdRead and fdWrite for communication with connection
	//msgSock to communicate with client
	int fdRead = arrayPointer[0];
	int fdWrite = arrayPointer[1];
	int msgSock = arrayPointer[2];

	int readResponseResult;
	int writeResponseResult;
	int argc;
	char* token;
	char* argv[size];
	char* requirement;

	while(true){
		for(int i = 0; i < 1000; i++){
			buff[i] = '\0';
		}
		readResponseResult = read(fdRead, buff, sizeof(buff)-1);

		if(readResponseResult == 0){
			sprintf(buff, "Connection broke in clientHandlerThread");
			puts(buff);
			exit(1);
			//check
			//pthread exit
		}
		if(readResponseResult == -1){
			sprintf(buff, "Read in clientHandlerThread");
			puts(buff);
			pthread_exit((void *)  NULL);
		}

		//adds input delimited by single space to argv array
		token = strtok(buff, " ");
		int ind = 0;
		while(token != NULL) {
			argv[ind] = token;
			ind += 1;
			token = strtok(NULL, " ");
		}
	
		// arg count:
		argc = ind;
		requirement = argv[0];

		if(strcmp(requirement, "list") == 0){
			//if requirment is list, send list to connection
			if(processIndex == 0){
				sprintf(buff, "None run");
			}
			else{
				time_t curTime = time(NULL);
				sprintf(buff, "\n");

				bool flag = false;

				for(int i = 0; i < processIndex; i++){
					time_t elapsedTime = curTime - processes[i].startTime;
					if(strcmp(processes[i].status, "active") == 0){
						flag = true;
						sprintf(buff, "%s\nIndex: %d              Name: %s             pID: %d          Start Time: %time_t          Elapsed Time: %time_t             Status:%s", buff, i, processes[i].name, processes[i].pid, processes[i].startTime, elapsedTime, processes[i].status);
					}
				}
				if(!flag){
					sprintf(buff, "None active");
				}
			}

			int writeResponseResult = write(fdWrite, buff, strlen(buff));
			if(writeResponseResult == -1){
				perror("write() in clientHandlerThread");
			}
			//check
			// sprintf(buff, "wrote this many bytes: %d", writeResponseResult);
			// puts(buff);
		}
		else if(strcmp(requirement, "print") == 0){
			//if connection requests to print something on client screen e.g. print abc
			
			//i = 1 so that print i.e. argv[0] is not returned to connection
			for(int i = 1; i < argc; i++){
				if(i == 1){
					sprintf(buff, "%s", argv[i]);
				}
				else{
					sprintf(buff, "%s %s",buff,argv[i]);
				}
			}
			
			write(msgsock, buff, strlen(buff));
		
		}
	}
}


void clientHandler(int sock, bool callAccept, int pid, int fdClientHandlerReads[2], int fdConnectionReads[2]){
	//buffer used for read 	
	char buff0[size];

	//buffer used for sprintf 	
	char buff1[size];

	int msgsock;
	if(callAccept){
		msgsock = accept(sock, (struct sockaddr *)&server,  (socklen_t*) &length);
		
		if (msgsock == -1){
			perror("accept");
		}
		else{
			sprintf(buff1, "Accepted new client after fork().");
			puts(buff1);

			//sending info to conn
			sprintf(buff1, "Info %d read %d write %d sock %d Port %u IP %s", clientsIndex, fdConnectionReads[0], fdClientHandlerReads[1], msgsock, ntohs(server.sin_port), inet_ntoa(server.sin_addr));
			write(fdConnectionReads[1], buff1, strlen(buff1));
			puts(buff1);
		}
	}
	else{
		msgsock = sock;
	}

	pthread_t thread1;
    int  iret1;
	

	int array[3];
	array[0]= fdClientHandlerReads[0];
	array[1] = fdConnectionReads[1];
	array[2] = msgsock;

	pthread_attr_t attr;
    int initRes = pthread_attr_init(&attr);
	int setdetachRes = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
	// creating thread that communicates with interactive server process
    iret1 = pthread_create( &thread1, &attr, clientHandlerThread, (void*) array);

    if(iret1){
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
    }

	int responseAttempt;
	int argc;
	char* argv[size];
	

	do {

		//there could be garbage values in buff0
		for(int i = 0; i < size; i++){
			buff0[i] = '\0';
			buff1[i] = '\0';
		}

		int readResult = read(msgsock,  buff0, sizeof(buff0)-1);

		if(readResult < 0){
			sprintf(buff1, "Server: error in reading from msgsock of client, exiting Client Handler.");
			responseAttempt = write(msgsock, buff1, strlen(buff1)-1);
			sprintf(buff0, "Wrote this to client: %s", buff1);
			puts(buff0);
			exit(1);
			//check
		}
		if(readResult == 0){
			sprintf(buff1, "\nClient Handler: I am terminating.");
			puts(buff1);
			exit(1);
		}

		sprintf(buff1, "Received this from client: %s", buff0);
		puts(buff1);

		//adds input delimited by single space to argv array
		char* token = strtok(buff0, " ");
		int ind = 0;
		while(token != NULL) {
			argv[ind] = token;
			ind += 1;
			token = strtok(NULL, " ");
		}

		// arg count:
		argc = ind;
		char* requirement = argv[0];

		if(argc == 1){
			if(strcmp(requirement, "list") == 0){
				
				if(processIndex == 0){
					sprintf(buff1, "Please initiate a process first.");
				}
				else{
					time_t curTime = time(NULL);
					sprintf(buff1, " ");

					bool flag = false;

					for(int i = 0; i < processIndex; i++){
						time_t elapsedTime = curTime - processes[i].startTime;
						if(strcmp(processes[i].status, "active") == 0){
							flag = true;
							sprintf(buff1, "%s\nIndex: %d              Name: %s             pID: %d          Start Time: %time_t          Elapsed Time: %time_t             Status:%s", buff1, i, processes[i].name, processes[i].pid, processes[i].startTime, elapsedTime, processes[i].status);
						}
					}
					sprintf(buff1, "\n%s\nNote: me_t = seconds.", buff1);
					if(!flag){
						sprintf(buff1, "No active process.");
					}
				}
			
		}
			else{
				sprintf(buff1, "Invalid requirements. Please re-enter.");
			}

			responseAttempt = write(msgsock, buff1, strlen(buff1)-1);
			sprintf(buff0, "Wrote this to client: %s", buff1);
			puts(buff0);
			continue;
		}
		
		char* requirement2 = argv[1];


		if(strcmp(requirement, "add") == 0){ 

			int total = 0;
			bool canAdd = true;
			//used to calculate length
			char* current;
			
			
			// for loop begins ----------------------------
			for(int i = 1; i < argc; i++){
				canAdd = true;
				current = argv[i];

				for(int j = 0; j < strlen(current); j++){
					if(!isdigit(argv[i][j])){
						canAdd = false;
						break;
					}
				}
				if(canAdd){
					total += atoi(argv[i]);
				}
				else{
					break;
				}
			}
			// for loop ended ----------------------------

			if(canAdd){
				sprintf(buff1, "The total is: %d.", total); 
			}
			else{
				sprintf(buff1, "Invalid input. Please enter valid numbers only.");
			}
		}
		else if(strcmp(requirement, "sub") == 0){

			int total = 0;
			bool canAdd = true;
			char* current;
			// for loop begins ----------------------------
			for(int i = 1; i < argc; i++){
				canAdd = true;
				current = argv[i];
				
				for(int j = 0; j < strlen(current); j++){
					if(!isdigit(argv[i][j])){
						canAdd = false;
						break;
					}
				}
				if(canAdd){
					if(i == 1) total += atoi(argv[i]);
					else total -= atoi(argv[i]);
				}
				else{
					break;
				}
			}
			// for loop ended ----------------------------

			if(canAdd){
				sprintf(buff1, "The total is: %d.", total);
			}
			else{
				sprintf(buff1, "Invalid input. Please enter valid numbers only.");
			}
		}
		else if(strcmp(requirement, "mul") == 0){

			int total = 1;
			bool canAdd = true;
			char* current;
			// for loop begins ----------------------------
			for(int i = 1; i < argc; i++){
				canAdd = true;
				current = argv[i];
				for(int j = 0; j < strlen(current); j++){
					if(!isdigit(argv[i][j])){
						canAdd = false;
						break;
					}
				}
				if(canAdd){
					total = total * atoi(argv[i]);
				}
				else{
					break;
				}
			}
			// for loop ended ----------------------------

			if(canAdd){
				sprintf(buff1, "The total is: %d.", total);
			}
			else{
				sprintf(buff1, "Invalid input. Please enter valid numbers only.");
			}
		}
		else if(strcmp(requirement, "div") == 0){
			float total = 1;
			bool canAdd = true;
			char* current;
			// for loop begins ----------------------------
			for(int i = 1; i < argc; i++){
				canAdd = true;
				current = argv[i];
				for(int j = 0; j < strlen(current); j++){
					if(!isdigit(argv[i][j])){
						canAdd = false;
						break;
					}
				}
				//is an integer
				if(canAdd){
						if(i == 1 && argv[i] == 0) total = 0;
						else if(i == 1) total = atoi(argv[i]);
						else if(total == 0.0){
							total = 0;
						}
						else if(atoi(argv[i]) == 0){
							canAdd = false;
							break;
						}
						else total = total/atoi(argv[i]);
					
				}
				else{
					break;
				}
			}
			// for loop ended ----------------------------

			if(canAdd){
				sprintf(buff1, "The total is: %f.", total);
			}
			else{
				sprintf(buff1, "Invalid input. Please enter valid numbers only.");
			}
		}
		else if(strcmp(requirement, "run") == 0){ // -------------------------------------------------------------- RUN -------------------------------------------------------------------------------------------
			
			char* current = argv[1];

			int l = strlen(current);

			char name[l+1];

			for(int i = 0; i < l; i++){
				name[i] = argv[1][i];
			}
			name[l] = '\0';

			int fdServerChildToServer[2];

			int pipeRes = pipe2(fdServerChildToServer, O_CLOEXEC);
			
			if(pipeRes < 0){
				sprintf(buff1, "Server: error in initiating pipe.");
			}

			else{
				
				int newPID = fork();

				if(newPID < 0){
					sprintf(buff1, "Server: error in fork before execution.");
				}
				else if(newPID == 0){ // child
										
					sprintf(buff1, "%s", name);
					int execResult = execlp(buff1, buff1, "-s", NULL);

					write(fdServerChildToServer[1], "failure", 10);

					//terminate this process
					exit(EXIT_SUCCESS);
				}
				else{//parent - keep it running
					
					close(fdServerChildToServer[1]);
					int res = read(fdServerChildToServer[0],  buff0, 10);
					if(res == 0){//success
							for(int i = 0; i < strlen(name); i++){
							processes[processIndex].name[i] = name[i];
						}
						processes[processIndex].name[strlen(name)] = '\0';
						processes[processIndex].startTime = time(NULL);
						processes[processIndex].pid = newPID;
						char temp[9] = "active"; //9 so that inactive is assignable
						for(int i = 0; i < strlen("active"); i++){
							processes[processIndex].status[i] = temp[i];
						}
						processes[processIndex].status[strlen("active")] = '\0';
						
						processIndex += 1;
						sprintf(buff1, "Success.");
					}
					else{//exec has failed
						sprintf(buff1, "exec() failed.");
					}
			}
			}
			
			//run ends   
		}

		else if((strcmp(requirement, "list") == 0) && (strcmp(requirement2, "all") == 0)){
			if(processIndex == 0){
				sprintf(buff1, "Please initiate a process first.");
			}
			else{
			time_t curTime = time(NULL);
			sprintf(buff1, " ");
			for(int i = 0; i < processIndex; i++){
				time_t elapsedTime = curTime - processes[i].startTime;
				sprintf(buff1, "%s\nIndex: %d              Name: %s             pID: %d          Start Time: %time_t          Elapsed Time: %time_t             Status:%s", buff1, i, processes[i].name, processes[i].pid, processes[i].startTime, elapsedTime, processes[i].status);
			}
			sprintf(buff1, "\n%s\nNote: me_t = seconds.", buff1);
			}
		}

		else if(strcmp(requirement, "kill") == 0){// -------------------------------------------------------------- KILL -------------------------------------------------------------------------------------------
			
			bool isID = true;
			bool isName = true;
			
			for(int i = 0; i < strlen(requirement2); i++){
				if(!isdigit(requirement2[i])){
					isID = false;
				}
				if(isdigit(requirement2[i])){
					isName = false;
				}
			}
			if(!isName && !isID){
				sprintf(buff1, "Invalid id and/or name.");
			}
			else{
				bool success = true;
				bool inList = false;
				bool flag = true;

				if(isName){
					for(int i = 0; i < processIndex; i++){
						if((strcmp(processes[i].name, requirement2) == 0) && (strcmp(processes[i].status, "active") == 0)){
							inList = true;
							int res = kill(processes[i].pid, SIGKILL);
							sprintf(buff1, "Ran kill on %d. Kill res = %d", processes[i].pid, res);
							puts(buff1);

							if(res == 0){//mark inactive
								char temp[9] = "inactive";
								for(int j = 0; j < strlen("inactive"); j++){
									processes[i].status[j] = temp[j];
								}
								processes[i].status[strlen("inactive")] = '\0';
								}
							else{
								sprintf(buff1, "Error in kill");
								puts(buff1);
								success = false;
							}
						}
					}
				}
				else{
					for(int i = 0; i < processIndex; i++){
						if(processes[i].pid == atoi(requirement2) && (strcmp(processes[i].status, "active") == 0)){
						
							inList = true;
							int res = kill(processes[i].pid, SIGKILL);
							sprintf(buff1, "Ran kill on %d. Kill res = %d", processes[i].pid, res);
							puts(buff1);

							if(res == 0){ //mark inactive
								char temp[9] = "inactive";
								for(int j = 0; j < strlen("inactive"); j++){
									processes[i].status[j] = temp[j];
								}
								processes[i].status[strlen("inactive")] = '\0';
								}
							else{
								sprintf(buff1, "Error in kill");
								success = false;
							}
						}
					}

				}
				if(success && inList){
					sprintf(buff1, "Done killing.");
				}
				else{
					if(flag){
					sprintf(buff1, "Invalid id and/or name.");
					}
				}
			}
			
		}
		
		else{
			sprintf(buff1, "Invalid requirement. Please re-enter.");
		}

		responseAttempt = write(msgsock, buff1, strlen(buff1)-1);
		sprintf(buff0, "Wrote this to client: %s", buff1);
		puts(buff0);
		
	} while (true);
	close(msgsock);
	exit(1);
}


void handler (int signo){
	if(signo == SIGCHLD){

		// int id = getpid();
		char buff[1000] = "\nCaught SIGCHILD.\n";

		int terminatedProcessID = waitpid(-1, NULL, WNOHANG);

		for(int i = 0; i < 100; i++){
			if(processes[i].pid == terminatedProcessID){
				//mark inactive
				sprintf(buff, "\nCollected pid of application %s with PID = %d", processes[i].name, processes[i].pid);
				puts(buff);
				char temp[9] = "inactive";
				for(int j = 0; j < strlen("inactive"); j++){
					processes[i].status[j] = temp[j];
				}
				processes[i].status[strlen("inactive")] = '\0';
			}
			if(clients[i].pid == terminatedProcessID){
				//unable to reach here since list not updated, so EITHER update the list through updateActivated() OR loop until -1
				//with pid = 0 since not immediate child!
				sprintf(buff, "Collected pid of client handler. Index = %d. PID = %d\n Updated client handlers list is as follows:", i, clients[i].pid);
				puts(buff);


				sprintf(buff, " ");
				clients[i].pid = -1;
				for(int j = 0; j < clientsIndex; j++){
					sprintf(buff, "%s\nIndex: %d PID: %d", buff, j, clients[j].pid);
				}
				puts(buff);
				
			}
		}
	}
	//check
	else if(signo == SIGINT){
		int id = getpid();
		char buff[1000] = "Caught SIGINT. Closing sock and msgsock";
		sprintf(buff, "%s in %d.\n", buff, id);
		write(1, buff, strlen(buff));
		close(msgsock);
		close(sock);
		exit(1);
	}
}