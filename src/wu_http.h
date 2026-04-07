
#define HTTP_RESSOURCE_MAX_LENGTH 254 
#define  HTTP_TYPE_MAX_LENGTH    32
#define HTTP_STRING_DATE_SIZE sizeof("Mon, 01 Jan 1970 00:00:00 GMT")
#define REQUEST_LINE_METHOD_MAX_SIZE 5

struct http_reqline {
	char method[sizeof("POST")];
	char resource[HTTP_RESSOURCE_MAX_LENGTH + 1];
	char  version[sizeof("HTTTP/1.1")];
};

struct http_resource {
	char resource[HTTP_RESSOURCE_MAX_LENGTH];
	char type[HTTP_TYPE_MAX_LENGTH];
};


errno_t redir404(int s);
int time_to_httpdate(char* http_date);
int http_recv_reqline(SOCKET s, struct http_reqline* reqline);
int http_recv_headernv(SOCKET s, struct header_nv* nv);
void create_http_header_nv(struct http_resource* res, struct header_nv* nv, size_t fsize, char connection);
int get_hours_minutes(char hrmn[6]);
int make_htmlpage(struct success_info* successinfo, char* resource, char* pbufferin, char** pbufferout, size_t* pbufferoutlen, DWORD fsize, char hrmn[6]);
int http_serv_resource(struct http_resource* res, SOCKET s, struct success_info* successinfo, int* bytesent, unsigned int status_code);
