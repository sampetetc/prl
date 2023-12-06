#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"

#define MAXLINE 8192
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:56.0) Gecko/20100101 Firefox/56.0\r\n";

// Structure to hold thread arguments
struct thread_args {
    int cfd;
};

// Function to parse the URL into host, port, and path
void parse_url(char *url, char *host, char *port, char *path);

// Function to build the request header
void build_header(char *hdrs, char *host, char *port, char *path, rio_t *client_rio);

// Thread routine to handle each client connection
void *p_request(void *varg);

void *p_request(void *varg) {
    struct thread_args *arg = varg;
    int fd = arg->cfd;
    free(arg);

    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char host[MAXLINE], path[MAXLINE], port[MAXLINE];

    rio_t rio, server_rio;

    // Initialize the read buffer for the client connection
    Rio_readinitb(&rio, fd);

    // Read the request line from the client
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, url, version);

    // Parse the URL to extract host, port, and path
    parse_url(url, host, port, path);

    // Build the request header
    char hdrs[MAXLINE];
    build_header(hdrs, host, port, path, &rio);

    // Open a connection to the server
    int serverfd = Open_clientfd(host, port);
    if (serverfd < 0) {
        printf("connection failed\n");
        return NULL;
    }

    // Initialize the read buffer for the server connection
    Rio_readinitb(&server_rio, serverfd);

    // Send the request header to the server
    Rio_writen(serverfd, hdrs, strlen(hdrs));

    size_t n;
    // Receive the response from the server and forward it to the client
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        printf("bytes received: %d, bytes sent:\n", (int)n);
        Rio_writen(fd, buf, n);
    }

    // Close the server connection and the client connection
    Close(serverfd);
    Close(fd);

    return NULL;
}

int main(int argc, char **argv) {
    int listenfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Open a listening socket
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        int *connfd = malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("%s %s connected successfully\n", hostname, port);

        // Create a detached thread for each request
        pthread_t thread;
        struct thread_args *arg = malloc(sizeof(*arg));
        arg->cfd = *connfd;
        int rv = pthread_create(&thread, NULL, p_request, arg);

        if (rv < 0) {
            perror("pthread_create");
            Close(*connfd); // Close the connection in case of thread creation failure
            free(connfd);
        } else {
            pthread_detach(thread); // Detach the thread to avoid memory leaks
        }
    }

    return 0;
}

void build_header(char *hdrs, char *host, char *port, char *path, rio_t *client_rio) {

    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);
    strcpy(other_hdr, ""); // Initialize other
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)
            break; /* EOF */

        if (!strncasecmp(buf, "Host", strlen("Host"))) {
            strcpy(host_hdr, buf);
        } else if (!strncasecmp(buf, "Connection", strlen("Connection")) && !strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection")) && !strncasecmp(buf, "User-Agent", strlen("User-Agent"))) {
            strcat(other_hdr, buf);
        }
    }

    if (strlen(host_hdr) == 0) {
        sprintf(host_hdr, "Host: %s\r\n", host);
    }

    sprintf(hdrs, "%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            "Connection: close\r\n",
            "Proxy-Connection: close\r\n",
            user_agent_hdr,
            other_hdr,
            "\r\n");
}

void parse_url(char *url, char *host, char *port, char *path) {
    char *hostpose = strstr(url, "//");
    if (hostpose == NULL) {
        char *pathpose = strstr(url, "/");
        if (pathpose != NULL)
            strcpy(path, pathpose);
        strcpy(port, "80");
        return;
    } else {
        char *portpose = strstr(hostpose + 2, ":");
        if (portpose != NULL) {
            int tmp;
            sscanf(portpose + 1, "%d%s", &tmp, path);
            sprintf(port, "%d", tmp);
            *portpose = '\0';
        } else {
            char *pathpose = strstr(hostpose + 2, "/");
            if (pathpose != NULL) {
                strcpy(path, pathpose);
                strcpy(port, "80");
                *pathpose = '\0';
            }
        }
        strcpy(host, hostpose + 2);
    }
}

