#include "extensions.h"
#include "log.h"
#include "config.h"
#include <strings.h>

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
