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

#include "config.h"
#include "log.h"
#include "ht.h"
#include "proxy.h"
#include "handlers.h"
#include "extensions.h"


struct ht_table *handlers;

/* send a file from the filesystem */
void sendfile_handler(int fd, char *buffer, int hit) {
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

  /* see if we can execute on any handlers, if not try to send a static file */
  handler_fp handler = ht_get(handlers, uri);
  if(handler) {
    handler(fd, uri, query_string);
  }
  else {
    sendfile_handler(fd, buffer, hit);
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


  // Setup our handlers
  handlers_init();
  handlers = ht_init(10);
  ht_set(handlers, "/multiproxy.html", (void *)multipart_handler);
  ht_set(handlers, "/a.json", (void *)a_handler);
  ht_set(handlers, "/b.json", (void *)b_handler);
  ht_set(handlers, "/c.json", (void *)c_handler);


	if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
		(void)printf("hint: multiproxy Port-Number Top-Directory\t\tversion %d\n\n"
	"\tmultiproxy is a small and very safe mini web server\n"
	"\tmultiproxy only servers out file/web pages with extensions named below\n"
	"\t and only from the named directory or its sub-directories.\n"
	"\tThere is no fancy features = safe and secure.\n\n"
	"\tExample: multiproxy 8080 /home/multiproxydir &\n\n", VERSION);

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
	//if(fork() != 0)
	//	return 0; /* parent returns OK to shell */

	//(void)signal(SIGCLD, SIG_IGN); /* ignore child death */
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
