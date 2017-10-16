#include "chatroom.h"
#include "common.h"
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#define MAX_AUTH_ATTEMPTS 3

//client structure
typedef struct 
{
    int sock;
    char login[BUFFSIZE];
    char ip[IP_LENGTH];
    int port;
} buddy_t;


pthread_t chatroom_id;

//array of clients
buddy_t chat_room[MAX_CLIENTS];

//current number of clients
int curr_nb_clients = 0;

/* new_client is a handler used to catch SIGUSR1 sent when a new
   client is registered in the chat room */
void new_client(int s) 
{
    switch (s)
    {
        case SIGUSR1: // new client signaled
          break;
        default:
          break;
    }
}

void initialize_chat_room()
{
    DEBUG("Initializing chat room");
    bzero(chat_room, sizeof(chat_room));
    
    /* using SIGUSR1 to signal new client
     (will interrupt select call in chatroom() )*/
    
    signal(SIGUSR1, new_client);
    
    // Creates the chat room
    pthread_create(&chatroom_id, NULL, &chatroom, NULL);
}

int broadcast_shutdown();

/* stop_chat_room() should be a safe function */
void stop_chat_room()
{
    /* Create the chat room */
    pthread_cancel(chatroom_id);
    broadcast_shutdown();
    
    pthread_join(chatroom_id, NULL);
}

int register_new_client(int sock, char login[], char *ip, int port) 
{
    int i = 0;
    
    DEBUG("registering client %s(%s:%d)", login, ip, port);
    
    /* find the first empty cell in chat_room */
    while (chat_room[i].sock != 0 && i < MAX_CLIENTS ) i++;
    
    
    if (i >= MAX_CLIENTS ) 
    { /* already too many clients */
        DEBUG("registration failed: already too many clients");
        send_msg(sock, BUSY, 0, NULL);
        
        return -1;
    }
    
    //save the new client
    chat_room[i].sock = sock;
    strncpy(get_client_login(i), login, BUFFSIZE);
    strncpy(chat_room[i].ip, ip, IP_LENGTH);
    chat_room[i].port = port;
    
    curr_nb_clients++;
    
    DEBUG("client %s(%s:%d) registered", get_client_login(i), get_client_ip(i), get_client_port(i));
    DEBUG("total number of registered clients: %d",curr_nb_clients );
    
    /* signal the new client in the chatroom */
    pthread_kill(chatroom_id, SIGUSR1);
    
    return curr_nb_clients;
}

int deregister_client(int sock) 
{
    int i = 0;
    
    /* find the first empty cell in chat_room */
    while (get_client_socket(i) != sock && i < MAX_CLIENTS ) i++;
    
    if (i >= MAX_CLIENTS ) 
    { /* client not found */
        DEBUG("deregistration failed: client not found");
        return -1;
    }
    
    close(sock);
    //set client i sock to 0
    chat_room[i].sock = 0;
    DEBUG("client %s,%s:%d deregistered", get_client_login(i), get_client_ip(i), get_client_port(i));
    
    return --curr_nb_clients;
}

int get_client_socket(int i) 
{
    if (i >= MAX_CLIENTS || i < 0 ) 
    {
        DEBUG("get client socket failed: wrong value %d", i);
        return -1;
    }
    
    return chat_room[i].sock;
}

char *get_client_login(int i) 
{
    if (i >= MAX_CLIENTS || i < 0 ) 
    {
        DEBUG("get client IP failed: wrong value %d", i);
        return NULL;
    }
    
    return chat_room[i].login;
}

char *get_client_ip(int i) 
{
    if (i >= MAX_CLIENTS || i < 0 ) 
    {
        DEBUG("get client IP failed: wrong value %d", i);
        return NULL;
    }
    
    return chat_room[i].ip;
}


int get_client_port(int i) 
{
    if (i >= MAX_CLIENTS || i < 0 ) 
    {
        DEBUG("get client port failed: wrong value %d", i);
        return -1;
    }
    
    return chat_room[i].port;
}

/* broadcast_shutdown send a END_OK message to all chat room clients
   This function is a safe function (could be called in a handler)
 */
int broadcast_shutdown() 
{
    int sock;
    
    if (curr_nb_clients == 0) 
    {
        return 2;
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if ( (sock = get_client_socket(i)) != 0 )
        { 
            send_msg(sock, END_OK, 0, NULL);
        }
    } // end for
    
    return 0;  
}

/* broadcast_msg: send a message to all chat room client
 */
int broadcast_msg(Code code, int size, char data[]) 
{
    int sock;
    
    DEBUG("broadcasting message \"%s\"", data);
    
    //to every client
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        //ensures the client exists and is connected
        if ( (sock = chat_room[i].sock) != 0 )
        {
            //sends the message
            if ( send_msg(sock, code, size, data) == -1) 
            {
                PERROR("erreur broadcast %s", get_client_login(i));
            }
        }
    }
    
    return 0;  
}

int broadcast_text(char *login, char *data)
{
    
    //broadcasts the login and the message, the formatting is left to the client
    broadcast_msg(LOGIN, strlen(login) + 1, login);
    broadcast_msg(MESG, strlen(data) + 1, data);
    
    color("31");
    printf("%s",login);
    color("37");
    printf(" : ");
    color("34");
    printf("%s\n", data);
    fflush(stdout);
    
    return 0;
}

/* gentication authenticate a new buddy and return her login
   clt_sock: client socket
   return a pointer to newly allocated string containing the login
 */
char* clt_authentication(int clt_sock)
{
    Code code;
    unsigned char size;
    char *login = NULL;
    
    //tries to connect at the most MAX_AUTH_ATTEMPTS times
    for(int attempt = 0; attempt < MAX_AUTH_ATTEMPTS; attempt++)
    {
        /* 
        authentification du client :
        
        - envoi du AUTH_REQ
        
        - lecture du AUTH_RESP
        
        - envoi du ACCESS_OK / ACCESS_DENIED ou nouveau tour de boucle
        
        - en cas de succès retourne un pointeur vers la chaîne de
         caractère contenant le login, sinon retourne NULL
        
        */
        
        //sends an authentification request
        send_msg(clt_sock, AUTH_REQ, 0, NULL);
        
        //receives the reply, treats errors
        if(recv_msg(clt_sock, &code, &size, &login) < 0)
        {
            PERROR("authetification");
            //retry to connect
            continue;
        }
        
        //if the reply is of type AUTH_RESP
        if(code == AUTH_RESP)
        {
            //sends message ACCESS_OK
            send_msg(clt_sock, ACCESS_OK, 0, NULL);
            return login;
        }
        
        //to prevent memory leak
        free(login);
    } /* for */
    
    //too many attempts, sends ACCESS_DENIED
    send_msg(clt_sock, ACCESS_DENIED, 0, NULL);
    return NULL;
}

int login_chatroom(int clt_sock, char *ip, int port)
{
    char *login;
    
    //if there are too many clients
    if ( curr_nb_clients == MAX_CLIENTS ) 
    {
        DEBUG("Already too many clients");
        DEBUG("%s:%d refused", ip, port);
        
        //sends busy message
        if ( send_msg(clt_sock, BUSY, 0, NULL) == -1 ) 
        {
            PERROR("Sending BUSY failed");
            return -1;
        }
        
        return -1;
    }
    
    // authenticate the connected client
    login = clt_authentication(clt_sock);
    
    //error case
    if ( login == NULL)
    {
        DEBUG("authentication failed for %s:%d", ip, port);      
        return -1;
    }
    
    DEBUG("client %s(%s:%d) authenticated", login, ip, port);
    
    //to every client
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        //ensure the client exists and is connected
        if ( get_client_socket(i) != 0 )
        {
            //sends the login in a LOGIN message then the CONNECTED message
            if ( send_msg(clt_sock, LOGIN,strlen(get_client_login(i)) + 1, get_client_login(i))==-1 || send_msg(clt_sock, CONNECTED, 0, NULL) == -1) 
            {
                PERROR("error connected %s", get_client_login(i));
            }
        }
    }
    
    //registers the clients in the chat room
    if ( register_new_client(clt_sock, login, ip, port) <=0 )
    {
        DEBUG("registration failed for %s(%s:%d)", login, ip, port);
        free(login);
        return -1;
    }
    
    //client registered
    DEBUG("client %s(%s:%d) logged in", login, ip, port);
    broadcast_msg(LOGIN, strlen(login) + 1, login);
    broadcast_msg(CONNECTS, 0, NULL);
    color("31");
    printf("%s",login);
    color("36");
    printf(" is now connected\n");
    free(login);
    
    return 0;
}

void *chatroom()//void *arg)
{
    Code code;
    unsigned char size;
    char* data = NULL;
    
    fd_set rset;
    int clt_sock;
    int nfds;
        
    while ( 1 ) 
    {
        
        FD_ZERO(&rset);
        /* FD_SET(p[0], &rset); */
        /* nfds = p[0]; */
        
        //adding clients' sockets to rset
        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            if ( (clt_sock = get_client_socket(i)) != 0 )
            { /* client i */
                DEBUG("adding %s to rset", get_client_login(i));
                
                FD_SET(clt_sock, &rset);
                if (FD_ISSET(clt_sock, &rset)){
                    DEBUG("%s(%d) is in rset", get_client_login(i), clt_sock);
                }
                else 
                {
                    DEBUG("%s(%d) is not in rset", get_client_login(i), clt_sock);
                } 
                nfds = clt_sock > nfds ? clt_sock: nfds;
            }
        }
        
        if (select(nfds+1, &rset, NULL, NULL, NULL) <= 0)
        {
            if ( errno == EINTR )
            {
                DEBUG("select interrupted");
                continue;
            }
            PERROR("select");
            return NULL;
        }
        
        //from every client
        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            clt_sock = get_client_socket(i);
            
            //ensure the client exists and is connected
            if ( clt_sock == 0 )
                continue;
            
            if (!FD_ISSET(clt_sock, &rset))
                continue;
            
            //receives message
            if(recv_msg(clt_sock, &code, &size, &data)<0)
            {
                PERROR("message not received");
            }
            DEBUG("%d message %s received by %s", code, data, get_client_login(i));
            
            //char *login = malloc(CHARSIZE*(strlen(get_client_login(i)) + 2 +strlen(data)));
            
            
            //depending of the code of the received message
            switch(code)
            {
              case MESG:
                
                //broadcast the mesage
                broadcast_text(get_client_login(i), data);
                break;
            
              default:
                
                if(code != END_OK)
                    //unexpected code
                    DEBUG("code : %d, %d or %d expected", code, MESG, END_OK);
                
                //deregisters client i
                deregister_client(get_client_socket(i));
                
                //warns other clients
                broadcast_msg(LOGIN, strlen(get_client_login(i)) + 1, get_client_login(i));
                broadcast_msg(DISCONNECTS, 0, NULL);
                
                //print
                color("31");
                printf("%s", get_client_login(i));
                color("36");
                printf(" is now disconnected\n");
                break;
            }
            free(data);
        }
    
    } /* while (1) */
    
    return NULL;
}
