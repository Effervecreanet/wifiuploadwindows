#include <Windows.h>

#include "ewu_http_nv.h"

/* Try to find a value in client header pairs */

int
nv_find_name_client(struct header_nv nv[HEADER_NV_MAX_SIZE], const char* name_client)
{
    int count = 0;
    size_t lennc = strlen(name_client);

    do {
        if (nv[count].name.client[0] == '\0')
            break;

        if (_strnicmp(nv[count].name.client, name_client,
            lennc) == 0)
            return count;

    } while (++count < HEADER_NV_MAX_SIZE);

    return -1;
}

/* Try to find a value in server header pairs */

int
nv_find_name_wsite(struct header_nv nv[HEADER_NV_MAX_SIZE], const char* name_wsite)
{
    int count = 0;

    do {
        if (nv[count].name.wsite == NULL)
            continue;

        if (strcmp(nv[count].name.wsite, name_wsite) == 0)
            return count;
    } while (++count < HEADER_NV_MAX_SIZE);

    return -1;
}

