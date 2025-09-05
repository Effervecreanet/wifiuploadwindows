#define HTTP_METHOD_GET  "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_VERSION 	 "HTTP/1.1"

int get_request_line(struct http_reqline *reqline, char BufferIn[2048]);
