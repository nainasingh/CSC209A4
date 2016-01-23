#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lists.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
	#define PORT 59098
#endif

//asked on Piazza 
#define MAXCLIENTS 20
#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 12
#define LISTENQ 30


typedef struct {
	int soc;
	int curpos; 
	char * buf[INPUT_BUFFER_SIZE]; 
	char * name; 
} Client;

Poll * pollList; 
Client clients[MAXCLIENTS];

//Calls poll functions and returns 0 if it went alright
//modified from polls.c
int process_args(int cmd_argc, char **cmd_argv, Poll **poll_list, Client * user) {

    // some commands need the poll_list head and others
    // require the address of that head
    char * message; 

    if (cmd_argc <= 0) {
        return 0;
    } else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        return -1;
        
    } else if (strcmp(cmd_argv[0], "list_polls") == 0 && cmd_argc == 1) {
        message = print_polls(*poll_list);
        if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}   
        
    } else if (strcmp(cmd_argv[0], "create_poll") == 0 && cmd_argc >= 3) {
        int label_count = cmd_argc - 2;
        int result = create_poll(cmd_argv[1], &cmd_argv[2], label_count,
                     poll_list);
        if (result == 1) {
        	message = "Poll by this name already exists\r\n"; 
             if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        }
   } else if (strcmp(cmd_argv[0], "vote") == 0 && cmd_argc == 4) {
         // name for clarity of code below
        char *poll_name = cmd_argv[2];        // better name for clarity of code below

        // try to add participant to this poll  
        int return_code = add_participant(user->name, poll_name, *poll_list, cmd_argv[3]);
        if (return_code == 1) {
        	message = "Poll by this name does not exist.\r\n"; 
             if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        } else if (return_code == 2) {
            // this poll already has this client participating so don't add
            // instead just update the vote
            return_code = update_availability(user->name, poll_name, cmd_argv[3], *poll_list); 
        }
        // this could apply in either case
        if (return_code == 3) {
        	message = "Availability string is wrong size for this poll.\r\n"; 
            if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        }
        if (return_code == 0){
        	int msglen = snprintf(NULL, 0, " There has been new activity on poll %s.\r\n", poll_name) + 1;
        	snprintf(message, msglen, " There has been new activity on poll %s.\r\n", poll_name); 
        	 if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        }
    } else if (strcmp(cmd_argv[0], "comment") == 0 && cmd_argc >= 4) {
        // first determine how long a string we need
        int space_needed = 0;
        int i;
        for (i=3; i<cmd_argc; i++) {
            space_needed += strlen(cmd_argv[i]) + 1;
        }
        // allocate the space
        char *comment = malloc(space_needed);
        if (comment == NULL) {
            perror("malloc");
            exit(1);
        }
        // copy in the bits to make a single string
        strcpy(comment, cmd_argv[3]);
        for (i=4; i<cmd_argc; i++) {
            strcat(comment, " ");
            strcat(comment, cmd_argv[i]);
        }
        
        int return_code = add_comment(user->name, cmd_argv[2], comment, 
                                      *poll_list);
        // comment was only used as parameter so we are finished with it now
        free(comment);

        if (return_code == 1) {
           message = "No poll by this name exists.\r\n"; 
            if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        } else if (return_code == 2) {
        	message = "There is no participant by this name in this poll"; 
           if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        }
    } else if (strcmp(cmd_argv[0], "delete_poll") == 0 && cmd_argc == 2) {
        if (delete_poll(cmd_argv[1], poll_list) == 1) {
        	message = "No poll by this name exists.\r\n"; 
            if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        }
    } else if (strcmp(cmd_argv[0], "poll_info") == 0 && cmd_argc == 2) { 
    	message = print_poll_info(cmd_argv[1], *poll_list); 
        if (message == NULL) {
            message = "No poll by this name exists.\r\n";
            if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        }
        else{
        	 if (write(user->soc, message, strlen(message) + 1) == -1) {
				perror("write");
			}
        }
    } else {
    	message = "Incorrect syntax\r\n"; 
        if (write(user->soc, message, strlen(message) + 1) == -1) {
			perror("write");
		}
    }
    return 0;
}

//Reads from client and call process_args
//Returns -1 if client closed connection and 1 otherwise 
int readfromclient(Client * client, Client * clients, Poll * pollList){
	char *startptr = client->buf[client->curpos];
	char * commands[INPUT_ARG_MAX_NUM]; 
	int num_commands = 0; 
	int parameter = 0; 
	int command =0; 
	char * actual_args[INPUT_BUFFER_SIZE]; 
	int len = read(client->soc, startptr, INPUT_BUFFER_SIZE - client->curpos);
	//client died on you 
	if (len <= 0){
		return -1; 
	}
	//there must be something to read
	else{
		int process; 
		client->curpos += len; 
		*client->buf[client->curpos] = '\0';
		//check if complete line and tokenize
		if(strchr(*client->buf, '\n')) {
			char * split = strtok(*client->buf, "\n");
			//count commands and copy to another array
			while(split != NULL){
				commands[num_commands] = split; 
				num_commands++; 
				split = strtok(NULL, "\n"); 
			}
			//separate by spaces + process eah command 
			while(commands[command] != NULL){
				//make sure each command isn't too long 
				char * split2 = strtok(commands[command], " "); 
				while(split2 != NULL){
					parameter++; 
					if (parameter > INPUT_ARG_MAX_NUM - 1){
						char *msg = "Too many arguments!\r\n";
						if(write(client->soc, msg, strlen(msg) + 1) == -1) {
							perror("write");
						}
						parameter = 0; 
						break; 
					}
					actual_args[parameter] = split2; 
					parameter++; 
					split2 = strtok(NULL, " ");
				}
				actual_args[parameter] = NULL; 
				//if it's the right length send it to the function 
				process = process_args(parameter, actual_args, &pollList, client); 
				if (process == -1){
					return -1; 
				}
				command++; 
			}
			// Need to shift anything still in the buffer over to beginning.
			char *leftover = client->buf[client->curpos];
			memmove(client->buf, leftover, client->curpos);
			client->curpos = 0;
		}
	}
	return 1; 
}

//mostly borrowed from Alan Rosenthal 
int main(int argc, char ** argv){
	//file descriptor 
	int listenfd, connfd; 
	//size of the address of client 
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	int i = 0; 
	int maxi; 

	//create socket 
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }

    //init serv_addr to zeroes 
   	bzero(&serv_addr, sizeof(serv_addr));

   	serv_addr.sin_family = AF_INET;
   	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   	//listen to this port 
   	serv_addr.sin_port = htons(PORT);

   	//bind socket to address server runs on   	
    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) {
		perror("bind");
		exit(1);
    }

    int yes; 
    //local addr reuse
    if (( setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ) == -1) {
        perror("setsocket");
        exit(1); 
    }

    //listen for connections 
    if (listen(listenfd, LISTENQ)){
    	perror("listen");
    	exit(1);
    }
    //for select 
    int maxfd = listenfd;
    fd_set allset,rset; 
    //empty it
    FD_ZERO(&allset); 
    //add fd to  set 
    FD_SET(listenfd, &allset); 

    //all sockets should be available 
    
    maxi = -1; 
    for (i = 0; i < MAXCLIENTS; i++) {
        clients[i].soc = -1;
        clients[i].curpos = 0; 
    }

    //initialize pollLIST
    pollList = NULL; 
    char * firstMsg = "What is your user name?\r\n"; 

    //exit server by killing it 
    while(1){
    	//weird thing to do from lab 
    	rset = allset; 
    	//select
    	if (select(maxfd + 1, &rset, NULL, NULL, NULL) < 0) {
			perror("select");
			exit(1); 
		}
		else{
			//new client  
			if(FD_ISSET(listenfd, &rset)){
				clilen = sizeof(cli_addr);
				connfd = accept(listenfd, (struct sockaddr *) &cli_addr, &clilen);
				if (connfd < 0){
					perror("accept"); 
					exit(1); 
				}
 				//add new client 
 				for (i = 0; i < MAXCLIENTS; i++) {
 					//if clients[i] is not storing a client then get it to store client 
	                if (clients[i].soc < 0) {
	                    clients[i].soc = connfd;
	                    break;
	                }
            	}
            	//set new maxfd and rset
            	FD_SET(connfd, &rset); 
            	if (connfd > maxfd){
            		maxfd = connfd; 
            	}
            	if (i > maxi){
            		maxi = i; 
            	}  
            	//ask for name           	
            	if(write(connfd, firstMsg, strlen(firstMsg) + 1) == -1){
					perror("write");
					exit(1); 
				}
				//read user name 
				char * username = NULL; 
				if (read(connfd,username, sizeof (char *) == -1)){
					perror("read"); 
					exit(1); 
				}
				//save it with client 
				clients[i].name = username; 
				clients[i].name[strlen(username)] = '\0'; 
				char *msgbuf = NULL; 
				//person can do whatever they want now 
				sprintf(msgbuf, "Go ahead and enter poll comands>\r\n");
				if(write(connfd, msgbuf, strlen(msgbuf) == -1)){
					perror("write");
					exit(1);					
				}
			}
			//read from all clients 
			for (i = 0; i < maxi; i++){
				if (clients[i].soc > 0){
					if (FD_ISSET(clients[i].soc, &rset)){
						//read from client + tokenize commands
						int r = readfromclient(&clients[i], clients, pollList);
						//remove client 
						if (r == -1){
							if(close(clients[i].soc) == -1){
								perror("close"); 
								exit(1); 
							}
							//reset the stuff 
							FD_CLR(clients[i].soc, &rset);
							clients[i].soc = -1;
							memset(clients[i].buf, '\0', INPUT_BUFFER_SIZE);
							memset(clients[i].name, '\0', INPUT_BUFFER_SIZE);
							clients[i].curpos = 0; 
						}
					}
				}
			}
		}
    }
	return 0; 
}