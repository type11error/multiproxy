#include "proxy.h"

struct {
  char *dest;
  char *url;
} dest_lookup [] = {
  { "A", "http://localhost:8080/a.json" },
  { "B", "http://localhost:8080/b.json" },
  { "C", "http://localhost:8080/c.json" },
  { 0, 0 } };


void proxy_init(proxy_data *data) {
  memset(data, 0, sizeof(proxy_data));
  data->result = malloc(1);
  data->size = 0;
}

void proxy_cleanup(proxy_data *data) {
  if(data->result)
    free(data->result);
}

size_t proxy_write_result(void *ptr, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;

  proxy_data *data = (proxy_data*)userp;

  /* realloc our data */
  data->result = realloc(data->result, data->size + realsize + 1);
  if(data->result == NULL) {
    data->error = 1;
    return 0;
  }

  /* copy the result */ 
  memcpy((void*)&(data->result[data->size]), ptr, realsize);
  data->size += realsize;
  data->result[data->size] = 0; 

  return realsize;
}

int proxy_fetch_content(char *url, proxy_data *data) {

  int res = 1;

  CURL *curl;
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, proxy_write_result);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)data);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, data->error_str);
  res = curl_easy_perform(curl);

  if(res != CURLE_OK) {
    res = 0;
  }

  curl_easy_cleanup(curl); 

  return res;
}

void *proxy_request(void *request) {
  proxy_data *data = (proxy_data*)request;

  /* fetch dest url */
  char url[1024];
  int i = 0;
	for(i=0;dest_lookup[i].url != 0;i++) {
		if( !strcmp(data->dest, dest_lookup[i].dest)) {
			snprintf(url,1024,"%s?%s",dest_lookup[i].url,data->query);
			break;
		}
	}
 
  /* fetch our data */ 
  data->error = proxy_fetch_content(url, data);  

  return NULL;
}
