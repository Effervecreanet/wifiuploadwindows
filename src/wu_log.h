
#define LOG_DIRECTORY ".wulogs"

void log_https_request(char *ipaddr_httpsclt, struct http_reqline *reqline, int bytesent);
void create_log_directory(char logpath[512], char log_filename[sizeof("log_19700101.txt")], char loghttps_filename[sizeof("loghttps_19700101.txt")]);
