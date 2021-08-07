#include "server_thread.h"

void* SERVER_THREAD(void* server_data)
{
    
    SERVER_THD_DATA* server_thread_data = (SERVER_THD_DATA*)server_data;

    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN] = { 0 };
    int recvbuflen = DEFAULT_BUFLEN;

    bool running = true;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        running = false;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        running = false;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        running = false;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        running = false;
    }

    freeaddrinfo(result);

    //tcp_ip_dat rec_dat;


    while (running) {

        iResult = listen(ListenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            printf("listen failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            running = false;
            break;
        }

        // Accept a client socket
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            running = false;
            break;
        }

        // No longer need server socket
        //closesocket(ListenSocket);

        // Receive until the peer shuts down the connection
        do {
            std::unique_lock<std::mutex> critical(*server_thread_data->mtx_ptr);
            iResult = recv(ClientSocket, (char*)&server_thread_data->incoming_data, sizeof(TCP_IP_DAT), 0);
            critical.unlock();


            if (iResult > 0) {
                /*printf("Bytes received: %d\n", iResult);
                printf("messege: %s\n", server_thread_data->incoming_data->path);
                printf("Horz: %u\n", server_thread_data->incoming_data->horz);
                printf("Vert: %u\n", server_thread_data->incoming_data->vert);
                printf("fps: %u\n", server_thread_data->incoming_data->fps);
                printf("exp: %u\n", server_thread_data->incoming_data->exp);
                printf("bpp: %u\n", server_thread_data->incoming_data->bpp);
                printf("capTime: %u\n", server_thread_data->incoming_data->capTime);
                printf("flags: %u\n", server_thread_data->incoming_data->flags);*/
                critical.lock();
                if (server_thread_data->incoming_data->flags & (CHANGE_CONFIG | AQUIRE_CAMERAS | START_CAPTURE | EXIT_THREAD)) {
                    // Wakeup main loop if one of these event flags is present
                    server_thread_data->signal_ptr->notify_one();
                    if (server_thread_data->incoming_data->flags & EXIT_THREAD) {
                        running = false;
                    }
                    critical.unlock();
                }
                else {
                    critical.unlock();
                }

                // Echo the buffer back to the sender
                iSendResult = send(ClientSocket, (char*)&server_thread_data->outgoing_data, sizeof(TCP_IP_DAT), 0);
                if (iSendResult == SOCKET_ERROR) {
                    printf("send failed with error: %d\n", WSAGetLastError());
                    closesocket(ClientSocket);
                    WSACleanup();
                    //return 1;
                }
                printf("Bytes sent: %d\n\n", iSendResult);
            }
            else if (iResult == 0)
                printf("Connection closing...\n");
            else {
                printf("recv failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                iResult = 0;
            }



            // shutdown the connection since we're done
            iResult = shutdown(ClientSocket, SD_SEND);
            if (iResult == SOCKET_ERROR) {
                printf("shutdown failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                iResult = 0;
            }
        } while (iResult > 0);
    }
    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}