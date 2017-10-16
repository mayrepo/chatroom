#include "common.h"
#include "chatroom.h"
#include <signal.h>
#include <pthread.h>
/*#include<sys/types.h>
#include<sys/socket.h>
#include <netinet/ip.h>*/

#define MAX_CONN 10            // nombre maximale de requêtes en attentes

int DFLAG;
int srv_sock;

void sig_handler(int s)
{
  switch (s) 
    {
      case SIGINT:
      
      //segmentation fault case to ensure the socket is closed in this case
      case SIGSEGV:
        stop_chat_room();
        int sock;
        sleep(1); /* wait for client to close */
        //close client sockets
        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            if ( (sock = get_client_socket(i)) != 0 )
            {
                close(sock);
            }
        }
        //close listening socket
        close(srv_sock);
        color(0);//reset color
        exit(0);
    
      case SIGPIPE:
        PERROR("broken pipe"); 
        break;
    
      default:
        break;
    }
}

int create_a_listening_socket(int srv_port, int maxconn)
{
    //creates a socket
    int srv_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    //error case
    if(srv_sock == -1)
    {
        PERROR("creation du socket serveur");
        return -1;
    }
    
    
    //initialise the socket address
    struct sockaddr_in address =
    {.sin_family = AF_INET,
     .sin_port = htons(srv_port),
     .sin_addr.s_addr = INADDR_ANY};
    
    //assigns the address to the socket
    if(bind(srv_sock, (const struct sockaddr *)&address, ADDRSIZE) == -1)
    {
        PERROR("bind");
        return -1;
    }
    
    //marks the socket as passive, i.e. able to accept incoming connections
    if(listen(srv_sock, maxconn)==-1)
    {
        PERROR("listen");
        return -1;
    }
    
    return srv_sock;
}

int accept_clt_conn(int srv_sock, struct sockaddr_in *clt_sockaddr)
{
    int clt_sock =-1;
    
    socklen_t addrsize = ADDRSIZE;
    
    
    //accepts the connexion request and creates a new connected socket specific to this client (clt_sock)
    clt_sock = accept(srv_sock, (struct sockaddr * restrict)clt_sockaddr, &addrsize);
    
    //error case
    if(clt_sock == -1)
    {
        PERROR("accept");
        return -1;
    }
    
    DEBUG("connexion accepted");
    return clt_sock;
}

int main(void)
{
    //defines the functions to call if a system signal is received
    signal(SIGINT, sig_handler);
    signal(SIGPIPE, sig_handler);
    signal(SIGSEGV, sig_handler);
    
    //if not null, debug mode
    DFLAG = 0;
    
    // creates a listening socket
    srv_sock = create_a_listening_socket(SRV_PORT, MAX_CONN);
    
    //error case
    if (srv_sock < 0) 
    {
        DEBUG("failed to create a listening socket");
        exit(EXIT_FAILURE);
    }
    
    // initializes the chat room with no client
    initialize_chat_room();
    
    int clt_sock;
    struct sockaddr_in clt_sockaddr;
    char *clt_ip;
    int clt_port;
    
    while (1)
    {
        
        // wait for new incoming connection
        if ((clt_sock = accept_clt_conn(srv_sock, &clt_sockaddr)) < 0 ) 
        {
            perror("accept_clt_conn");	
            exit(EXIT_FAILURE);
        }
        clt_ip = inet_ntoa(clt_sockaddr.sin_addr);
        clt_port = ntohs(clt_sockaddr.sin_port);
        
        // register new buddies in the chat room
        if ( login_chatroom(clt_sock, clt_ip, clt_port) != 0 ) 
        {
            DEBUG("client %s:%d not accepted", clt_ip, clt_port);	
            close(clt_sock);
            DEBUG("close clt_sock %s:%d", clt_ip, clt_port);
        }
        
    } /* while */
    int sock;
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if ( (sock = get_client_socket(i)) != 0 )
        { 
            close(sock);
        }
    }
    close(srv_sock);
    return EXIT_SUCCESS;
}
