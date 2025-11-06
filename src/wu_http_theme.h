int wu_http_recv_theme(struct header_nv httpnv[HEADER_NV_MAX_SIZE], int s_user, int* theme);
void wu_http_post_theme_hdr_nv(struct header_nv hdrnv[32], char* cookie);
int apply_theme(int susr, char* cookie);
