/*
 * Proxy Lab: 
 * 1. Implement a sequential web proxy
 * 2. Implement the concurrent proxy program using threads
 * 3. Implement the cache
 *
 * Andrew ID: jiayuem
 * Name: Jiayue Mao
 */

#include "csapp.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>

#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/*
 * Debug macros, which can be enabled by adding -DDEBUG in the Makefile
 * Use these if you find them useful, or delete them if not
 */
#ifdef DEBUG
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_assert(...)
#define dbg_printf(...)
#endif

/* the whole cache (linked list) */
Cache cache;

/* mutex used to lock the cache */
pthread_mutex_t mutex;

/*
 * Strings to use for the different kinds of headers.
 */
static const char *header_user_agent = "User-Agent: Mozilla/5.0"
                                    " (X11; Linux x86_64; rv:3.10.0)"
                                    " Gecko/20191101 Firefox/63.0.1\r\n";
static const char *connection_header = "Connection: close\r\n";
static const char *proxy_connection_header = "Proxy-Connection: close\r\n";
static const char *requestline_header = "GET %s HTTP/1.0\r\n";
static const char *end_header = "\r\n";

/* 
 * Strings acted as keys in different kinds of headers. 
 */
static const char *host_key = "Host";
static const char *connection_key = "Connection";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *user_agent_key = "User-Agent";

/* 
 * Self-defined functions 
 */
void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, char *port);
void build_http_header(char *hostname, char *path, 
	                   char *port, rio_t *client_rio, char *http_header);
void *thread(void *vargo);
void sigpipe_handler(int sig);

/*
 * Self-defined wrapper functions for error handling
 */
int Open_listenfd(char *port);
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags);
void Close(int fd);
int Rio_writen(int fd, void *usrbuf, size_t n);
void Rio_readinitb(rio_t *rp, int fd);
ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

/* 
 * main - the main function used to connect to client
 * and use threads to connect to resolve the client requests.
 * args: int argc, char** argv
 */
int main(int argc, char** argv) {

	int listenfd;
	int *connfd;
	struct sockaddr_in clientaddr;
	char hostname[MAXLINE], port[MAXLINE];
	pthread_t tid;

	// check command line args
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	
	listenfd = Open_listenfd(argv[1]); // argv[1] => port number
	Signal(SIGPIPE, sigpipe_handler);
	init_cache();

	while (1) {
		socklen_t clientlen;
		clientlen = sizeof(clientaddr);

		/* accept a client */
		connfd = malloc(sizeof(int));
		if (connfd == NULL) {
			continue;
		}
		if ((*connfd = accept(listenfd, (SA *)&clientaddr, &clientlen)) < 0) {
			sio_printf("Proxy cannot accept a client.\n");
			continue;
		}

		/* get client hostname and port from clientaddr */
		Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		sio_printf("Connection from (%s, %s).\n", hostname, port);
		pthread_create(&tid, NULL, thread, connfd);
		
	}
    
    return 0;
}

/* thread - thr function that each thread execute
 * args: void *vargo 
 * return: none
 */
void *thread(void *vargo) {
	int connfd = *((int *) vargo);
	pthread_detach(pthread_self());
	free(vargo);
    doit(connfd);
    Close(connfd);
    return NULL;
}

/* doit - for each thread to finish their work for client.
 * args: int connfd - the file descriptor of client
 * return: none
 */
void doit(int connfd) {
	char buf[MAX_OBJECT_SIZE];
	char method[MAXLINE], port[MAXLINE], hostname[MAXLINE];
	char path[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char http_header[MAXLINE];
	rio_t client_rio, server_rio;
	int serverfd;

	/* read request line from client */
	Rio_readinitb(&client_rio, connfd);
	Rio_readlineb(&client_rio, buf, MAXLINE);
	/* convert request line to method, uri, version */
	sscanf(buf, "%s %s %s", method, uri, version);

	/* if method is not GET, print error message */
	if (strcasecmp(method, "GET")) {
		sio_printf("Proxy does not implement this method.\n");
		return;
	}

	/* find if the cache include the url */
	pthread_mutex_lock(&mutex);
	cache_block *find_block = find_cache(uri);
	pthread_mutex_unlock(&mutex);

	/* found - read the content from the block */
	if (find_block != NULL) {
		read_from_cache(find_block, connfd);
	} else {
		/* transform uri into hostname, path and port */
		parse_uri(uri, hostname, path, port);

		/* build http header */
		build_http_header(hostname, path, port, &client_rio, http_header);

		/* connect to server */
		serverfd = open_clientfd(hostname, port);
		if (serverfd < 0) {
			sio_printf("connection to server fails.\n");
			return;
		}

		/* send header to server */
		Rio_readinitb(&server_rio, serverfd);
		if (Rio_writen(serverfd, http_header, strlen(http_header)) < 0) {
			return;
		}

		int n;
		char cache_buf[MAX_CACHE_SIZE];
		char *buf_pointer = cache_buf;
		size_t size_count = 0;
		while ((n = rio_readnb(&server_rio, buf, MAX_OBJECT_SIZE)) > 0) {
			size_count += n;
			memcpy(buf_pointer, buf, n);
			buf_pointer += n;
		}

		/* find the url in cache again to make sure (for unique tests) */
		pthread_mutex_lock(&mutex);
		find_block = find_cache(uri);
		pthread_mutex_unlock(&mutex);

		if (find_block == NULL) { // not found
			rio_writen(connfd, cache_buf, size_count); // write to client
			/* if the content size less than MAX_OBJECT_SIZE, write to cache */
			if (size_count < MAX_OBJECT_SIZE) {
				pthread_mutex_lock(&mutex);
				find_cache_to_write(cache_buf, uri, size_count);
				pthread_mutex_unlock(&mutex);
			}
		} else {
			read_from_cache(find_block, connfd);
		}

		//print_cache();
		/* close server file descriptor */
		Close(serverfd);
	}

}

/* parse_uri - parse thr uri into hostname, path and port
 * args: 
 * char *uri - the uri to be parsed
 * char *hostname - the string of hostname to be writen
 * char *path - the string of path to be writen
 * char *port - the string of port to be writen
 * return: none
 */
void parse_uri(char *uri, char *hostname, char *path, char *port) {

	int i = 0;
	char *hostname_pos, *port_pos, *path_pos;

	/* extract where the hostname start as */
	hostname_pos = strstr(uri, "//");
	if (hostname_pos == NULL) {
		hostname_pos = uri;
	} else {
		hostname_pos = hostname_pos + 2;
	}

	/* iterate through uri until encourtering 
	 * '/' or ':' where the hostname ends */
	for (i = 0; i < strlen(hostname_pos); i++) {
		hostname[i] = hostname_pos[i];
		if (hostname_pos[i] == '/' || hostname_pos[i] == ':') {
			hostname[i] = '\0';
			break;
		}
	}

	/* extrace port from uri */
	hostname_pos = strstr(uri, "//");
	if (hostname_pos == NULL) {
		hostname_pos = uri;
	} else {
		hostname_pos = hostname_pos + 2;
	}
	port_pos = strstr(hostname_pos, ":");
	/* if there's no port, it's set to 80 by default. */
	if (port_pos == NULL) {
		port[0] = '8';
		port[1] = '0';
		port[2] = '\0';
	} else {
		port_pos += 1;
		strcpy(port, port_pos);
		for (i = 0; i < strlen(port); i++) {
			if (!isdigit(port[i])) {
				port[i] = '\0';
				break;
			}
		}
	}

	/* extract path from uri */
	path_pos = strchr(hostname_pos, '/');
	if (path_pos != NULL) {
		strcpy(path, path_pos);
		for (i = 0; i < strlen(path); i++) {
			if (path[i] == ':') {
				path[i] = '\0';
				break;
			}
		}
	}
	return;
}

/* build_http_header - build the http header to be sent to server
 * args: 
 * char *hostname - the string of hostname
 * char *path - the string of path
 * char *port - the string of port
 * rio_t *client_rio - the rio struct of client
 * char *http_header - the string of http header to be writen
 * return: none
 */
void build_http_header(char *hostname, char *path, 
	                   char *port, rio_t *client_rio, char *http_header) {
	char buf[MAXLINE];
	char request_header[MAXLINE], host_header[MAXLINE], other_header[MAXLINE];
	int n;

	sprintf(request_header, requestline_header, path);
	/* read from the header sent from client */
	while ((n = Rio_readlineb(client_rio, buf, MAXLINE)) > 0) {
		if (!strncasecmp(buf, host_key, strlen(host_key))) { //Host
			strcpy(host_header, buf);
		} else if (strncasecmp(buf,connection_key,strlen(connection_key))
             && strncasecmp(buf,proxy_connection_key,strlen(proxy_connection_key))
             && strncasecmp(buf,user_agent_key,strlen(user_agent_key))) {
			strcat(other_header, buf); //other header
		} else if (strcmp(buf, end_header)) {
			break;
		}
	}

	if (strlen(host_header) == 0) {
		sprintf(host_header, "Host: %s\r\n", hostname);
	}

	*http_header = '\0';
	strcat(http_header, request_header);
	strcat(http_header, host_header);
	strcat(http_header, other_header);
	strcat(http_header, connection_header);
	strcat(http_header, proxy_connection_header);
	strcat(http_header, header_user_agent);
	strcat(http_header, end_header);
	return;
}

/* sigpipe_handler - the signal handler of SIGPIPE
 * args: int sig - signal
 * return: none
 */
void sigpipe_handler(int sig) {
	return;
}

/***************************************************
 * Wrappers for error handling helper functions
 ***************************************************/

/*
 * Open_listenfd - helper function for open_listenfd(char *port)
 */
int Open_listenfd(char *port) {

    int rc;

    if ((rc = open_listenfd(port)) < 0) 
    sio_printf("Open_listenfd error");
    return rc;

}

/*
 * Getnameinfo - helper function for getnameinfo
 */
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags)
{
    int rc;

    if ((rc = getnameinfo(sa, salen, host, hostlen, serv, 
                          servlen, flags)) != 0) 
        sio_printf("Getnameinfo error");
}

/*
 * Close - helper function for close(int fd)
 */
void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0) {
    	sio_printf("Close error");
    }
}

/*
 * Rio_writen - helper function for rio_writen
 */
int Rio_writen(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n) {
    	sio_printf("Rio_writen error\n");
    	return -1;
    } else {
    	return 1;
    }

}

/*
 * Rio_readinitb - helper function for rio_readinitb
 */
void Rio_readinitb(rio_t *rp, int fd)
{
    rio_readinitb(rp, fd);
} 

/*
 * Rio_readlineb - helper function for rio_readlineb
 */
ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
    sio_printf("Rio_readlineb error");
    return rc;
} 