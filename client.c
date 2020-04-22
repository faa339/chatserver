/*
Author:Faris Alotaibi
Description:Client to connect to the server and chat with.
Run with ./client username 
Note that username must be below 256 characters 
*/
#include "headers.h"

void chat(char* username, int chatteefd);

int main(int argc, char* argv[])
{
	errno = 0;
	int clientfd, writetest;
	struct sockaddr_in servaddr;
	int port = PORTNUM;
	char* ip = "127.0.0.1";
	

	char* username = argv[1];
	if (username == NULL)
	{
		fprintf(stderr, "Usage is ./client username\n");
		exit(EXIT_FAILURE);
	}

	if(strlen(username) >=256)
	{
		fprintf(stderr, "Username should be below 256 characters\n");
		exit(EXIT_FAILURE);
	}

	if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Socket");
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if(inet_pton(AF_INET, ip, &servaddr.sin_addr) <=0)
	{
		if(errno)
			perror("inet_pton");
		else
			fprintf(stderr, "inet error\n");
		exit(EXIT_FAILURE);
	}

	if((connect(clientfd, (struct sockaddr *)&servaddr, 
				sizeof(servaddr))) < 0)
	{
		perror("Connect error");
		exit(EXIT_FAILURE);
	}
	//Send the username first	
	writetest = write(clientfd, username, strlen(username)+1);
	if(writetest < 0)
	{
		perror("Server");
		exit(EXIT_FAILURE);
	}
	chat(username, clientfd);
	close(clientfd);
	printf("Disconnected\n");
	exit(EXIT_SUCCESS);
}


void chat(char* username, int chatteefd)
{
	//This code deals with nonblocking IO from stdin and 
	//the client being talked to through use of select
	//Note that because messages are username:message format
	//the clientbuf's size was increased to accomodate that.
	int maxfd, selectnum;
	char inputbuf[MAXLINE];
	char clientbuf[MAXNAME+MAXLINE];
	struct timeval timeout;
	fd_set set;

	timeout.tv_sec = TIMEOUT_SECS;
	timeout.tv_usec = 0;
	while(1) //Keep looking for input
	{
		FD_ZERO(&set);
		memset(inputbuf, 0, MAXLINE-1);
		memset(clientbuf, 0, MAXLINE-1);

		maxfd = chatteefd + 1;
		FD_SET(chatteefd, &set);
		FD_SET(0, &set);
		selectnum = select(maxfd, &set, NULL, NULL, &timeout);
		if (selectnum < 0)
		{
			perror("Select failed");
			break;
		}

		if(FD_ISSET(0, &set)) //Check if anything received via stdin
		{
			int n = read(0,inputbuf, MAXLINE);
			inputbuf[n] = 0;

			if(strcmp(inputbuf, "exit\n") == 0) //Chat's over
			{
				//Make sure the server knows we're done too
				int writetest = write(chatteefd, inputbuf, strlen(inputbuf)+1);
				if(writetest < 0)
				{
					perror("Self exit");
					fflush(stdout);
					break;
				}
				break;
			}
			//Write the username first, then the message 
			if (write(chatteefd, inputbuf, n) < 0)
			{
				perror("Client");
				fflush(stdout);
				break;
			}
		}

		//Check if anything from the chat partner
		if(FD_ISSET(chatteefd, &set))
		{
			int n=read(chatteefd, clientbuf, (MAXNAME+MAXLINE));
			if(n==0)break; //In case we were closed from server side
			clientbuf[n] = 0;
			if(write(1, clientbuf, n) < 0)
			{
				perror("Client");
				fflush(stdout);
				break;
			}
		}
	}
	//Leaving it on the caller to close the fd they passed in here
}