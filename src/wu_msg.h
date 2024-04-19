#define CONSOLE_TITLE "Effervecreanet - Wifiupload"
#define PROMOTE_EFFERVECREANET CONSOLE_TITLE

#ifdef LOCAL_EN

#define ERR_FMT_MSG_INIT_CONSOLE "Cannot initialize win32 console, err: %lu\n"
#define ERR_FMT_MSG_INIT_WINSOCK "Cannot initialize network stack Winsock, error: %lu\n"

#define EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_GO "%.2f GB/s"
#define EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_MO "%.2f MB/s"
#define EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_KO "%.2f KB/s"

#define ERROR_MESSAGE_SOCKET_1 "Cannot create socket error: %lu"
#define ERROR_MESSAGE_SOCKET_2 "Cannot bind socket error: %i"
#define ERROR_MESSAGE_SOCKET_3 "Cannot accept connection: %i"

#else

#define ERR_FMT_MSG_INIT_CONSOLE "Erreur lors de l'initialisation de la console, err: %lu\n"
#define ERR_FMT_MSG_INIT_WINSOCK "Erreur lors de l'initialisation de la pile r" "\xE9" "seau, err: %lu\n"

#define EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_GO "%.2f GO/s"
#define EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_MO "%.2f MO/s"
#define EWU_WIFIUPLOAD_AVERAGE_TX_SPEED_KO "%.2f KO/s"

#define ERROR_MESSAGE_SOCKET_1 "Impossible de cr" "\xE9" "er le socket erreur: %lu"
#define ERROR_MESSAGE_SOCKET_2 "Impossible de lier le socket erreur: %i"
#define ERROR_MESSAGE_SOCKET_3 "Impossible d'accepter la connexion erreur: %i"

#endif




enum idmsg {
    INF_PROMOTE_WIFIUPLOAD,
    INF_WIFIUPLOAD_SERVICE,
    INF_WIFIUPLOAD_IS_LISTENING_TO,
    INF_WIFIUPLOAD_HTTP_LISTEN,
    INF_WIFIUPLOAD_DOWNLOAD_DIRECTORY_IS,
    INF_WIFIUPLOAD_UI_DOWNLOAD_DIRECTORY,
    INF_WIFIUPLOAD_UI_TX_RX,
    INF_WIFIUPLOAD_UI_FILE_DOWNLOAD,
    INF_FMT_MSG_ONE_AVAILABLE_ADDR,
    INF_FMT_MSG_ONE_AVAILABLE_ADDR_2,
    INF_FMT_MSG_TWO_AVAILABLE_ADDR,
    INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_1,
    INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_2,
    INF_MSG_CHOICE_QUESTION,
    INF_MSG_TWO_AVAILABLE_ADDR,
    INF_MSG_INCOMING_CONNECTION,
    STR_FMT_DOWNLOAD_DIR,
    ERR_MSG_CONNECTIVITY,
    ERR_MSG_TOO_MANY_ADDR,
    ERR_MSG_CANNOT_GET_RESOURCE,
    ERR_FMT_MSG_CANNOT_SERV_RESOURCE,
    ERR_MSG_CANNOT_CREATE_FILE,
    ERR_MSG_FAIL_TX,
    INF_ZERO_PERCENT,
    INF_CENT_PERCENT,
    INF_WIFIUPLOAD_TX_SPEED_UI_GO,
    INF_WIFIUPLOAD_TX_SPEED_UI_MO,
    INF_WIFIUPLOAD_TX_SPEED_UI_KO,
    INF_WIFIUPLOAD_ONE_PBAR,
    INF_WIFIUPLOAD_CURRENT_PERCENT,
    INF_ERR_END
};

struct wu_msg {
	enum idmsg	 id;
	CONST CCHAR* Msg;
	DWORD     	 szMsg;
	WORD		 wAtributes[2];
};

