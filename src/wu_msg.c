#include <Windows.h>
#include <WinSock.h>
#include <iphlpapi.h>
#include <UserEnv.h>
#include <strsafe.h>
#include <stdio.h>

#include "wu_msg.h"

#ifdef VERSION_FR

ewu_msg ewumsg[] = {
    {
        INF_PROMOTE_WIFIUPLOAD,
        PROMOTE_EFFERVECREANET,
        sizeof(PROMOTE_EFFERVECREANET) - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN,
            0
        }
    },
    {
        INF_WIFIUPLOAD_SERVICE,
        "Service de t" "\xE9" "l" "\xE9" "chargement wifi",
        sizeof("Service de t" "\xE9" "l" "\xE9" "chargement wifi") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN,
            0
        }
    },
    {
        INF_WIFIUPLOAD_IS_LISTENING_TO,
        "wifiupload est disponible " "\xE0" " l'adresse suivante:",
        sizeof("wifiupload est disponible " "\xE0" " l'adresse suivante:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_HTTP_LISTEN,
        "http://%s/",
        sizeof("http://%s/"),
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | COMMON_LVB_UNDERSCORE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_DOWNLOAD_DIRECTORY_IS,
        "Le r" "\xE9" "p" "\xE9" "rtoire de t" "\xE9" "l" "\xE9" "chargement est:",
        sizeof("Le r" "\xE9" "p" "\xE9" "rtoire de t" "\xE9" "l" "\xE9" "chargement est:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_UI_DOWNLOAD_DIRECTORY,
        "T" "\xE9" "l" "\xE9" "chargements",
        sizeof("T" "\xE9" "l" "\xE9" "chargements") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_UI_TX_RX,
        "Affichage envoie/r" "\xE9" "ception: ",
        sizeof("Affichage envoie/r" "\xE9" "ception: ") - 1,
        {
            FOREGROUND_INTENSITY,
            0
        }
     },
    {
        INF_WIFIUPLOAD_UI_FILE_DOWNLOAD,
        "Fichier: T" "\xE9" "l" "\xE9" "chargements\\",
        sizeof("Fichier: T" "\xE9" "l" "\xE9" "chargements\\") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_MSG_TWO_AVAILABLE_ADDR,
        "Deux adresses r" "\xE9" "seaux disponibles:",
        sizeof("Deux adresses r" "\xE9" "seaux disponibles:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_MSG_INCOMING_CONNECTION,
        "connexion entrante re" "\xE7" "ue",
        sizeof("connexion entrante re" "\xE7" "ue") - 1,
        {
            FOREGROUND_INTENSITY,
            0
        }
    },
    {
        INF_MSG_CHOICE_QUESTION,
        "choix ? ",
        sizeof("choix ? ") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_FMT_MSG_ONE_AVAILABLE_ADDR,
        "une seule adresse r" "\xE9" "seau disponible:",
        sizeof("une seule adresse r" "\xE9" "seau disponible:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_FMT_MSG_ONE_AVAILABLE_ADDR_2,
        "%s",
        sizeof("%s") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },

    {
        INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_1,
        "adresse r" "\xE9" "seau disponible (choix 1): %s",
        sizeof("adresse r" "\xE9" "seau disponible (choix 1): %s") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_2,
        "adresse r" "\xE9" "seau disponible (choix 2): %s",
        sizeof("adresse r" "\xE9" "seau disponible (choix 2): %s") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        ERR_MSG_CONNECTIVITY,
        "Erreur: l'application ne peut pas fonctionner car aucune connectivit" "\xE9" " n'est " "\xE9" "tablie (pas de connexion r" "\xE9" "seau).",
        sizeof("Erreur: l'application ne peut pas fonctionner car aucune connectivit" "\xE9" " n'est " "\xE9" "tablie (pas de connexion r" "\xE9" "seau).") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        ERR_MSG_TOO_MANY_ADDR,
        "Erreur: trop d'adresse r" "\xE9" "seau disponibles (maximum 2)",
        sizeof("Erreur: trop d'adresse r" "\xE9" "seau disponibles (maximum 2)") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        ERR_MSG_CANNOT_GET_RESOURCE,
        "Impossible d'obtenir le r" "\xE9" "pertoire courant ou la ressource " "\xE0" " servir a " "\xE9" "t" "\xE9" " supprim" "\xE9" "e",
        sizeof("Impossible d'obtenir le r" "\xE9" "pertoire courant ou la ressource " "\xE0" " servir a " "\xE9" "t" "\xE9" " supprim" "\xE9" "e") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 0}
    },
    {
        ERR_FMT_MSG_CANNOT_SERV_RESOURCE,
        "Impossible de servir la ressource erreur: %i",
        sizeof("Impossible de servir la ressource erreur: %i") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 0}
    },
    {
        ERR_MSG_CANNOT_CREATE_FILE,
        "Impossible de cr" "\xE9" "er le fichier : disque plein ou pas assez de droits(%i)",
        sizeof("Impossible de cr" "\xE9" "er le fichier: disque plein ou pas assez de droits (%i)") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 0}
    },
    {
        INF_ZERO_PERCENT,
        "   0 %",
        sizeof("   0 %") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_CENT_PERCENT,
        " 100 % ",
        sizeof(" 100 % ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_TX_SPEED_UI_GO,
        "vitesse de transfert: %s GO/s    ",
        sizeof("vitesse de transfert: %s GO/s    ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_TX_SPEED_UI_MO,
        "vitesse de transfert: %s MO/s    ",
        sizeof("vitesse de transfert: %s MO/s    ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_TX_SPEED_UI_KO,
        "vitesse de transfert: %s KO/s    ",
        sizeof("vitesse de transfert: %s KO/s    ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_ONE_PBAR,
        " ",
        1,
        {BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_UNDERSCORE, 0}
    },
    {
        INF_WIFIUPLOAD_CURRENT_PERCENT,
        "  %hu %% ",
        sizeof("  %hu %% ") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY, 0}
    },
    {
        INF_ERR_END,
        "",
        1,
        {0, 0}
    }
};

#else

ewu_msg ewumsg[] = {
    {
        INF_PROMOTE_WIFIUPLOAD,
        PROMOTE_EFFERVECREANET,
        sizeof(PROMOTE_EFFERVECREANET) - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN,
            0
        }
    },
    {
        INF_WIFIUPLOAD_SERVICE,
        "Wifiupload service",
        sizeof("Wifiupload service") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN,
            0
        }
    },
    {
        INF_WIFIUPLOAD_IS_LISTENING_TO,
        "wifiupload is available at the following address:",
        sizeof("wifiupload is available at the following address:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_HTTP_LISTEN,
        "http://%s/",
        sizeof("http://%s/"),
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | COMMON_LVB_UNDERSCORE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_DOWNLOAD_DIRECTORY_IS,
        "The download directory is:",
        sizeof("The download directory is:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_UI_DOWNLOAD_DIRECTORY,
        "Downloads",
        sizeof("Downloads") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_WIFIUPLOAD_UI_TX_RX,
        "Show send/receive: ",
        sizeof("Show send/receive: ") - 1,
        {
            FOREGROUND_INTENSITY,
            0
        }
     },
    {
        INF_WIFIUPLOAD_UI_FILE_DOWNLOAD,
        "File: Downloads\\",
        sizeof("File: Downloads\\") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_MSG_TWO_AVAILABLE_ADDR,
        "Two address are availables:",
        sizeof("Two address are availables:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_MSG_INCOMING_CONNECTION,
        "incoming connection received",
        sizeof("incoming connection received") - 1,
        {
            FOREGROUND_INTENSITY,
            0
        }
    },
    {
        INF_MSG_CHOICE_QUESTION,
        "choice ? ",
        sizeof("choice ? ") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_FMT_MSG_ONE_AVAILABLE_ADDR,
        "one network address available:",
        sizeof("one network address available:") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_FMT_MSG_ONE_AVAILABLE_ADDR_2,
        "%s",
        sizeof("%s") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },

    {
        INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_1,
        "network address available (choice 1): %s",
        sizeof("network address available (choice 1): %s") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        INF_FMT_MSG_AVAILABLE_ADDR_CHOICE_2,
        "network address available (choice 2): %s",
        sizeof("network address available (choice 2): %s") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        ERR_MSG_CONNECTIVITY,
        "Error: no network connectivity",
        sizeof("Error: no network connectivity") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        ERR_MSG_TOO_MANY_ADDR,
        "Error: too many newtork address availables (maximum 2)",
        sizeof("Error: too many newtork address availables (maximum 2)") - 1,
        {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            0
        }
    },
    {
        ERR_MSG_CANNOT_GET_RESOURCE,
        "Cannot get current directory or the resource has been deleted",
        sizeof("Cannot get current directory or the resource has been deleted") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 0}
    },
    {
        ERR_FMT_MSG_CANNOT_SERV_RESOURCE,
        "Cannot serve the resource error: %i",
        sizeof("Cannot serve the resource error: %i") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 0}
    },
    {
        ERR_MSG_CANNOT_CREATE_FILE,
        "Cannot create the file: disk is full or unsufficients permission (%i)",
        sizeof("Cannot create the file: disk is full or unsufficients permission (%i)") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, 0}
    },
    {
        INF_ZERO_PERCENT,
        "   0 %",
        sizeof("   0 %") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_CENT_PERCENT,
        " 100 % ",
        sizeof(" 100 % ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_TX_SPEED_UI_GO,
        "transfer speed: %s GB/s    ",
        sizeof("transfer speed: %s GB/s    ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_TX_SPEED_UI_MO,
        "transfer speed: %s MB/s    ",
        sizeof("transfer speed: %s MB/s    ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_TX_SPEED_UI_KO,
        "transfer speed: %s KB/s    ",
        sizeof("transfer speed: %s KB/s    ") - 1,
        {FOREGROUND_INTENSITY, 0}
    },
    {
        INF_WIFIUPLOAD_ONE_PBAR,
        " ",
        1,
        {BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_UNDERSCORE, 0}
    },
    {
        INF_WIFIUPLOAD_CURRENT_PERCENT,
        "  %hu %% ",
        sizeof("  %hu %% ") - 1,
        {FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY, 0}
    },
    {
        INF_ERR_END,
        "",
        1,
        {0, 0}
    }
};

#endif
