#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "chat_helper.h"

#define ISVALIDSOCKET(s) ((s) >=0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

unsigned hash_string(const void *key) {
    const char *str = key;
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  
    }
    return hash;
}

int compare_string(const void *x, const void *y) {
    return strcmp((const char *)x, (const char *)y);
}

void drop_registered_client(Chat_admin ca,SOCKET client);

Client_info get_ci_from_sd(SOCKET client_socket,Chat_admin ca){
         Seq_T seq = ca->client_list;
       
        int length = Seq_length(seq);
        Client_info ci = NULL;
        for(int i = 0;i<length;i++){
                Client_info di = Seq_get(seq,i);
                 //fprintf(stderr,"Comparing with clientID '%s'\n",ci->client_ID);
                 if(di!=NULL){
                if(di->socket == client_socket){
                       // found = true;
                       ci = di;
                        break;
                }
                 }

        }
        //assert(ci);
        return ci;

}

bool has_incomplete_message(SOCKET client,Chat_admin ca){
        Client_info ci = get_ci_from_sd(client,ca);
        if(ci==NULL){
                //unregistered client
                return false;
        }
        return ci->timeout_set;

}

void process_incomplete_clients(Chat_admin ca){
         Seq_T seq = ca->client_list;
        int length = Seq_length(seq);
        struct timeval curr_time;
        gettimeofday(&curr_time,NULL);
        
        for(int i = 0;i<length;i++){
                Client_info di = Seq_get(seq,i);
                 
                 if(di!=NULL && di->timeout_set){

                        if(curr_time.tv_sec >= di->timeout.tv_sec){
                                
                                drop_registered_client(ca,di->socket);
                                fprintf(stderr,"Client timed out\n");
                        }
                        else{
                                di->time_left.tv_sec = (di->timeout.tv_sec)-(curr_time.tv_sec);
                        }
                
                 }

}
return;

}

struct timeval* get_timeout(Chat_admin ca){
        bool set = false;
        struct timeval *tout = malloc(sizeof(*tout));
        tout->tv_sec = 61;
          Seq_T seq = ca->client_list;
       
        int length = Seq_length(seq);
     
        for(int i = 0;i<length;i++){
                Client_info ci = Seq_get(seq,i);
                 
                 if(ci!=NULL){
                        if(ci->timeout_set){
                                if(ci->time_left.tv_sec < tout->tv_sec){
                                        tout->tv_sec = ci->time_left.tv_sec;
                                        set = true;
                                }
                        }
                
                 }


}
if(set){
        return tout;
}

return NULL;
}
Client_info get_client_struct(char* name,Chat_admin ca){

       // fprintf(stderr,"Client ID looking for is '%s'\n",name);
        Seq_T seq = ca->client_list;
        //bool found = false;
        int length = Seq_length(seq);
        Client_info ci = NULL;
        for(int i = 0;i<length;i++){
                Client_info di = Seq_get(seq,i);
                 //fprintf(stderr,"Comparing with clientID '%s'\n",ci->client_ID);
                 if(di!=NULL){
                if(memcmp(name,di->client_ID,20)==0){
                       // found = true;
                       ci = di;
                        break;
                }
                 }

        }
//        // assert(found);
//        if(ci == NULL){
//         fprintf(stderr,"No match found\n");
//        }
//        else{
//         fprintf(stderr,"Found Client with clientID %s\n",ci->client_ID);
//        }
        return ci;
}

void process_partial_message(SOCKET client_socket,Chat_admin ca,char *message,int bytes_received){

        // IF MESSAGE IS COMPLETE , ADD TO BUFFER AND PROCESS IT
        Client_info ci = get_ci_from_sd(client_socket,ca);
        Client_message cm = ci->message;
        assert(bytes_received<= (cm->data_length)-(cm->data_received));
        memcpy(cm->data+cm->data_received,message,bytes_received);
        cm->data_received +=bytes_received;
        if(cm->data_received == cm->data_length){
                //PROCESS CHAT MESSAGE and remove from Incomplete fd set
                FD_CLR(client_socket,&(ca->incomplete_sockets));
                 fprintf(stderr,"sending message\n");
                struct Message *chat_message = malloc(sizeof(*chat_message));
                chat_message->message_type = htons(5);
                memcpy(chat_message->source,cm->source,20);     
                memcpy(chat_message->destination,cm->destination,20); 
                chat_message->length = htonl(cm->data_length);
                chat_message->message_id = htonl(cm->id);
                char message_data[cm->data_length];
                memcpy(message_data,cm->data,cm->data_length);    
                Client_info di = get_client_struct(chat_message->destination,ca);   
                ssize_t bytes_sent = send(di->socket, chat_message, sizeof(*chat_message), 0);
                if (bytes_sent < 0) {
                   perror("send() failed");
        } else {
                printf("Chat header sent to destination %s\n", chat_message->destination);

               }
               ssize_t bytes_sent2 = send(di->socket, message_data, cm->data_length, 0);
               if (bytes_sent2 < 0) {
                   perror("send() failed");
        } else {
                printf("Chat message sent to destination %s\n", chat_message->destination);

               }


               free(chat_message);
               free(cm);
               ci->timeout_set = false;
               return;

        }

        //IF STILL INCOMPLETE , ADD TO BUFFER AND SET NEW TIMEOUT VAL
        else{   struct timeval curr_time;
                gettimeofday(&curr_time,NULL);
                ci->time_left.tv_sec = ci->timeout.tv_sec - curr_time.tv_sec;
    ci->time_left.tv_usec = ci->timeout.tv_usec - curr_time.tv_usec;

    
    if (ci->time_left.tv_usec < 0) {
        ci->time_left.tv_sec -= 1;
        ci->time_left.tv_usec += 1000000;
    }

    if (ci->time_left.tv_sec < 0) {
        ci->time_left.tv_sec = 0;
        ci->time_left.tv_usec = 0;
    }


        }

                  return;        

}



Chat_admin new_Chat_admin(){

        Chat_admin ca = malloc(sizeof(struct Chat_admin));
        if(ca==NULL){
                fprintf(stderr,"Alloc for chat admin failed.\n");
                exit(1);
        }
        ca->num_clients = 0;
        FD_ZERO(&ca->socket_set);
        FD_ZERO(&ca->incomplete_sockets);
       
      
       // ca->id_to_client =Table_new(100, compare_string, hash_string);
        ca->client_list = Seq_new(10);
        
        
       
       
       
        return ca;

}

SOCKET create_socket(const char *port) {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, port, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen;
}

SOCKET get_newclient_socket(SOCKET listen_socket){

        struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof(client_address);
                    SOCKET socket_client = accept(listen_socket,
                            (struct sockaddr*) &client_address,
                            &client_len);
                    if (!ISVALIDSOCKET(socket_client)) {
                        fprintf(stderr, "accept() failed. (%d)\n",
                                GETSOCKETERRNO());
                        exit(1);
                    }

                   

                    char address_buffer[100];
                    getnameinfo((struct sockaddr*)&client_address,
                            client_len,
                            address_buffer, sizeof(address_buffer), 0, 0,
                            NI_NUMERICHOST);
                    printf("New connection from %s\n", address_buffer);

                    return socket_client;
}


void drop_unregistered_client(Chat_admin ca,SOCKET client){

       FD_CLR(client,&ca->socket_set);
       CLOSESOCKET(client);
       fprintf(stderr,"Client socket closed%d\n",client);
       return; 

}

void drop_registered_client(Chat_admin ca,SOCKET client){

        int length = Seq_length(ca->client_list);
        Client_info ci = NULL;
        bool exists = false;
        for(int i = 0;i<length;i++){
                          ci = (Client_info)Seq_get(ca->client_list,i);
                         if(ci){
                                if(client == ci->socket){
                                        exists = true;
                                        Seq_put(ca->client_list,i,NULL);
                                        break;
                                }
                         }

                    }
         FD_CLR(client,&ca->socket_set);            

        assert(exists);
       // assert(Table_get(ca->id_to_client,ci->client_ID)!=NULL);
        //Table_put(ca->id_to_client,ci->client_ID,NULL);
        ca->num_clients--;
        CLOSESOCKET(client);
        if(ci->message!=NULL){
                free(ci->message);
        }
        free(ci);
        ci = NULL;
         fprintf(stderr,"Client socket closed%d\n",client);



}

void drop_client(SOCKET client,Chat_admin ca){
        Client_info ci = get_ci_from_sd(client,ca);
        if(ci==NULL){
                drop_unregistered_client(ca,client);
        }
        else{
                drop_registered_client(ca,client);
        }
        return;
}

char* build_client_list(Chat_admin ca){

          //BUILD CLIENT ID LIST
           char *client_list = malloc(400);
           assert(client_list);
          int pos = 0;  

int length = Seq_length(ca->client_list);  

for (int i = 0; i < length; i++) {
    Client_info ci = (Client_info)Seq_get(ca->client_list, i);
    
    if (ci && ci->client_ID) {  
        int id_len = strlen(ci->client_ID);  

        
        strncpy(&client_list[pos], ci->client_ID, id_len);
        pos += id_len;  
        
    }
}
client_list[pos] = '\0';
  return client_list;



}

void send_hello_ack(SOCKET client_socket,char* clientID){

        struct Message hello_ack;

    hello_ack.message_type = htons(2);
    strncpy(hello_ack.source, "Server\0", sizeof(hello_ack.source) - 1);
    hello_ack.source[sizeof(hello_ack.source) - 1] = '\0'; 

    strncpy(hello_ack.destination, clientID, sizeof(hello_ack.destination) - 1);
    hello_ack.destination[sizeof(hello_ack.destination) - 1] = '\0'; 

    hello_ack.length = htonl(0); 
    hello_ack.message_id = htonl(0); 
    //memset(hello_ack.data, 0, sizeof(hello_ack.data)); 

    
    ssize_t bytes_sent = send(client_socket, &hello_ack, sizeof(hello_ack), 0);
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending hello ack message: %s\n", strerror(GETSOCKETERRNO()));
        
    }
    else{
        fprintf(stderr,"Hello ack sent\n");
    }
    return;
              
              




           
}

void send_client_list(Chat_admin ca,SOCKET client_socket,char* ClientID){

    char *client_list = build_client_list(ca);
    struct Message cl_message;
    int data_length = strlen(client_list);
              
    cl_message.message_type = htons(4);  
    strncpy(cl_message.source, "Server\0", sizeof(cl_message.source) - 1);
    cl_message.source[sizeof(cl_message.source) - 1] = '\0';  

    strncpy(cl_message.destination, ClientID, sizeof(cl_message.destination) - 1);
    cl_message.destination[sizeof(cl_message.destination) - 1] = '\0';  

    cl_message.length = htonl(data_length);  
    cl_message.message_id = htonl(0);  

    //memcpy(cl_message.data, client_list, data_length);
    //cl_message.data[sizeof(cl_message.data) - 1] = '\0';  

    
    ssize_t bytes_sent = send(client_socket, &cl_message, sizeof(cl_message), 0);
    if (bytes_sent < 0) {
        perror("send() failed");
    } else {
        printf("Client list msg header sent to client %s\n", ClientID);
    }


    // Free the dynamically allocated client list
    
    fprintf(stderr,"Sending message type: %d, source: %s, destination: %s, length: %d, message_id: %d\n", 
    ntohs(cl_message.message_type), cl_message.source, cl_message.destination, ntohl(cl_message.length), ntohl(cl_message.message_id));
    ssize_t bytes_sent2 = send(client_socket, client_list, data_length, 0);
    if (bytes_sent2 < 0) {
        perror("send() failed");
    } else {
        printf("Client list sent to client %s\n", ClientID);
    }
    
    free(client_list);
              



}

void send_error_id_taken(SOCKET client_socket){

        struct Message id_taken;

    id_taken.message_type = htons(7);
    strncpy(id_taken.source, "Server\0", sizeof(id_taken.source) - 1);
   id_taken.source[sizeof(id_taken.source) - 1] = '\0'; 

    strncpy(id_taken.destination, "Invalid Client", sizeof(id_taken.destination) - 1);
   id_taken.destination[sizeof(id_taken.destination) - 1] = '\0'; 

    id_taken.length = htonl(0); 
   id_taken.message_id = htonl(0); 
    //memset(hello_ack.data, 0, sizeof(hello_ack.data)); 

    
    ssize_t bytes_sent = send(client_socket, &id_taken, sizeof(id_taken), 0);
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending ID taken message: %s\n", strerror(GETSOCKETERRNO()));
        
    }
    else{
        fprintf(stderr,"ID taken error msg sent\n");
    }
    return;
              


}

void parse_message(Chat_admin ca ,SOCKET client_socket, char *message,int bytes_recv){
        //STEP 1 : Get ClientID , Check if it should be a hello Msg
        short message_type = *(short*)message;
        message_type = ntohs(message_type);
       char clientID[20];
                char destination[20];

                // Initialize both arrays to null
                memset(clientID, 0, sizeof(clientID));
                memset(destination, 0, sizeof(destination));

                // Copy from message and ensure null-termination
                memcpy(clientID, message + 2, sizeof(clientID));
                memcpy(destination, message + 22, sizeof(destination));

                // Ensure bytes after the first null terminator are set to null
                for (int i = 0; i < 20; i++) {
                if (clientID[i] == '\0') {
                        fprintf(stderr,"Null terminator is at %d\n",i);
                        memset(clientID + i + 1, 0, sizeof(clientID) - i - 1);
                        break;
                }
                }

                for (int i = 0; i < 20; i++) {
                if (destination[i] == '\0') {
                         fprintf(stderr,"Null terminator is at %d\n",i);
                        memset(destination + i + 1, 0, sizeof(destination) - i - 1);
                        break;
                }
                }

                fprintf(stderr, "The client is %s\n", clientID);
                fprintf(stderr, "The destination is %s\n", destination);
                        fprintf(stderr, "The message type is %d\n", message_type);
                        fprintf(stderr, "The client socket is %d\n", client_socket);
        
        int length = *(int*)(message+42);
        length = ntohl(length);

        int msg_id = *(int*)(message+46);
        msg_id = ntohl(msg_id);


        // IF MESSAGE TYPE == 1 , IT SHOULD BE FROM A NEW CLIENT(CLIENT ID IS NOT IN HASH TABLE) , ELSE DISCONNECT
        if(message_type == 1){
                    
                    //Check if in registered sequence
                    bool socket_exists = false;
                    int length = Seq_length((ca->client_list));
                    for(int i = 0;i<length;i++){
                         Client_info ci = (Client_info)Seq_get(ca->client_list,i);
                         if(ci){
                                if(client_socket == ci->socket){
                                        socket_exists = true;
                                }
                         }

                    }

                    // IF SOCKET EXISTS , REMOVE CLIENT
                    if(socket_exists){

                        //DROP CLIENT
                        fprintf(stderr,"Same connection\n");
                        drop_registered_client(ca,client_socket);
                        return;
                    }
                    if(strcmp(clientID,"Server\0")==0){
                        drop_unregistered_client(ca,client_socket);
                        return;
                        //CANT KEEP CLIENT ID AS SERVER
                    }
                    if(strcmp(destination,"Server\0")!=0){
                        drop_unregistered_client(ca,client_socket);
                        return;
                        //CANT KEEP destination AS not SERVER
                    }

                    


                    //IF SOCKET DOES NOT EXIST , CHECK IF CLIENTID IS TAKEN OR NOT , IF NOT TAKEN - REGISTER CLIENT , ELSE - Send error msg & disconnect
                    bool id_taken = false;
                    Client_info existing_client = get_client_struct(clientID,ca);
                    if(existing_client != NULL){
                        fprintf(stderr,"The client ID key entered is %s\n",clientID);
                        id_taken = true;
                        //Client_info thief = Table_get(ca->id_to_client,clientID);
                        fprintf(stderr,"existing client with same ID is %s\n",existing_client->client_ID);
                         fprintf(stderr,"existing client socket is %d\n",existing_client->socket);
                    }
                    if(id_taken){
                        //disconnect client and send error message
                        //send message
                        fprintf(stderr,"id taken\n");
                        // send error message id taken
                        send_error_id_taken(client_socket);
                        drop_unregistered_client(ca,client_socket);


                        return;
                    }
                    else{
                        // REGISTER CLIENT
                      Client_info new_client = malloc(sizeof(struct client_info));
                        new_client->socket = client_socket;
                        memcpy(new_client->client_ID, clientID, sizeof(new_client->client_ID));
                       
                        Seq_addhi(ca->client_list, (void*)new_client);
                      
                        ca->num_clients++;
                        fprintf(stderr, "Client registered\n");

                        //SEND ACK
                 send_hello_ack(client_socket,new_client->client_ID);
                 send_client_list(ca,client_socket,new_client->client_ID);
                 return;

                    }



        }
        
        //LIST REQUEST MESSAGE , SHOULD BE FROM EXISTING CLIENT
       else if(message_type == 3){
                Client_info ci = get_client_struct(clientID,ca);
                
                if(ci == NULL || ci->socket != client_socket){
                        //DROP CLIENT
                        if(ci == NULL){
                                //fprintf(stderr,"Here1\n");
                                drop_unregistered_client(ca,client_socket);
                                return;
                        }
                        //fprintf(stderr,"Here2\n");
                        drop_registered_client(ca,client_socket);
                        return;
                        

                        
                }
                send_client_list(ca,client_socket,clientID);
                return;
        }

        //CHAT MESSAGE
        else if(message_type == 5){


                //IF CLIENT NOT REGISTERED , DISCONNECT SOCKET
                  Client_info ci = get_client_struct(clientID,ca);//Table_get(ca->id_to_client,clientID);
                if(ci == NULL){
                        fprintf(stderr,"Here1\n");
                        drop_unregistered_client(ca,client_socket);
                        return;
                }  
                // ALSO CHECK IF SOCKET MATCHES REGISTERED SOCKET FOR THE GIVEN ID , A CLIENT COULD SEND A MSG WITH ANOTHER'S ID
                if(ci->socket != client_socket){
                        drop_registered_client(ca,client_socket);
                }

                Client_info di =  get_client_struct(destination,ca);//Table_get(ca->id_to_client,destination);

                //CANT SEND MSG TO ITSELF
                if(ci==di){
                        drop_registered_client(ca,client_socket);
                        return;
                }
                
                

                //IF DESTINATION DNE , RETURN ERROR MESSAGE
                if(di == NULL){

                        fprintf(stderr,"destination for chat msg is not in client list\n");
                        //ERROR TO CI
                         struct Message error_message;

    error_message.message_type = htons(8);
    strncpy(error_message.source, "Server\0", sizeof(error_message.source) - 1);
    error_message.source[sizeof(error_message.source) - 1] = '\0'; 

    strncpy(error_message.destination, clientID, sizeof(error_message.destination) - 1);
    error_message.destination[sizeof(error_message.destination) - 1] = '\0'; 

    error_message.length = htonl(0); 
    error_message.message_id = htonl(msg_id); 
    //memset(error_message.data, 0, sizeof(error_message.data));
     ssize_t bytes_sent = send(client_socket, &error_message, sizeof(error_message), 0);
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending Error message: %s\n", strerror(GETSOCKETERRNO()));
        
    }
    drop_registered_client(ca,client_socket);

                        return;
                }
                fprintf(stderr,"Bytes recevied from message:%d\n",bytes_recv);
                fprintf(stderr,"Message length %d\n",length);

                //IF INCOMPLETE MSG RECEIVED
                if((bytes_recv-50)<(length)){
                        Client_message cm = ci->message;
                        cm = malloc(sizeof(*cm));
                        cm->data_received = bytes_recv-50;
                        cm->data_length = length;
                        cm->id = msg_id;
                         memcpy(cm->source,clientID,20);
                         memcpy(cm->destination,destination,20);
                        cm->isComplete = false;
                        
                        memcpy(cm->data,message+50,cm->data_received);
                        ci->timeout_set = true;
                        ci->time_left.tv_sec=60;
                        ci->time_left.tv_usec=0;
                        struct timeval curr_time;
                        gettimeofday(&curr_time,NULL);
                        ci->timeout.tv_sec = curr_time.tv_sec + 60;
                        ci->timeout.tv_usec = curr_time.tv_usec ;
                        fprintf(stderr,"Timeout set\n");
                        return;

                }


                //ELSE
                fprintf(stderr,"sending message\n");
                struct Message *chat_message = malloc(sizeof(*chat_message));
                chat_message->message_type = htons(5);
                memcpy(chat_message->source,message+2,20);     
                memcpy(chat_message->destination,message+22,20); 
                chat_message->length = htonl(length);
                chat_message->message_id = htonl(msg_id);
                char message_data[length];
                memcpy(message_data,message+50,length);       
                ssize_t bytes_sent = send(di->socket, chat_message, sizeof(*chat_message), 0);
                if (bytes_sent < 0) {
                   perror("send() failed");
        } else {
                printf("Chat header sent to destination %s\n", destination);

               }
               ssize_t bytes_sent2 = send(di->socket, message_data, length, 0);
               if (bytes_sent2 < 0) {
                   perror("send() failed");
        } else {
                printf("Chat message sent to destination %s\n", destination);

               }


               free(chat_message);
               return;
        }

        else if(message_type == 6){

                  Client_info ci = get_client_struct(clientID,ca);
                if(ci == NULL){
                        drop_unregistered_client(ca,client_socket);
                        return;
                }
                drop_registered_client(ca,client_socket);
                return;


        }
        else{

                  Client_info ci = get_client_struct(clientID,ca);
                if(ci == NULL){
                        drop_unregistered_client(ca,client_socket);
                        return;
                }
                drop_registered_client(ca,client_socket);
                return;




        }



       
        


}


