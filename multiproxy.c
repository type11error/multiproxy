#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "proxy.h"
#include "ht.h"

#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404
#define MAX_REQUESTS 10

/* Predefine a few functions */
void logger(int type, char *s1, char *s2, int socket_fd);
void multipart_handler(int fd, char *uri, char *query);
void test_handler(int fd, char *uri, char *query);

/* Static table and typedefs for our handlers */
typedef void (*handler_fp)(int fd, char *uri, char *query);
struct {
  char *uri;
  handler_fp function;
} handlers [] = {
  {"/multipart.html", &multipart_handler},
  {"/test4.html", &test_handler}, /* test handler */
  {0,0} 
};

/* query_lookup table */
struct ht_table *query_lookup;

/* lookup table and helper function for extensions */
struct {
  char *ext;
  char *filetype;
} extensions [] = {
  {"gif", "image/gif" },  
  {"jpg", "image/jpg" }, 
  {"jpeg","image/jpeg"},
  {"png", "image/png" },  
  {"ico", "image/ico" },  
  {"zip", "image/zip" },  
  {"gz",  "image/gz"  },  
  {"tar", "image/tar" },  
  {"htm", "text/html" },  
  {"html","text/html" },  
  {0,0} };
char *get_extension_type(char *uri) {
	/* work out the file type and check we support it */
	int buflen=strlen(uri);
	char *fstr = (char *)0;
  int i = 0;
	for(i=0;extensions[i].ext != 0;i++) {
		int len = strlen(extensions[i].ext);
		if( !strncmp(&uri[buflen-len], extensions[i].ext, len)) {
			fstr = extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) logger(FORBIDDEN,"file extension type not supported",uri,0);

  return fstr;
}

/* multipart handler. This takes in a dest and query query parameters for
   proxying */
void multipart_handler(int fd, char *uri, char *query) {
	static char headers[BUFSIZE+1]; /* static so zero filled */
  long len = 0;
  int i = 0;
  char *fstr = get_extension_type(uri);
  char *q_dest = NULL;
  char *q_query = NULL;

  /* cut up the query string on & 
     note: we should probably sanatize this */
  char *str_ptr = strtok(query,"&");
  while(str_ptr) {

    char *key = str_ptr;
    char *value = strchr(str_ptr, '=');
    if(value) {
      *value = 0;
      value +=1;
	    logger(LOG,"KEY",key,0);
	    logger(LOG,"VALUE",value,0);

      if(!strcmp("dest", key)) {
        q_dest = value;
      } else if (!strcmp("query", key)) {
        q_query = value;
      }
    }

    str_ptr = strtok(NULL, "&"); 
  }

  /* iterate through dest query parameter */
  proxy_data proxy_requests[MAX_REQUESTS];
  int request = 0;
  if(q_dest && q_query) {
    str_ptr = strtok(q_dest, ",");
    while(str_ptr) {
      /* execute on dest */
      if(request < MAX_REQUESTS) {
        /* setup proxy request */
        proxy_init(&proxy_requests[request]);
        proxy_requests[request].dest = str_ptr;
        proxy_requests[request].query = q_query;

	      logger(LOG,"Starting thread",str_ptr,request);
        pthread_create(&proxy_requests[request].id,
                        NULL,
                        proxy_request,
                        (void *)&proxy_requests[request]);
        
        request += 1; 
        str_ptr = strtok(NULL, ","); 
      }
      else {
			  logger(ERROR,"too many proxy requests","multiproxy",0);
        break;
      }
    }
  }

  /* printout our html header */ 
  static char output_value[BUFSIZE+1]; 
  len += snprintf(&output_value[len], BUFSIZE-len+1, 
                  "<html><body><ul>\n<h3>query: %s<h3>\n", q_query);
    

  /* wait for all requests to finish and print output */
  for(i=0; i < request; i += 1) {
    int error = pthread_join(proxy_requests[i].id, NULL);
    /* print out requests */ 
    if(!error) {
      len += snprintf(&output_value[len], BUFSIZE-len+1, 
                      "<li>%s</li>\n", proxy_requests[i].result);
    }
    else {
      len += snprintf(&output_value[len], BUFSIZE-len+1, 
                      "<li>ERROR: \"%s\"</li>\n", proxy_requests[i].error_str);
    }
  }
  
  /* print out the footer */
  len += snprintf(&output_value[len], BUFSIZE-len+1, "</ul></body></html>\n");


  /* Header + a blank line */
	static char html_header[BUFSIZE+1]; /* static so zero filled */
  (void)snprintf(headers, BUFSIZE+1, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
	logger(LOG,"header",headers,0);
	(void)write(fd,headers,strlen(headers));

  (void)write(fd,output_value,len);

  /* clean up */
  for(i=0; i < request; i += 1) {
    proxy_cleanup(&proxy_requests[i]);
  }
  
}

/* test handler */
void test_handler(int fd, char *uri, char *query) {
  char *output_value = "Italian Village 5370 South 900 East  Salt Lake City, UT 84117  (801) 266-4182  - 3 stars - $10 per plate";
  long len = strlen(output_value);
  char *fstr = get_extension_type(uri);
	static char headers[BUFSIZE+1]; /* static so zero filled */

  /* Header + a blank line */
  (void)sprintf(headers,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
	logger(LOG,"header",headers,0);
	(void)write(fd,headers,strlen(headers));
  
  (void)write(fd,output_value,len);
}

/* send a file from the filesystem */
void sendfile(int fd, char *buffer, int hit) {
  long i = 0;
  int j = 0;
  int ret = 0;
  long len = 0;
  int buflen = 0;
  int file_fd = 0;
	char *fstr = NULL;

 	/* check for illegal parent directory use .. */
	for(j=0;j<i-1;j++) {
		if(buffer[j] == '.' && buffer[j+1] == '.') {
			logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
		}
  }

  /* convert no filename to index file */
	if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) {
		(void)strcpy(buffer,"GET /index.html");
  }

	/* work out the file type and check we support it */
  fstr = get_extension_type(&buffer[5]);

  /* open the file for reading */
	if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {
		logger(NOTFOUND, "failed to open file",&buffer[5],fd);
	}
	logger(LOG,"SEND",&buffer[5],hit);

  /* lseek to the file end to find the length */
	len = (long)lseek(file_fd, (off_t)0, SEEK_END); 

  /* lseek back to the file start ready for reading */
	(void)lseek(file_fd, (off_t)0, SEEK_SET); 

  /* Header + a blank line */
  (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); 
	logger(LOG,"Header",buffer,hit);
	(void)write(fd,buffer,strlen(buffer));

	/* send file in 8KB block - last block may be smaller */
	while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		(void)write(fd,buffer,ret);
	}
}

/* log mechanism */
void logger(int type, char *s1, char *s2, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];

	switch (type) {
	case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid()); 
		break;
	case FORBIDDEN: 
		(void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",272);
		(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2); 
		break;
	case NOTFOUND: 
		(void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
		(void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2); 
		break;
	case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
	}	
	/* No checks here, nothing can be done with a failure anyway */
	if((fd = open("nweb.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer)); 
		(void)write(fd,"\n",1);      
		(void)close(fd);
	}
	if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit)
{
	int j;
	long i, ret, len;
	static char buffer[BUFSIZE+1]; /* static so zero filled */

	ret = read(fd,buffer,BUFSIZE); 	/* read Web request in one go */
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		logger(FORBIDDEN,"failed to read browser request","",fd);
	}

	if(ret > 0 && ret < BUFSIZE) {	/* return code is valid chars */
		buffer[ret]=0;		/* terminate the buffer */
  }
	else { 
    buffer[0]=0;
  }

  /* remove CF and LF characters */
	for(i=0;i<ret;i++) {	
		if(buffer[i] == '\r' || buffer[i] == '\n') {
			buffer[i]='*';
    }
  }

	logger(LOG,"request",buffer,hit);
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
		logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
	}

  /* null terminate after the second space to ignore extra stuff */
	for(i=4;i<BUFSIZE;i++) { 
		if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = 0;
			break;
		}
	}

  /* break out the query string */
  char *uri = buffer + 4;
  char *query_string = NULL;
  for(i=4; i<BUFSIZE;i++) {
    if(buffer[i] == '?') {
      buffer[i] = 0;
      query_string = &buffer[i+1];
      break;
    }
  }

  /* see if we can execute on any handlers */
  int found_handler = 0;
	for(i=0;handlers[i].uri != 0;i++) {
		len = strlen(handlers[i].uri);
		if( !strncmp(uri, handlers[i].uri, len)) {
      (*handlers[i].function)(fd, uri, query_string);
      found_handler = 1;
			break;
		}
	}
 
  /* if we havent found a handler, see if we can send a file */ 
  if(!found_handler) {
    sendfile(fd, buffer, hit);
  }

	sleep(1);	/* allow socket to drain before signalling the socket is closed */
	close(fd);
	exit(1);

}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd, hit;
	socklen_t length;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */

  struct ht_table *query_lookup = ht_init(20);
  ht_set(query_lookup, "test1", "test1_value");
  ht_set(query_lookup, "test2", "test2_value");
  ht_set(query_lookup, "test3", "test3_value");

  printf("%s: %s", "test1", (char *)ht_get(query_lookup, "test1"));

	if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
		(void)printf("hint: multiproxy Port-Number Top-Directory\t\tversion %d\n\n"
	"\tmultiproxy is a small and very safe mini web server\n"
	"\tmultiproxy only servers out file/web pages with extensions named below\n"
	"\t and only from the named directory or its sub-directories.\n"
	"\tThere is no fancy features = safe and secure.\n\n"
	"\tExample: multiproxy 8080 /home/multiproxydir &\n\n"
	"\tOnly Supports:", VERSION);
		for(i=0;extensions[i].ext != 0;i++)
			(void)printf(" %s",extensions[i].ext);

		(void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
	"\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
	"\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n"  );
		exit(0);
	}
	if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
	    !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
	    !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
	    !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
		(void)printf("ERROR: Bad top directory %s, see nweb -?\n",argv[2]);
		exit(3);
	}
	if(chdir(argv[2]) == -1){ 
		(void)printf("ERROR: Can't Change to directory %s\n",argv[2]);
		exit(4);
	}

  curl_global_init(CURL_GLOBAL_ALL);

	/* Become deamon + unstopable and no zombies children (= no wait()) */
	if(fork() != 0)
		return 0; /* parent returns OK to shell */

	(void)signal(SIGCLD, SIG_IGN); /* ignore child death */
	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	for(i=0;i<32;i++)
		(void)close(i);		/* close open files */
	(void)setpgrp();		/* break away from process group */
	logger(LOG,"nweb starting",argv[1],getpid());
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		logger(ERROR, "system call","socket",0);
	port = atoi(argv[1]);
	if(port < 0 || port >60000)
		logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		logger(ERROR,"system call","bind",0);
	if( listen(listenfd,64) <0)
		logger(ERROR,"system call","listen",0);
	for(hit=1; ;hit++) {
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			logger(ERROR,"system call","accept",0);
		if((pid = fork()) < 0) {
			logger(ERROR,"system call","fork",0);
		}
		else {
			if(pid == 0) { 	/* child */
				(void)close(listenfd);
				web(socketfd,hit); /* never returns */
			} else { 	/* parent */
				(void)close(socketfd);
			}
		}
	}
}
