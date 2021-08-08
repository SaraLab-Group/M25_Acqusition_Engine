
//#pragma once
#ifndef SERVER_THREAD_H
#define SERVER_THREAD_H

#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#include "project_headers.h"

//

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
//#pragma comment (lib, "Mswsock.lib")
//#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

void* SERVER_THREAD(void* server_data);

#endif
