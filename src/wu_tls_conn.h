int acceptSecure(int s, CredHandle *credHandle, CtxtHandle *ctxtHandle, char ipaddr_httpslog[16]);
void tls_shutdown(CtxtHandle *ctxtHandle, CredHandle *credHandle, int s_clt);
