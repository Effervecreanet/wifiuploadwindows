
#define HTTP_RESSOURCE_MAX_LENGTH 127 
#define  HTTP_TYPE_MAX_LENGTH    32
#define HTTP_STRING_DATE_SIZE sizeof("Mon, 01 Jan 1970 00:00:00 GMT")

struct http_reqline {
  unsigned char method[sizeof("POST")];
  unsigned char resource[HTTP_RESSOURCE_MAX_LENGTH + 1];
  unsigned char  version[sizeof("HTTTP/1.1")];
};

struct http_resource {
  unsigned char resource[HTTP_RESSOURCE_MAX_LENGTH];
  unsigned char type[HTTP_TYPE_MAX_LENGTH];
};

struct success_info {
  char filename[FILENAME_MAX_SIZE];
  char filenameSize[24];
  char elapsedTime[24];
  char averagespeed[20];
};

errno_t redir404(int s);
errno_t http_recv_reqline(struct http_reqline *reqline, int s);
errno_t http_recv_headernv(struct header_nv *nv, int s);
int http_match_resource(char* resource);
int http_serv_resource(struct http_resource *res, int s,
                       struct success_info *successinfo,
					   int *bytesent);
