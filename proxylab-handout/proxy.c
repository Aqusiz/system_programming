#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct {
    char *method;
    char *hostname;
    char *path;
    char *port;
    char *proxy_connection;
    char *user_agent_hdr;
    char *uri;
} Request;

typedef struct {
    int *connfdp;
    struct sockaddr_in *clientaddrp;
} Thread_args;

void *handle_client(void *args);
void initialize_struct(Request *req);
void parse_request(char request[MAXLINE], Request *req);
void parse_absolute(Request *req);
// void parse_relative(Request *req);
void parse_header(char header[MAXLINE], Request *req);
void assemble_request(Request *req, char *reqeust);
// int get_from_cache(Request *req, int clientfd);
void get_from_server(Request *req, char request[MAXLINE], int clientfd);
void close_wrapper(int fd);
void print_full(char *string);
void print_struct(Request *req);

int main(int argc, char** argv)
{
    int listenfd;
    pthread_t tid;
    int clientlen = sizeof(struct sockaddr_in);

    printf("%s", user_agent_hdr);
    listenfd = Open_listenfd(argv[1]);
    
    while(1) {
        int *connfdp = malloc(sizeof(int *));
        struct sockaddr_in *clientaddrp = malloc(sizeof(struct sockaddr_in *));
        *connfdp = accept(listenfd, (SA *)clientaddrp, (socklen_t *)&clientlen);
        Thread_args *args = malloc(sizeof(Thread_args *));
        args->connfdp = connfdp;
        args->clientaddrp = clientaddrp;
        pthread_create(&tid, NULL, handle_client, (void *)args);
    }

    return 0;
}

void *handle_client(void *args) {
    Thread_args *p = (Thread_args *) args;
    struct sockaddr_in clientaddr = *(p->clientaddrp);
    struct hostent *hp;                 // pointer to client's DNS host entry
    char *haddrp;                       // pointer to client's dotted decimal string
    unsigned short client_port;
    rio_t rp;
    char buf[MAXLINE], request[MAXLINE];    // read buffer, request line
    Request *req = malloc(sizeof(Request));
    
    int connfd = *(p->connfdp);
    Pthread_detach(pthread_self());
    Free(p->connfdp);

    hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    haddrp = inet_ntoa(clientaddr.sin_addr);
    client_port = ntohs(clientaddr.sin_port);
    printf("server connected to %s (%s), port %u\n", hp->h_name, haddrp, client_port);

    // Receive request from client
    initialize_struct(req);
    printf("%s\n", req->port);
    Rio_readinitb(&rp, connfd);
    while(Rio_readlineb(&rp, buf, MAXLINE) > 0) {
        printf("%s", buf);
        if (strcmp(buf, "\r\n") == 0) break;
        if (strncmp(buf, "GET", 3) == 0) {
            parse_request(buf, req);
        }
        else {
            parse_header(buf, req);
        }
    }
    parse_absolute(req);

    printf("after parsing\nmethod: %s, hostname: %s, uri: %s, Proxy-Connection: %s", req->method, req->hostname, req->uri, req->proxy_connection);
    printf("port: %s, path: %s\n", req->port, req->path);

    // Send request to end server and forward response
    get_from_server(req, request, connfd);

    return NULL;
}

void initialize_struct(Request *req) {
    req->hostname = (char *) malloc(MAXLINE);
    req->method = (char *) malloc(MAXLINE);
    req->path = (char *) malloc(MAXLINE);
    req->port = (char *) malloc(MAXLINE);
    req->proxy_connection = (char *) malloc(MAXLINE);
    req->uri = (char *) malloc(MAXLINE);
    req->user_agent_hdr = (char *) malloc(MAXLINE);
}

void parse_request(char request[MAXLINE], Request *req) {
    char temp[MAXLINE];
    strcpy(temp, request);

    strcpy(req->method, strtok(temp, " "));
    printf("parse method: %s\n", req->method);
    strcpy(req->uri, strtok(NULL, " ")+7);
    printf("parse URI: %s\n", req->uri);
}

void parse_header(char header[MAXLINE], Request *req) {
    char temp[MAXLINE];
    char *token = (char *)malloc(MAXLINE);
    strcpy(temp, header);

    token = strtok(temp, ": ");
    printf("parse header: %s\n", token);
    if (strcmp(token, "Host") == 0) {
        token = strtok(NULL, " ");
        token[strlen(token)-2] = '\0';
        strcpy(req->hostname, token);
        printf("Host: %s\n", req->hostname);
    }
    else if (strcmp(token, "User-Agent") == 0) {
        token = strtok(NULL, " ");
        strcpy(req->user_agent_hdr, token);
        printf("User-Agent: %s", req->user_agent_hdr);
    }
    else if (strcmp(token, "Accept") == 0) {
        return;
    }
    else if (strcmp(token, "Proxy-Connection") == 0) {
        token = strtok(NULL, " ");
        strcpy(req->proxy_connection, token);
        printf("Proxy-Connection: %s", req->proxy_connection);
    }
}

void parse_absolute(Request *req) {
    char temp[MAXLINE];
    char *token;
    char *p;
    strcpy(temp, req->uri);
    printf("temp: %s\n", temp);
    token = strtok(temp, "/");
    printf("token: %s\n", token);
    if ((p = strchr(token, ':')) != NULL) {
        printf("%s\n", p);
        strcpy(req->port, p+1);
        printf("port: %s\n", req->port);
    }
    token = strtok(NULL, " ");
    printf("token: %s\n", token);
    strcpy(temp, "/");
    strcpy(req->path, strcat(temp, token));
    printf("path: %s\n", req->path);
}

void get_from_server(Request *req, char request[MAXLINE], int clientfd) {
    int serverfd;
    rio_t rp;
    char buf[MAXLINE];

    if (strcmp(req->port, "") == 0) req->port = "80";
    if (strchr(req->hostname, ':') != NULL) req->hostname = strtok(req->hostname, ":");
    serverfd = Open_clientfd(req->hostname, req->port);
    Rio_readinitb(&rp, serverfd);

    sprintf(buf, "%s %s HTTP/1.0\r\n", req->method, req->path);
    Rio_writen(serverfd, buf, strlen(buf));
    sprintf(buf, "Host: %s\r\n", req->hostname);
    Rio_writen(serverfd, buf, strlen(buf));
    sprintf(buf, "%s", user_agent_hdr);
    Rio_writen(serverfd, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
    sprintf(buf, "Proxy-Connection: close\r\n\r\n");
    Rio_writen(serverfd, buf, strlen(buf));

    size_t n;
    char cbuf[MAX_OBJECT_SIZE]; // buffer to store in cache
    int buf_size = 0;
    while((n = Rio_readlineb(&rp, buf, MAXLINE)) != 0) {
        buf_size += n;
        if(buf_size < MAX_OBJECT_SIZE) strncat(cbuf, buf, n); // !
        Rio_writen(clientfd, buf, n);
    }

    Close(serverfd);
}