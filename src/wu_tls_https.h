#define HTTP_METHOD_GET  "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_VERSION 	 "HTTP/1.1"



int get_https_request(CtxtHandle* ctxtHandle, SOCKET s_clt, char** tls_recv_output, unsigned int* tls_recv_output_size, COORD* cursorPosition,
						struct http_reqline* reqline, struct header_nv headernv[HEADER_NV_MAX_SIZE], struct in_addr inaddr);
						
int handle_get_request(CtxtHandle* ctxtHandle, SOCKET s_clt, struct http_reqline* reqline, struct header_nv headernv[HEADER_NV_MAX_SIZE],
						int* bytesent, COORD* cursorPosition);

int handle_post_request(CtxtHandle* ctxtHandle, SOCKET s_clt, struct http_reqline* reqline, struct header_nv headernv[HEADER_NV_MAX_SIZE],
						int* bytesent, char* https_body, COORD* cursorPosition);

int https_serv_resource(struct http_resource* res, SOCKET s, struct success_info* successinfo, int* bytesent, CtxtHandle* ctxtHandle, COORD cursorPosition);
