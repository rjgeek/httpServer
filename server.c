

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/wait.h>

#include <pthread.h>

#include "server.h"

#include "protocol/http_header.h"
#include "thread/thread_pool.h"

static char *custom_path;
static struct thread_pool *pool;

static void userguide(char *progname)
{
    printf("Usage: %s -h\n"
       " -p port to accept HTTP clients requests\n"
	   " -f path specifies root directory of server reachable under '/files' prefix\n"
	   " -h get help\n",
	   progname);
    exit(EXIT_SUCCESS);
}
/**
 * main
 */
int main(int argc, char **argv)
{
	//after read
    signal(SIGPIPE, SIG_IGN);
    if (argc < 2)
    	userguide(argv[0]);
    long port;
    char c;
    // 接收输入
    while ((c = getopt(argc, argv, "p:f:h:")) != -1) {
	switch (c) {
		case 'h':
			userguide(argv[0]);
			break;
		case 'p': {
			char *garbage = NULL;
			port = strtol(optarg, &garbage, 10);
	
			switch (errno) {
			case ERANGE:
			printf("Please enter a valid port number");
			return -1;
			case EINVAL:
			printf("Please enter a valid port number");
			return -1;
			}
			break;
		}
		case 'f':
			// path that specifies the root dir of the server
			//回一个指针,指向为复制字符串分配的空间;如果分配空间失败,则返回NULL值。
			custom_path = strdup(optarg);
			break;
		case '?':
		default:
			userguide(argv[0]);
		}
    }

    //默认地址
    if (custom_path == NULL){
    	custom_path = "files";
    }

    list_init(&memory_list);

    int socketfd, optval = 1;
    intptr_t connfd;

    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Error opening socket.\n");
		exit(1);
    }

    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0) {
		fprintf(stderr, "Error in socket. %s.\n", strerror(errno));
		exit(1);
    }

    struct sockaddr_in serveraddr;
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) port);
    
    if (bind(socketfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		fprintf(stderr, "Error binding socket. %s.\n", strerror(errno));
		exit(1);
    }

    if (listen(socketfd, 1024) < 0) {
		fprintf(stderr, "Error listening to socket.\n");
		exit(1);
    }
    puts("Bham Server is stated");
    printf("workplace is %s\n server port is %ld\n",custom_path,port);
    pool = thread_pool_new(THREADS);
    struct sockaddr_in clientaddr;

    while(true) {
		// new connected socket
		int clientlen = sizeof(clientaddr);

		if ((connfd = accept(socketfd, (struct sockaddr *)&clientaddr, (socklen_t *)&clientlen)) <= 0) {
			fprintf(stderr, "Error acceping connection.\n");
			exit(1);
		}
		// execute request in a different thread
		thread_pool_submit(pool, (thread_pool_callable_func_t)process_http, (int *)connfd);
		/*while (future_get(f)) {
		  future_free(f);
		  }*/
    }
    thread_pool_shutdown(pool);
    return 0;
}

static void process_http(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE];
    char method[MAXLINE];
    char uri[MAXLINE];
    char version[MAXLINE];
    char filename[MAXLINE];
    char cgiargs[MAXLINE];
    rio_t rio;
    char pathbuf[64];
    sprintf(pathbuf, "./%s/", custom_path); 

    Rio_readinitb(&rio, fd);
    
	while (1) {
		/* Read request line and headers */
		ssize_t x;
		if ((x = Rio_readlineb(&rio, buf, MAXLINE)) <= 0)
			break;

		sscanf(buf, "%s %s %s", method, uri, version);

		Rio_readlineb(&rio, buf, MAXLINE);

		//printf("method=%s, uri=%s, version=%s\n\n", method, uri, version);
		//printf("%s\n",method);
		//strcasecmp -- Compare Strings without Case Sensitivity
		/*if (strcasecmp(method, "POST")==0) {
			clienterror(fd, method, "501", "Not Implemented", "Server does not implement this method", version);
			continue;
		}*/
		ssize_t size = read_request_header(&rio);

		if (size <= 0) {
			 /* Compare the two strings provided up to 8 characters
			  * If the two strings are the same say return 0
			  * */
			if (strncmp(version, "HTTP/1.0", 8) == 0) {
				break;
			}
			else {
				continue;
			}
		}
		/* Parse URI from GET request */
		is_static = uri_is_static(uri, filename, cgiargs);

		if (strstr(filename, "..") != NULL){
			clienterror(fd, filename, "403", "Forbidden", "Server couldn't read the file", version);
		}
		/**
		 * check the file if exist
		 */
		if (is_static) {
			if (stat(filename, &sbuf) < 0){
						clienterror(fd, filename, "404", "Not found", "Page Not Found", version);
			}

			if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
				clienterror(fd, filename, "403", "Forbidden", "Server couldn't read the file", version);
			}
			serve_static(fd, filename, sbuf.st_size);
		}else {
			puts(version);
			/*if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
				clienterror(fd, filename, "403", "Forbidden", "Server couldn't run the CGI program", version);
			}*/
			serve_dynamic(fd, filename, cgiargs);
		}
		if (strncmp(version, "HTTP/1.0", 8) == 0) {
			break;
		}
	}
    close(fd);
}


ssize_t read_request_header(rio_t *rp)
{
    char buf[MAXLINE];
    ssize_t size = Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
	size = Rio_readlineb(rp, buf, MAXLINE);
	if (size <= 0)
	    break;
    }
    return size;
}

/*
 * uri_is_static - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 * Taken from Tiny server and slightly modified
 */
int uri_is_static(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    if (strstr(uri, "jsp")||strstr(uri, "php")||strstr(uri, "aspx")) {
    	 /* Dynamic content */                        //line:netp:parseuri:isdynamic
			ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
			if (ptr) {
				strcpy(cgiargs, ptr+1);
				*ptr = '\0';
			}
			else{
				strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
			}
			strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
			strcat(filename, uri);                           //line:netp:parseuri:endconvert2
			return 0;
    }else {
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if (uri[strlen(uri)-1] == '/') {
			char pathbuf[64];
			sprintf(pathbuf, "%s/index.html", custom_path);
			strcat(filename, pathbuf);
		}
		ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
		if (ptr) {
			char *p;
			const char *split = "?";
			p = strtok(uri,split);
			strcpy(filename, ".");
			strcat(filename, p);                           //line:netp:parseuri:beginconvert2
			//puts(filename);
			*ptr = '\0';
		}
		return 1;
    }
}

/*
 * clienterror - returns an error message to the client
 * Taken from Tiny server
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, char *version) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Bham Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "%s %s %s\r\n", version, errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


/*
 * serve_static - copy a file back to the client
 * Taken from Tiny server
 */
void serve_static(int fd, char *filename, int filesize) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    char *serve_time;
    char time_buf[TIME_BUFFER_SIZE];
    memset(time_buf, '\0', sizeof(time_buf));
    serve_time = get_date_str(time_buf);

    get_content_type(filename, filetype);
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Bham Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n", buf, filetype);
    sprintf(buf, "%sExpires: %s\r\n",buf, "-1");
    sprintf(buf, "%sPragma: %s\r\n",buf, "no-cache");
    sprintf(buf, "%sConnection: %s\r\n",buf, "keep-alive");
    sprintf(buf, "%sDate: %s\r\n",buf, serve_time);

    Rio_writen(fd, buf, strlen(buf));       //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//line:netp:servestatic:mmap
    close(srcfd);                           //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);         //line:netp:servestatic:write
    munmap(srcp, filesize);                 //line:netp:servestatic:munmap
}



/*
 * serve_dynamic - run a CGI program on behalf of the client
 * Taken from Tiny server and slightly modified
 */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
	    puts("handle dynamic language");
	    char filetype[MAXLINE];

	    if (strstr(filename, ".jsp")){
	        	strcpy(filetype, "jsp");
		}else if (strstr(filename, ".php")){
			strcpy(filetype, "php");
		}else if (strstr(filename, "aspx")){
			strcpy(filetype, "aspx");
		}
	    strcat(filetype, " Parsing Function Is Under Construction ");

	    char buf[MAXLINE], body[MAXBUF];

		/* Build the HTTP response body */
		sprintf(body, "<html><title>Error</title>");
		sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
		sprintf(body, "%sSorry:%s\r\n", body, filetype);
		sprintf(body, "%s<p>%s:\r\n", body, "give me more time,I can do it");
		sprintf(body, "%s<hr><em>Bham Web server</em>\r\n", body);

		/* Print the HTTP response */
	    sprintf(buf, "%s %s %s\r\n", "HTTP/1.1", "000", "give me more time,I can do it");
		Rio_writen(fd, buf, strlen(buf));
		sprintf(buf, "Content-type: text/html\r\n");
		Rio_writen(fd, buf, strlen(buf));
		sprintf(buf, "%sPragma: %s\r\n",buf, "no-cache");
		sprintf(buf, "%sConnection: %s\r\n",buf, "keep-alive");
		sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
		Rio_writen(fd, buf, strlen(buf));
		Rio_writen(fd, body, strlen(body));
}
