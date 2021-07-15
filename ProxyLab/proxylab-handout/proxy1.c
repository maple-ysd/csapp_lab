#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


#include "csapp.h"

void doit(int fd);
void parse_uri(char *uri, char *hostname, char *path, char *port); 
void generate_request_hdrs(rio_t *rio_client, char *req_buf, char *hostname, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:Proxy:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);                                             //line:netp:Proxy:doit
	Close(connfd);                                            //line:netp:Proxy:close
    }
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) 
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
    
    rio_t rio_client, rio_server;
    int end_serverfd;
    
    char req_buf[MAXLINE];

    /* Read request line and headers */
    Rio_readinitb(&rio_client, fd);
    Rio_readlineb(&rio_client, buf, MAXLINE);
    printf("%s", buf);
    if (3 != sscanf(buf, "%s %s %s", method, uri, version))
        return;       //line:netp:doit:parserequest
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }             
    // parse the uri to get hostname, file path, port for end server
    parse_uri(uri, hostname, path, port);
    // connect with end server
    if ((end_serverfd = open_clientfd(hostname, port)) < 0)
        return;
    Rio_readinitb(&rio_server, end_serverfd);
    // generate the request and send request to end server
    generate_request_hdrs(&rio_client, req_buf, hostname, path);
    Rio_writen(end_serverfd, req_buf, MAXLINE);
    
    // receive message from end server and send to client
    int n;
    while ((n = Rio_readlineb(&rio_server, buf, MAXLINE)) != 0) {
        printf("proxy received %d bytes\n", n);
        Rio_writen(fd, buf, n);
    }
    Close(end_serverfd);
}

/*
 * parse_uri
 */
void parse_uri(char *uri, char *hostname, char *path, char *port) 
{
    strcpy(port, "80");
    char *pos = strstr(uri, "//");
    pos = pos != NULL ? pos + 2 : uri;
    char *pos2 = strstr(pos, ":");
    char *pos3 = strstr(pos, "/");
    if (pos2 != NULL && pos3 != NULL) {
        *pos2 = '\0';
        sscanf(pos, "%s", hostname);
        *pos3 = '\0';
        sscanf(pos2 + 1, "%s", port);
        *pos3 = '/';
        sscanf(pos3, "%s", path);
    }
    else if (pos2 != NULL && pos3 == NULL) {
        *pos2 = '\0';
        sscanf(pos, "%s", hostname);
        sscanf(pos2 + 1, "%s", port);
        strcpy(path, "/");
    }
    else if (pos2 == NULL && pos3 != NULL) {
        *pos3 = '\0';
        sscanf(pos, "%s", hostname);
        *pos3 = '/';
        sscanf(pos3, "%s", path);
    }
    else {
        sscanf(pos, "%s", hostname);
        strcpy(path, "/");
    }
}
/*
 * generate_request_hdrs - generate request to end server
 */
void generate_request_hdrs(rio_t *rio_client, char *req_buf, char *hostname, char *path)
{
    char buf[MAXLINE], req_line[MAXLINE], host_hdr[MAXLINE], other[MAXLINE];
    strcpy(other, "\0");
    // request line
    sprintf(req_line, "GET %s HTTP/1.0\r\n", path);
    // get request headers from rio_client
    while (Rio_readlineb(rio_client, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0) break;
        if (!strncasecmp(buf, "Host", 4)) {
            strcpy(host_hdr, buf);
            continue;
        }
        
        if (strncasecmp(buf, "User-Agent", strlen("User-Agent"))
            && strncasecmp(buf,"Proxy-Connection",strlen("Proxy-Connection"))
            && strncasecmp(buf,"Connection",strlen("Connection")))
            strcat(other, buf);     
    }
    if(strlen(host_hdr)==0)
    {
        sprintf(host_hdr,"Host: %s\r\n",hostname);
    }
    sprintf(req_buf, "%s%s%s%s%s%s%s", req_line, host_hdr, 
            user_agent_hdr,
            "Connection: close\r\n",
            "Proxy-Connection: close\r\n",
            other,
            "\r\n");
}

void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
