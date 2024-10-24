#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "chat_helper.h"
#include <ctype.h>



#define ISVALIDSOCKET(s) ((s) >=0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)



int main(int argc , char *argv[]){
        
        if (argc != 2) {
        fprintf(stderr, "Usage: %s <port_number>\n", argv[0]);
        return 1;
             }
        const char *port = argv[1];
      
        port = argv[1];
        
        
        //GET server listening socket
        SOCKET listen_socket = create_socket(port);
        
        //INITIALIZE FD SOCKET SET , CHAT ADMIN STRUCT

       Chat_admin chat_admin = new_Chat_admin();
       FD_SET(listen_socket,&(chat_admin->socket_set));

        SOCKET max_socket = listen_socket;
        printf("Waiting for connections..\n");

        //BEGIN accepting and make select call

        while(1){
                fd_set reads = chat_admin->socket_set;
                
                struct timeval* timeout = get_timeout(chat_admin);
                if(select(max_socket+1,&reads,0,0,timeout)<0){
                         fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
                          return 1;             
                }
                process_incomplete_clients(chat_admin); //Kickout timed-out clients


                SOCKET i;
                for(i=1;i<=max_socket;i++){

                        if(FD_ISSET(i,&reads)){
                                
                                //New Client connecting
                                if(i == listen_socket){
                                        SOCKET new_client = get_newclient_socket(listen_socket);
                                         FD_SET(new_client, &(chat_admin->socket_set));
                                        
                                          if (new_client > max_socket){
                                          max_socket = new_client;
                                          }

                                }

                                else if(has_incomplete_message(i,chat_admin)){
                                        char *new_message = malloc(450);
                                        assert(new_message!=NULL);
                                        int bytes_received = recv(i,new_message,450,0);
                                        if(bytes_received<0){
                                                //error handling
                                                fprintf(stderr,"Error receiving message\n");
                                                drop_client(i,chat_admin);
                                        }
                                        else if(bytes_received == 0){
                                                // client closed socket , remove client
                                                drop_client(i,chat_admin);
                                        }
                                        else{
                                                // parse message , do the needful
                                                process_partial_message(i,chat_admin,new_message,bytes_received);
                                        }
                                        free(new_message);




                                }
                                





                                //Added client sending message , could be Hello message , add it to chat admin hash table here and register
                                else{
                                        char *new_message = malloc(450);
                                        assert(new_message!=NULL);
                                        int bytes_received = recv(i,new_message,450,0);
                                        if(bytes_received<0){
                                                //error handling
                                                fprintf(stderr,"Error receiving message\n");
                                                drop_client(i,chat_admin);
                                        }
                                        else if(bytes_received == 0){
                                                // client closed socket , remove client
                                                drop_client(i,chat_admin);
                                        }
                                        else{
                                                // parse message , do the needful
                                                parse_message(chat_admin,i,new_message,bytes_received);
                                        }
                                        free(new_message);


                                }
                        }
                }


        }

        return 0;






}