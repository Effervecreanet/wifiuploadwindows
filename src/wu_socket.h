
int  create_socket(HANDLE conScreenBuffer, COORD* cursorPosition);
void bind_socket(HANDLE conScreenBuffer, COORD* cursorPosition, int s, struct in_addr inaddr);
int  accept_conn(HANDLE conScreenBuffer, COORD* cursorPosition, int s);
