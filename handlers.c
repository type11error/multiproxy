#include "handlers.h"
#include "extensions.h"
#include "proxy.h"
#include "config.h"
#include "ht.h"

struct ht_table *resturaunt_db;

void handlers_init() { 
  resturaunt_db = ht_init(10);

  /* this could be moved to a file */
  ht_set(resturaunt_db, "italian_a", "{\"name\":\"Italian Village\", \"address\": \"5370 South 900 East  Salt Lake City, UT 84117\", \"number\": \"(801) 266-4182\", \"notes\": \"3 stars - $10 per plate\"  }");
  ht_set(resturaunt_db, "italian_b", "{\"name\":\"Mama\'s Italian\", \"address\": \"10078 South 902 West Salt Lake City, UT 84117\", \"number\": \"(801) 724-2039\", \"notes\": \"4 stars - $15 per plate\"  }");
  ht_set(resturaunt_db, "italian_c", "{\"name\":\"Good Food\", \"address\": \"233 W Maple North Salt Lake City, UT 84118\", \"number\": \"(801) 234-6586\", \"notes\": \"5 stars - $30 per plate\"  }");

}

/* multipart handler. This takes in a dest and query query parameters for
   proxying */
void multiproxy_handler(int fd, char *uri, char *query) {
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
                  "{ \"query\": \"%s\",\n\"result\": {", q_query);
    

  /* wait for all requests to finish and print output */
  for(i=0; i < request; i += 1) {
    int error = pthread_join(proxy_requests[i].id, NULL);

    /* set end string so we can print out comma's */
    char *end_str = ",";
    if(i == request -1) { end_str = ""; }

    /* print out requests */ 
    if(!error) {
      len += snprintf(&output_value[len], BUFSIZE-len+1, 
                      "%s%s\n", proxy_requests[i].result, end_str);
    }
    else {
      len += snprintf(&output_value[len], BUFSIZE-len+1, 
                      "{ \"error\": \"%s\"}%s\n", proxy_requests[i].error_str, 
                      end_str);
    }
  }
  
  /* print out the footer */
  len += snprintf(&output_value[len], BUFSIZE-len+1, "}\n}\n");


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

/* a handler */
void a_handler(int fd, char *uri, char *query) {
  char lookup[1024];
  snprintf(lookup, sizeof(lookup), "%s_a", query);
  send_resturaunt(fd, uri, lookup);
}

void b_handler(int fd, char *uri, char *query) {
  char lookup[1024];
  snprintf(lookup, sizeof(lookup), "%s_b", query);
  send_resturaunt(fd, uri, lookup);
}

void c_handler(int fd, char *uri, char *query) {
  char lookup[1024];
  snprintf(lookup, sizeof(lookup), "%s_c", query);
  send_resturaunt(fd, uri, lookup);
}

void send_resturaunt(int fd, char *uri, char *resturaunt){
  char *output_value = ht_get(resturaunt_db, resturaunt);
  if(!output_value) output_value = "{ \"error\": \"not found\"}";
  long len = strlen(output_value);
  char *fstr = get_extension_type(uri);
	static char headers[BUFSIZE+1]; /* static so zero filled */

  /* Header + a blank line */
  (void)sprintf(headers,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
	logger(LOG,"header",headers,0);
	(void)write(fd,headers,strlen(headers));
  
  (void)write(fd,output_value,len);
}
