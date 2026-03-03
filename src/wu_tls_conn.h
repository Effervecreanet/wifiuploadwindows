
int tls_send(int s_clt, CtxtHandle* ctxtHandle, char* message, unsigned int message_size, COORD cursorPosition);
int tls_recv(CtxtHandle* ctxtHandle, int s, char** output, unsigned int* outlen, COORD* cursorPosition);
void tls_shutdown(CtxtHandle *ctxtHandle, CredHandle *credHandle, int s_clt);
int acceptSecure(int s, CredHandle *credHandle, CtxtHandle *ctxtHandle, char ipaddr_httpslog[16]);
