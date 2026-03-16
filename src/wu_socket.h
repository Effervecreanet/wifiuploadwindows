
SOCKET  create_socket(COORD* cursorPosition);
void bind_socket(COORD* cursorPosition, SOCKET s, struct in_addr inaddr);
int  accept_conn(COORD* cursorPosition, SOCKET s, char ipaddrstr[16]);
