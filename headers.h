//Author:Faris Alotaibi
//Description: Assorted headers & defines needed for functionality

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>      
#include <stdio.h>        
#include <unistd.h>      
#include <sys/select.h>  
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define LISTENQ 		1024
#define PORTNUM 		13000
#define TIMEOUT_SECS 	120
#define CAPACITY 		1024
#define MAXLINE 		4098
#define MAXCONS 		10
#define MAXNAME 		256
#define ADD 			0
#define REMOVE			1