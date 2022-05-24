#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CACHE_ITEM 10

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
void destruct_struct(Request *req);
void parse_request(char request[MAXLINE], Request *req);
void parse_absolute(Request *req);
void parse_header(char header[MAXLINE], Request *req);
void assemble_request(Request *req, char *reqeust);
void get_from_server(Request *req, char request[MAXLINE], int clientfd);

typedef struct {
    char obj[MAX_OBJECT_SIZE];
    char url[MAXLINE];
    int LRU, valid, rc, wc;
    sem_t wmutex, wcmutex, rcmutex, q;
} CachedItem;

typedef struct {
    CachedItem item[10];
} CacheList;

CacheList cache;

void cache_init();
int find(char *uri);
int evict();
void LRU(int idx);
void cache_uri(char *uri, char *buf);
void read_lock(int idx);
void read_unlock(int idx);
void write_lock(int idx);
void write_unlock(int idx);

int main(int argc, char** argv)
{
    if (argc != 2 || atoi(argv[1]) == 0) {
        printf("./proxy [port]\n");
        exit(0);
    }
    int listenfd;
    pthread_t tid;
    int clientlen = sizeof(struct sockaddr_in);

    printf("%s", user_agent_hdr);
    cache_init();
    listenfd = Open_listenfd(argv[1]);

    while(1) {
        // accept connection and make client address
        int *connfdp = malloc(sizeof(int));
        struct sockaddr_in *clientaddrp = malloc(sizeof(struct sockaddr_in *));
        if ((*connfdp = accept(listenfd, (SA *)clientaddrp, (socklen_t *)&clientlen)) < 0) {
            printf("Connection error.\n");
            continue;
        };
        // pass connfd and client address to thread as arguments
        Thread_args *args = malloc(sizeof(Thread_args));
        args->connfdp = connfdp;
        args->clientaddrp = clientaddrp;
        pthread_create(&tid, NULL, handle_client, (void *)args);
    }

    Close(listenfd);
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

    if ((hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET)) == NULL) {
        printf("gethostbyaddr error\n");
        return NULL;
    }
    if ((haddrp = inet_ntoa(clientaddr.sin_addr)) < 0) {
        printf("inet_ntoa error\n");
        return NULL;
    }
    client_port = ntohs(clientaddr.sin_port);
    printf("server connected to %s:%u (%s)\n", hp->h_name, client_port, haddrp);

    // Receive request from client
    initialize_struct(req);
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

    printf("%s %s %s\n", req->method, req->hostname, req->uri);
    // find cache memory and if exist, forward it to client
    int idx;
    if ((idx = find(req->uri)) != -1) {
        Rio_writen(connfd, cache.item[idx].obj, strlen(cache.item[idx].obj));
        LRU(idx);
        return NULL;
    }

    // Send request to end server and forward response
    get_from_server(req, request, connfd);

    Close(connfd);
    Free(req);
    // destruct_struct(req);
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

void destruct_struct(Request *req) {
    Free(req->hostname);
    Free(req->method);
    Free(req->path);
    Free(req->port);
    Free(req->proxy_connection);
    Free(req->uri);
    Free(req->user_agent_hdr);
    Free(req);
}

void parse_request(char request[MAXLINE], Request *req) {
    char temp[MAXLINE];
    char *token;
    strcpy(temp, request);
    strcpy(req->method, strtok(temp, " "));
    token = strtok(NULL, " ");
    if (strncmp(token, "http://", 7) == 0) token += 7;  // remove "http://"
    strcpy(req->uri, token);
}

void parse_header(char header[MAXLINE], Request *req) {
    char temp[MAXLINE];
    char *token = (char *)malloc(MAXLINE);
    strcpy(temp, header);

    token = strtok(temp, ": ");
    if (strcmp(token, "Host") == 0) {
        token = strtok(NULL, " ");
        token[strlen(token)-2] = '\0';
        strcpy(req->hostname, token);
    }
    else if (strcmp(token, "User-Agent") == 0) {
        token = strtok(NULL, " ");
        strcpy(req->user_agent_hdr, token);
    }
    else if (strcmp(token, "Accept") == 0) {
        return;
    }
    else if (strcmp(token, "Proxy-Connection") == 0) {
        token = strtok(NULL, " ");
        strcpy(req->proxy_connection, token);
    }
}

void parse_absolute(Request *req) {
    char temp[MAXLINE];
    char *token;
    char *p;
    strcpy(temp, req->uri);
    token = strtok(temp, "/");
    printf("%s\n", token);
    if ((p = strchr(token, ':')) != NULL) {
        strcpy(req->port, p+1);
    }

    if ((token = strtok(NULL, " ")) != NULL) {
        strcpy(temp, "/");
        strcpy(req->path, strcat(temp, token));
    }
    else {
        p = malloc(MAXLINE);
        strcpy(p, "http://");
        strcpy(req->path, strcat(p, req->uri));
    }
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
        if(buf_size < MAX_OBJECT_SIZE) strncat(cbuf, buf, n);
        Rio_writen(clientfd, buf, n);
    }
    if(buf_size < MAX_OBJECT_SIZE) cache_uri(req->uri, cbuf);
    Close(serverfd);
}

void cache_init() {
    for (int i = 0; i < MAX_CACHE_ITEM; i++) {
        CachedItem *item = &cache.item[i];
        item->LRU = 0;
        item->valid = 1;
        item->rc = 0;
        item->wc = 0;
        Sem_init(&item->wmutex, 0, 1);
        Sem_init(&item->wcmutex, 0, 1);
        Sem_init(&item->rcmutex, 0, 1);
        Sem_init(&item->q, 0, 1);
    }
}

int find(char *uri) {
    int i;
    for (i = 0; i < MAX_CACHE_ITEM; i++) {
        read_lock(i);
        if (!cache.item[i].valid && (strcmp(uri, cache.item[i].url) == 0)) break;
        read_unlock(i);
    }

    if (i >= MAX_CACHE_ITEM) return -1;
    return i;
}

int evict() {
    int minidx = 0;

    for (int i = 0; i < MAX_CACHE_ITEM; i++) {
        read_lock(i);
        CachedItem *item = &cache.item[i];
        if (item->valid) {
            minidx = i;
            read_unlock(i);
            return minidx;
        }
        if (item->LRU < cache.item[minidx].LRU) minidx = i;
        read_unlock(i);
    }
    return minidx;
}

void LRU(int idx) {
    write_lock(idx);
    cache.item[idx].LRU = 8192;
    write_unlock(idx);

    for (int i = 0; i < 10; i++) {
        if (i == idx) continue;
        write_lock(i);
        if(!cache.item[i].valid && i != idx) cache.item[i].LRU--;
        write_unlock(i);
    }
}

void cache_uri(char *uri, char *buf) {
    int idx = evict();
    write_lock(idx);
    CachedItem *item = &cache.item[idx];
    strcpy(item->obj, buf);
    strcpy(item->url, uri);
    item->valid = 0;
    write_unlock(idx);

    LRU(idx);
}

void read_lock(int idx) {
    CachedItem *item = &cache.item[idx];

    P(&item->q);
    P(&item->rcmutex);
    item->rc += 1;

    if (item->rc == 1) P(&item->wmutex);

    V(&item->rcmutex);
    V(&item->q);
}

void read_unlock(int idx) {
    CachedItem *item = &cache.item[idx];

    P(&item->rcmutex);
    item->rc -= 1;

    if (item->rc == 0) V(&item->wmutex);

    V(&item->rcmutex);
}

void write_lock(int idx) {
    CachedItem *item = &cache.item[idx];

    P(&item->wcmutex);
    item->wc += 1;

    if(item->wc == 1) P(&item->q);

    V(&item->wcmutex);
    P(&item->wmutex);
}

void write_unlock(int idx) {
    CachedItem *item = &cache.item[idx];

    V(&item->wmutex);
    P(&item->wcmutex);
    item->wc -= 1;

    if (item->wc == 0) V(&item->q);

    V(&item->wcmutex);
}