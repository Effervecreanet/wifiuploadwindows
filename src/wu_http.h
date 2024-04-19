#define HTTP_RESSOURCE_MAX_LENGTH  1024
#define  HTTP_TYPE_MAX_LENGTH    32

struct http_reqline {
  unsigned char method[sizeof("POST")];
  unsigned char resource[HTTP_RESSOURCE_MAX_LENGTH + 1];
  unsigned char  version[sizeof("HTTTP/1.1")];
};

struct http_resource {
  unsigned char resource[HTTP_RESSOURCE_MAX_LENGTH];
  unsigned char type[HTTP_TYPE_MAX_LENGTH];
};

errno_t http_recv_reqline(struct http_reqline *reqline, int s);
errno_t http_recv_headernv(struct header_nv *nv, int s);