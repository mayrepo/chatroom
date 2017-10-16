#include "common.h"
#include <signal.h>
#include <netdb.h>


char srv_name[] = "localhost";
int clt_sock;

int DFLAG;

void sig_handler(int s)
{
    
    switch (s) 
    {
      case SIGINT:
        send_msg(clt_sock, END_OK, 0, NULL);
        close(clt_sock);
        color("0");//reset color
        printf("\n");
        exit(EXIT_SUCCESS);
        
      case SIGSEGV:
        close(clt_sock);
        exit(0);
        
      default: return;
    }
}

/* Établie une session TCP vers srv_name sur le port srv_port
 * char *srv_name: nom du serveur (peut-être une adresse IP)
 * int srv_port: port sur lequel la connexion doit être effectuée
 *
 * renvoie: descripteur vers le socket
 */ 
int connect_to_server(char *srv_name, int srv_port){
    
    //creates of the socket
    int clt_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    //error case
    if(clt_sock == -1)
    {
        PERROR("creation du socket client");
        return -1;
    }
    
    struct hostent* host  = gethostbyname(srv_name);
    
    //creates a the address structure
    struct sockaddr_in address =
    {.sin_family = AF_INET,//host->haddrtype
     .sin_port = htons(srv_port),
     //.sin_addr.s_addr = (int)*(host->h_addr_list[0])
    };
    
    memcpy(&(address.sin_addr.s_addr), host -> h_addr_list[0], host -> h_length);
    
    
    //connects to the client at "address" through the client socket, catches error
    if (connect(clt_sock, (const struct sockaddr *)&address, ADDRSIZE) == -1)
    {
        PERROR("connection au serveur");
        return -1;
    }
    
    //return the socket
    return clt_sock;
}
    
int authenticate(int clt_sock){
    
    /* Code nécessaire à l'authentification auprès du serveur :
    
    - attendre un paquet AUTH_REQ
    
    - répondre avec un paquet AUTH_RESP
    
    - attendre un paquet ACCESS_OK / ACCESS_DENIED / AUTH_REQ
    
    - agir en conséquence ...
    
    */
    Code code;
    unsigned char size = 0;
    char *data = NULL;
    char login[BUFFSIZE];
    
    color("0");
    color("33");
    
    //receive message from the server
    recv_msg(clt_sock, &code, &size, &data);
    free(data);
    //error if AUTH_REQ not received
    if(code != AUTH_REQ)
    {
        PERROR("AUTH_REQ expeced");
        fprintf(stderr,"%d",code);
        fflush(stderr);
        
        return -1;
    }
    
    
    //while the authentification is requested
    do{
        color("33");
        
        printf("Enter login: ");
        fflush(stdout);
        color("31");
        
        //scans the login
        while(!fgets(login, BUFFSIZE, stdin)) {
            PERROR("fgets()");
        }
        
        //strtok(login, "\n");
        //adds the null character after the login instead of the newline character
        login[strcspn(login, "\n")] = '\0';
        
        //sends the login to the server
        send_msg(clt_sock, AUTH_RESP, strlen(login) + 1, login);
        
        
        
        //receives an answer
        recv_msg(clt_sock, &code, &size, &data);
        free(data);
        
        switch(code)
        {
          case ACCESS_OK:
            return 0;
            break;
          
          case ACCESS_DENIED:
            PERROR("access_denied");
            return -1;
            break;
          
          case AUTH_REQ:
            break;
          case BUSY:
            PERROR("busy");
            return -1;
          
          case END_OK:
            close(clt_sock);
            color("0");
            color("33");
            eprintf("connexion closed without problem\n");
            exit(EXIT_SUCCESS);
          
          default:
            PERROR("no answer");
            return -1;
        }
    }
    while(code == AUTH_REQ);
    
    return 0;
}

int instant_messaging(int clt_sock){
    
    fd_set rset;
    Code code;
    unsigned char size;
    char *data, *login;
    
    while(1)
    {
        FD_ZERO(&rset);
        FD_SET(clt_sock, &rset);
        FD_SET(STDIN_FILENO, &rset);
        
        
        if (select(clt_sock+1, &rset, NULL, NULL, NULL) < 0)
        {
            PERROR("select");
            exit(EXIT_FAILURE);
        }
        
        if (FD_ISSET(STDIN_FILENO, &rset))
        {
            //the user typed a message
            color("37");
            color("0");
            DEBUG("STDIN_FILENO isset");
            
            //allocates memory for the message
            data = malloc(BUFFSIZE);
            
            //
            if (fgets(data, BUFFSIZE, stdin) == NULL)
            {
                /* gérer feof et ferror */
                
                //<COMPLÉTER>
                
               return 0;
            }
            
            //replace the newline character by the null character
            data[strcspn(data, "\n")] = '\0';
            
            size = strlen(data)+1;
            
            //if the message is not just a newline
            if(size > 1)
            {
                DEBUG("sending MESG %s(%d)", data, size);
                //send the mesage to the server
                send_msg(clt_sock, MESG, size, data);
            }
            free(data);
            
        }
        
        if (FD_ISSET(clt_sock, &rset))
        {
        //message received from the server
            
            //receives the message and treats error
            if(recv_msg(clt_sock, &code, &size, &login) < 0)
            {
                PERROR("message");
                return -1;
            }
            
            //if the server sent END_OK, quit messaging
            if(code == END_OK)
                return 0;
            
            //else, LOGIN message expected, followed by a second message
            if(code != LOGIN || recv_msg(clt_sock, &code, &size, &data) < 0)
            {
                PERROR("message");
                return -1;
            }
            color("0");
            
            //treats the second message
            switch(code)
            {
              case MESG:
                //print the message
                color("31");
                printf("%s",login);
                color("37");
                printf(" : ");
                color("34");
                printf("%s\n", data);
                break;
                
              case CONNECTED:
                //print that a user is connected
                color("31");
                printf("%s",login);
                color("36");
                printf(" is already connected\n");
                break;
                
              case CONNECTS:
                //print that a user just connected
                color("31");
                printf("%s",login);
                color("36");
                printf(" is now connected\n");
                break;
                
              case DISCONNECTS:
                //print that a user just dicconnected
                color("31");
                printf("%s",login);
                color("36");
                printf(" is now disconnected\n");
                break;
                
              default:
                //code unexpected
                PERROR("message, wrong code : %d", code);
                return -1;
                break;
            }
            
            //standard color and free memory
            color("37");
            color("0");
            fflush(stdout);
            free(login);
            free(data);
        }
    
    } /* while (1) */
    
    return 0;
}

int main(int argc, char *argv[])
{
    // char srv_name[BUFFSIZE];
    int srv_port = SRV_PORT;
    
    DFLAG = 0;
    
    signal(SIGINT, sig_handler);
    
    //connects to the server and saves the socket
    clt_sock = connect_to_server(srv_name, srv_port);
    
    //error case
    if (clt_sock < 0)
        exit(EXIT_FAILURE);
    
    color("0");
    color("33");
    printf("Welcome to the chatroom !\n");
    fflush(stdout);
    
    //waits for identification, ensures the authentification is successful
    if (authenticate(clt_sock) < 0)
    {
        close(clt_sock);
        color("0");
        color("33");
        eprintf("connexion closed\n");
        exit(EXIT_FAILURE);
    }
    
    color("0");
    color("33");
    printf("Authentification ok.\nType a message and press enter to send.\n");
    fflush(stdout);
    
    //starts messaging with the server
    if (instant_messaging(clt_sock) < 0)
    {
        close(clt_sock);
        color("0");
        color("33");
        eprintf("connexion closed\n");
        exit(EXIT_FAILURE);
    }
    
    //close socket
    close(clt_sock);
    
    color("0");
    color("33");
    eprintf("connexion closed without problem\n");
    exit(EXIT_SUCCESS);
}
