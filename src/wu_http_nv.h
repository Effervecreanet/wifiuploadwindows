#define HTTP_CODE_STATUS_OK                             200
#define HTTP_CODE_STATUS_OK_STR                         "200"
#define HTTP_CODE_STATUS_NOT_MODIFIED                   304
#define HTTP_CODE_STATUS_NOT_MODIFIED_STR               "304"
#define HTTP_CODE_STATUS_MOVED_PERMANENTLY              301
#define HTTP_CODE_STATUS_MOVED_PERMANENTLY_STR          "301"
#define HTTP_CODE_STATUS_PARTIAL_CONTENT                206
#define HTTP_CODE_STATUS_PARTIAL_CONTENT_STR            "206"
#define HTTP_CODE_STATUS_BAD_REQUEST     		404
#define HTTP_CODE_STATUS_BAD_REQUEST_STR		"404"

#define HTTP_STRING_STATUS_OK                           "Ok"
#define HTTP_STRING_STATUS_BAD_REQUEST                  "Bad Request"
#define HTTP_STRING_STATUS_NOT_MODIFIED                 "Not Modified"
#define HTTP_STRING_STATUS_MOVED_PERMANENTLY            "Moved Permanently"
#define HTTP_STRING_STATUS_PARTIAL_CONTENT              "Partial Content"

#define HTTP_HEADER_ALLOW                     "Allow"
#define HTTP_HEADER_CONTENT_LANGUAGE          "Content-Language"
#define HTTP_HEADER_CONTENT_LENGTH            "Content-Length"
#define HTTP_HEADER_DATE                      "Date"
#define HTTP_HEADER_HOST                      "Host"
#define HTTP_HEADER_EXPIRES                   "Expires"
#define HTTP_HEADER_LAST_MODIFIED             "Last-Modified"
#define HTTP_HEADER_SERVER                    "Server"
#define HTTP_HEADER_CONTENT_TYPE              "Content-Type"
#define HTTP_HEADER_CONNECTION                "Connection"
#define HTTP_HEADER_IF_MODIFIED_SINCE         "If-Modified-Since"
#define HTTP_HEADER_CACHE_CONTROL             "Cache-Control"
#define HTTP_HEADER_VARY                      "Vary"
#define HTTP_HEADER_ACCEPT_RANGES             "Accept-Ranges"
#define HTTP_HEADER_CONTENT_RANGE             "Content-Range"
#define HTTP_HEADER_RANGE                     "Range"
#define HTTP_HEADER_SET_COOKIE                "Set-Cookie"
#define HTTP_HEADER_UPGRADE_INSECURE          "Upgrade-Insecure-Requests"
#define HTTP_HEADER_UPGRADE_INSECURE_VALUE    "1"
#define HTTP_HEADER_KEEP_ALIVE                "Keep-Alive"

#define HTTP_HEADER_CACHE_CONTROL_VALUE "no-cache, no-store, must-revalidate"
#define HTTP_HEADER_SERVER_VALUE        "wifiupload/5.22"
#define HTTP_HEADER_CONNECTION_VALUE    "Keep-Alive"
#define HTTP_HEADER_KEEP_ALIVE_VALUE    "timeout=127, max=64"
#define HTTP_HEADER_VARY_VALUE          "User-Agent"
#define HTTP_HEADER_ACCEPT_RANGES_VALUE "bytes"
#define HTTP_HEADER_CONTENT_LANGUAGE_VALUE "fr_FR"
#define HTTP_HEADER_LOCATION            "Location"

#define HEADER_MAX_SIZE       12001
#define HEADER_NV_MAX_SIZE    24
#define HEADER_NAME_MAX_SIZE  256
#define HEADER_VALUE_MAX_SIZE 512

union header_name {
    char            client[HEADER_NAME_MAX_SIZE];
    const char* wsite;
};

struct header_value {
    char            v[HEADER_VALUE_MAX_SIZE];
    const char* pv;
};

struct header_nv {
    union header_name name;
    struct header_value value;
};

int             nv_find_name_client(struct header_nv nv[HEADER_NV_MAX_SIZE], const char* name_client);
int             nv_find_name_wsite(struct header_nv nv[HEADER_NV_MAX_SIZE], const char* name_wsite);
