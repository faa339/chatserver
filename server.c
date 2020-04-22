/*
Author:Faris Alotaibi

Description: Server that clients connect to.
Run with ./server 
*/

#include "headers.h"

//msgQ mutex and cond var
pthread_mutex_t qMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t qCond = PTHREAD_COND_INITIALIZER;

//clients array mutex
pthread_mutex_t connectMutex = PTHREAD_MUTEX_INITIALIZER;


void* clientThread(void* fd);
void* messageThread(void* arg);
int addremoveClient(int fd,char* username, int op);
void message(int fd,char* username);
void addmsg(char* msg);
char* popmsg(void);
void broadcast(char* msg);
int listConnects(int fd);
void disconnect(int fd, char* namebuf);

//Note that this is an array of fds to make broadcasting easier
int clients[MAXCONS];
int numCons = 0;

char **users;

char **msgQ;
int front = 0;
int back = CAPACITY-1;
int currsize = 0;


int main(int argc, char *argv[])
{
	errno = 0;
	int serverfd;
	struct sockaddr_in servaddr;
	int port = PORTNUM;
	
	if((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Socket");
		exit(EXIT_FAILURE);
	}

	//Setup our sockaddr_in for use in bind
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	char* ip = "127.0.0.1"; //Server assumed to be running here
	servaddr.sin_port = htons(port);

	if(inet_pton(AF_INET, ip, &servaddr.sin_addr) <=0)
	{
		perror("inet_pton");
		exit(EXIT_FAILURE);
	}

	if ((bind(serverfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) < 0)
	{
		perror("Bind");
		exit(EXIT_FAILURE);
	}

	if((listen(serverfd, LISTENQ)) < 0)
	{
		perror("Listen");
		exit(EXIT_FAILURE);
	}

	//Dont need to acquire lock here because no other threads exist yet 
	for(int i =0; i<MAXCONS; i++)
		clients[i]=-1;

	//Init our Queue and users array & clear them out
	//Note that our lines are always maxname+maxline
	msgQ = (char**) malloc(CAPACITY * sizeof(char*));
	for(int i=0; i<CAPACITY; i++)
	{
		msgQ[i] = (char*) malloc((MAXLINE+MAXNAME) * sizeof(char));
		memset(msgQ[i], 0, MAXLINE-1);
	}

	users = (char**) malloc(MAXCONS * sizeof(char*));
	for(int i=0; i<MAXCONS; i++)
	{
		users[i] = (char*) malloc(MAXNAME * sizeof(char));
		memset(users[i], 0, MAXNAME-1);
	}

	//Create and detach our message consumer
	pthread_t msghandler;
	pthread_create(&msghandler, NULL, messageThread, NULL);
	pthread_detach(msghandler);
	while(1) //Get connections and start making clients
	{
		int clientfd = accept(serverfd, NULL, NULL);
		pthread_t tid;
		pthread_create(&tid, NULL, clientThread, &clientfd);
		//Detaching bc we don't care for their returns
		pthread_detach(tid);
	}

	close(serverfd);
	exit(EXIT_SUCCESS);
}

void* messageThread(void* arg)
{
	//Message consumer thread -- will simply get messages and 
	//broadcast them to all clients
	while(1)
	{
		char* message = popmsg();
		broadcast(message);
	}
	pthread_exit((void*)EXIT_SUCCESS);
}

void broadcast(char* msg)
{
	//Send a message to all clients 
	pthread_mutex_lock(&connectMutex);
	int writetest=0;
	for(int i=0; i<MAXCONS;i++)
	{
		if(clients[i] != -1)
		{
			writetest =write(clients[i], msg, strlen(msg)+1);
			if(writetest < 0)
			{
				perror("Broadcast");
				break;
			}
		}
	}
	pthread_mutex_unlock(&connectMutex);
}

int listConnects(int fd)
{
	//List all current connections to a client
	pthread_mutex_lock(&connectMutex);
	char* startmsg = "Connected users:\n";
	int writetest = write(fd, startmsg, strlen(startmsg)+1);
	if(writetest < 0)
	{
		perror("listConnects");
		pthread_mutex_unlock(&connectMutex);
		return -1;
	}	
	for(int i=0; i<MAXCONS; i++)
	{
		if(strlen(users[i]) != 0)
		{
			char* msg = (char*) malloc(MAXNAME * sizeof(char));
			sprintf(msg, "%s\n", users[i]);
			writetest = write(fd, msg, strlen(msg)+1);
			if(writetest < 0)
			{
				perror("listConnects");
				pthread_mutex_unlock(&connectMutex);
				return -1;
			}
		}
	}
	pthread_mutex_unlock(&connectMutex);
	return 0;
}

void* clientThread(void* fd)
{
	//Thread to deal with connected Client
	
	//Client always sends name first -- get that before 
	//beginning chat 
	char *namebuf = (char*) malloc(MAXNAME * sizeof(char));
	memset(namebuf, 0, MAXNAME-1);
	int clientfd = *((int*) fd); 
	int test = read(clientfd,namebuf, MAXNAME-1);
	if(test==0) //Something went wrong with the client 
	{
		perror("Client thread");
		close(clientfd);
		pthread_exit((void*)EXIT_FAILURE);
	} 
	namebuf[test]=0;
	printf("Client connected: %s\n",namebuf);

	//Add us to the connected users list (if theres room)
	if(addremoveClient(clientfd,namebuf,ADD) < 0)
	{
		char* discmsg = "The chatroom is full\n";
		test= write(clientfd, discmsg, strlen(discmsg)+1);
		printf("Client refused: %s\n",namebuf);
		close(clientfd);
		pthread_exit((void*)EXIT_FAILURE);
	} 
	char* connected = (char*) malloc((MAXLINE) * sizeof(char));

	//Let everyone know that we're here 
	sprintf(connected, "%s has connected.\n", namebuf);
	broadcast(connected);

	//Display all connected users
	test = listConnects(clientfd);
	if(test < 0)//Something went wrong with the client
	{
		disconnect(clientfd, namebuf);
		pthread_exit((void*)EXIT_FAILURE);
	}

	//Begin chatting
	message(clientfd, namebuf);

	//We're done by this point, time to exit 
	disconnect(clientfd,namebuf);
	close(clientfd);
	pthread_exit((void*)EXIT_SUCCESS);
}

void disconnect(int fd, char* namebuf)
{
	//Handles all disconnect activities 
	//Tells other users about disconnection, logs it to server, 
	//and removes client from the users and fd arrays
	char* message = (char*) malloc((MAXLINE+MAXNAME)*sizeof(char));
	sprintf(message, "%s is disconnecting.\n", namebuf);
	broadcast(message);
	printf("Client disconnected: %s\n",namebuf);
	addremoveClient(fd,namebuf,REMOVE);
}

int addremoveClient(int fd,char* username, int op)
{
	//Function to add/remove someone from the users and fd list
	pthread_mutex_lock(&connectMutex);
	if(op==ADD && (numCons != MAXCONS))
	{
		for(int i=0; i<MAXCONS; i++)
		{
			if(clients[i]==-1)//Look for the first available client
			{
				//printf("Found a spot\n");
				clients[i] = fd;
				memset(users[i], 0, MAXNAME-1);
				users[i] = username;
				numCons++;
				pthread_mutex_unlock(&connectMutex);
				return 0;
			}
		}
	}else if(op==REMOVE && numCons != 0)
	{
		for(int i=0; i<MAXCONS; i++)
		{
			if(clients[i] == fd)
			{
				clients[i] = -1;
				memset(users[i], 0, MAXNAME-1);
				numCons--;
				pthread_mutex_unlock(&connectMutex);
				return 0;
			}
		}
	}
	pthread_mutex_unlock(&connectMutex);
	//We only get here if there was no room to connect
	return -1;
}

void message(int fd, char* username)
{
	//Chat processing function for each client 
	//Every message received is formatted and pushed 
	//onto msgQ 
	while(1)
	{
		char buf[MAXLINE];
		int n = read(fd, buf, MAXLINE);
		if(n==0) break; //Can't receive from the client anymore
		buf[n] = 0;
		if(strcmp(buf, "exit\n") == 0) break;
		if(strcmp(buf, "\n") == 0) continue; //dont process empty newlines
		char* message = (char*) malloc((MAXLINE+MAXNAME) * sizeof(char));
		sprintf(message, "%s: %s", username, buf);
		addmsg(message);
	}
}

void addmsg(char* msg)
{
	//Push a message onto the Q 
	pthread_mutex_lock(&qMutex);
	while(currsize == CAPACITY)
		pthread_cond_wait(&qCond, &qMutex);

	back = (back+1)%CAPACITY;
	msgQ[back] = msg;
	currsize++;

	pthread_mutex_unlock(&qMutex);
	pthread_cond_broadcast(&qCond);
}

char* popmsg(void)
{
	//Get a message off the Q
	pthread_mutex_lock(&qMutex);
	while(currsize == 0)
		pthread_cond_wait(&qCond, &qMutex);
	char* msg = (char*) malloc((MAXLINE+MAXNAME) * sizeof(char));
	msg = msgQ[front];
	front = (front+1)%CAPACITY;
	currsize--;

	pthread_mutex_unlock(&qMutex);
	pthread_cond_broadcast(&qCond);
	return msg;
}