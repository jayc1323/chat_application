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
#include <table.h>
#include <seq.h>
#include <set.h>
#include <sys/time.h>

#define ISVALIDSOCKET(s) ((s) >=0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)



typedef struct Chat_admin* Chat_admin;
typedef struct client_info* Client_info;
typedef struct client_message* Client_message;
struct Chat_admin{
        
        fd_set socket_set;
        Seq_T client_list;
        int num_clients;
        fd_set incomplete_sockets;

};

struct __attribute__((packed)) Message {
    uint16_t message_type;       
    char source[20];     
    char destination[20]; 
    uint32_t length;     
    uint32_t message_id;       
};

struct client_message{
        short type;
        char source[20];
        char destination[20];
        int data_length;
        int data_received;
        int id;
        char data[400];
        bool isComplete;
};
struct client_info {
   
    SOCKET socket;
    Client_message message;
    char client_ID[20];
    struct timeval timeout;
    bool timeout_set;
    struct timeval time_left;
    
    
    
};

Chat_admin new_Chat_admin();
SOCKET create_socket(const char *port);
SOCKET get_newclient_socket(SOCKET listen_socket);
void parse_message(Chat_admin ca ,SOCKET client_socket, char *message,int bytes);
void process_partial_message(SOCKET client_socket,Chat_admin ca,char *message,int bytes_received);
bool has_incomplete_message(SOCKET client,Chat_admin ca);
void process_incomplete_clients(Chat_admin ca);
struct timeval* get_timeout(Chat_admin ca);
void drop_client(SOCKET client_socket,Chat_admin ca);