#include "common.h"

/* send_msg send a message on socket sock
   sock: the socket
   code: message's protocol code
   size: message's size
   msg: message to be sent
*/
int send_msg(int sock, Code code, unsigned char size, char *body) 
{
    //sending case error and prevention from sending a 0-long body
    if(send(sock, &code, CODESIZE, 0) == -1
    || send(sock, &size, CHARSIZE, 0) == -1)
    {
        PERROR("send_msg");
        return -1;
    }
    
    //if the body is nopt empty, sends it and catches error
    if(size != 0)
        if(send(sock, body, size*CHARSIZE, 0) == -1)
        {
            PERROR("send_msg");
            return -1;
        }
    
    return 0;
}

/* recv_msg recv a message from the socket sock
   sock: the socket
   code: message's protocol code
   size: message's size
   msg: message to be received
*/
int recv_msg(int sock, Code *code, unsigned char *size, char **body) 
{
    //receipt of the code and the size of data
    if(recv(sock, code, CODESIZE, 0) == -1
    || recv(sock, size, CHARSIZE, 0) == -1)
    {
        PERROR("recv_msg");
        return -1;
    }
    
    //memory allocation to receive data, needs a liberation outside
    *body = malloc((*size+1)*CHARSIZE);
    
    //if there is no data
    if(*size == 0)
    {
        //to ensure there is no segmentation fault if the body is read
        (*body)[0]='\0';
        return 0;
    }
    
    //receipt error case
    if(recv(sock, *body, *size, 0) < 0)
    {
        PERROR("recv_msg");
        return -1;
    }
    
    //adds the null character at the end to ensure the string is correctly prompted later
    (*body)[*size]='\0';
    
    return 0;
}
