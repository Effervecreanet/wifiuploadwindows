
int http_match_resource(char* res);
int create_local_resource(struct http_resource* lres, int ires, int theme);
void check_cookie_theme(struct header_nv hdrnv[], int* theme);
void show_download_directory(void);
int http_loop(COORD* cursorPosition, struct in_addr* inaddr, SOCKET s, char logentry[256]);
