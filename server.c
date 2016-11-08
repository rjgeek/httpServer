

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

		puts(buf);

		//printf("method=%s, uri=%s, version=%s\n\n", method, uri, version);
		//printf("%s\n",method);
		//strcasecmp -- Compare Strings without Case Sensitivity
		/*if (strcasecmp(method, "POST")==0) {
			clienterror(fd, method, "501", "Not Implemented", "Server does not implement this method", version);
			continue;
		}*/
        //puts(buf);
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
		is_static = parse_uri(uri, filename, cgiargs);

		/* Compare the two strings provided up to 8 characters
		* If the two strings are the same say return 0
		*/
		if (strncmp(uri, "/meminfo\0", sizeof("/meminfo\0")-1) == 0 || strncmp(uri, "/meminfo?", sizeof("/meminfo?")-1) == 0) {
			FILE *fp;
			fp = fopen("/proc/meminfo", "r");
			if (fp) {
				char line[256];
				char membuf[1024];
				strcpy(membuf, "{");
				int flag = 0;
				while (fgets(line, sizeof(line), fp)) {

					if (flag != 0) {
					strcat(membuf, ", ");
					}
					char title[64];
					long memory;
					char skip[5];
					sscanf(line, "%s %lu %s", title, &memory, skip);
					title[strlen(title)-1] = '\0';
					char tempbuf[256];
					sprintf(tempbuf, "\"%s\": \"%lu\"", title, memory);
					strcat(membuf, tempbuf);
					flag = 1;
				}
				fclose(fp);
				strcat(membuf, "}");
				char callbackbuf[256];
				if (parse_callback(uri, callbackbuf) > 0) {
					char returnbuf[256];
					sprintf(returnbuf, "%s(%s)", callbackbuf, membuf);
					response_ok(fd, returnbuf, "application/javascript", version);
				}else{
					response_ok(fd, membuf, "application/json", version);
				}
			}
		}
		/***
		 * check filename contain pathbuf
		 * if name not contain pathbuf return 404
		 * example:
		 * right: http://127.0.0.1:8088/files/index.html
		 * wrong:http://127.0.0.1:8088/index.html
		 */
		else if (strncmp(filename, pathbuf, strlen(pathbuf)) == 0) {
				// check that filename does not contain '..'
			    /**
			     * strstr:
			     * Parameters or Arguments
			     * s1 A string to search within.
                   s2 The substring that you want to find.
                  The strstr function returns a pointer to the first occurrence s2 within the string pointed to by s1. If s2 isn't found,
                   it returns a null pointer.
			     */
				if (strstr(filename, "..") != NULL){
					clienterror(fd, filename, "403", "Forbidden", "Server couldn't read the file", version);
				}
				/**
				 * check the file if exist
				 */

				if (stat(filename, &sbuf) < 0){
					clienterror(fd, filename, "404", "Not found", "Page Not Found", version);
				}

				if (is_static) {
					if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
						clienterror(fd, filename, "403", "Forbidden", "Server couldn't read the file", version);
					}
					serve_static(fd, filename, sbuf.st_size);
				}else {
					if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
						clienterror(fd, filename, "403", "Forbidden", "Server couldn't run the CGI program", version);
					}
					serve_dynamic(fd, filename, cgiargs);
				}
		}else{
			clienterror(fd, filename, "404", "Not Found", "Not found", version);
		}
		/* Compare the two strings provided up to 8 characters
		* If the two strings are the same say return 0
		*/
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
	puts(buf);
	if (size <= 0)
	    break;
    }
    return size;
}

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 * Taken from Tiny server and slightly modified
 */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;
    if (!strstr(uri, "jsp")) {
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
    }else {
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
    }
}

char* substringStr(char* ch,int pos,int length)
 {
      char* pch=ch;
  //定义一个字符指针，指向传递进来的ch地址。
      char* subch=(char*)calloc(sizeof(char),length+1);
  //通过calloc来分配一个length长度的字符数组，返回的是字符指针。
     int i;
 //只有在C99下for循环中才可以声明变量，这里写在外面，提高兼容性。
     pch=pch+pos;
 //是pch指针指向pos位置。
     for(i=0;i<length;i++)
     {
         subch[i]=*(pch++);
 //循环遍历赋值数组。
     }
     subch[length]='\0';//加上字符串结束符。
     return subch;       //返回分配的字符数组地址。
}

/*
 * Build and send an HTTP response
 */
void response_ok(int fd, char *msg, char *content_type, char *version)
{
    char buf[MAXLINE];
    char body[MAXBUF];
    sprintf(body, "%s", msg);
    sprintf(buf, "%s 200 OK\r\n", version);
    if (strncmp(version, "HTTP/1.0", strlen("HTTP/1.0")) == 0){
    	sprintf(buf, "Connection: close\r\n");
    }
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Content-type: %s\r\n", content_type);
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));

    //printf("Content-length: %d\r\n\r\n", (int)strlen(buf));
    Rio_writen(fd, body, strlen(body));
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
 * parse_callback
 * parse URI and returns callback buffer size
 * return 0 if no callback exists
 */
static size_t parse_callback(char *uri, char *callback)
{
    char *ptr;
    ptr = index(uri, '?');
    fflush(stdout);
    
    if (ptr) {

	while (ptr) {
	    ptr++;
	    if (strncmp(ptr, "callback=", sizeof("callback=")-1) == 0) {
		
		ptr = index(ptr, '=');
		int pass = 0;
		
		if (ptr) {
		    ptr++;
		    
		    int i = 0;
		    
		    while (*ptr && *ptr != '&') {
			
			int c = ptr[0];
			if ((c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 46 || (c >= 48 && c <= 57) || c == 95) {
			    callback[i] = *ptr;
			    i++;
			    ptr++;
			}

			else
			    pass = 1;
		    }

		    if (pass == 0) {
			callback[i] = '\0';
			return sizeof(callback);
		    }
		}
		
	    }

	    else
		ptr = index(ptr, '&');
	}
    }

    return 0;
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
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Bham Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));


    if (fork() == 0) { /* child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
	dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }

    if (wait(NULL) < 0) {
	fprintf(stderr, "Wait Error.\n");
	exit(1);
    }
}
