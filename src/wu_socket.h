
int  create_socket(COORD* cursorPosition);
void bind_socket(COORD* cursorPosition, int s, struct in_addr inaddr);
void bind_socket2(COORD* cursorPosition, int s, struct in_addr inaddr);
int  accept_conn(COORD* cursorPosition, int s, char ipaddrstr[16]);
