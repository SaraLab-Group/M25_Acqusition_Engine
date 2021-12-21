#include "server_thread.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstdint>

void* SERVER_THREAD(void* server_data)
{
    printf("Starting Server\n");
    SERVER_THD_DATA* server_thread_data = (SERVER_THD_DATA*)server_data;
    printf("Horz: %u\n",server_thread_data->incoming_data->horz);

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

    uint16_t usb_count = 50;

    printf("Starting Server Loop\n");
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
        //printf("sizeof(TCP_IP_DAT): %d\n", sizeof(TCP_IP_DAT));
        do {
            std::unique_lock<std::mutex> critical(*server_thread_data->mtx_ptr);
            iResult = recv(ClientSocket, (char*)server_thread_data->incoming_data, sizeof(TCP_IP_DAT), 0);
            critical.unlock();

            //printf("iResult: %d \n", iResult);
            if (iResult > 0) {
                /*printf("Bytes received: %d\n", iResult);
                critical.lock();
              
                printf("messege: %s\n", server_thread_data->incoming_data->path);
                printf("Horz: %u\n", server_thread_data->incoming_data->horz);
                printf("Vert: %u\n", server_thread_data->incoming_data->vert);
                printf("fps: %u\n", server_thread_data->incoming_data->fps);
                printf("exp: %u\n", server_thread_data->incoming_data->exp);
                printf("bpp: %u\n", server_thread_data->incoming_data->bpp);
                printf("capTime: %u\n", server_thread_data->incoming_data->capTime);
                printf("flags: %u\n", server_thread_data->incoming_data->flags);
                printf("gain: %f\n", server_thread_data->incoming_data->gain);
                printf("Before Mutex Lock\n");*/
                critical.lock();

                // This statement addresses an issue with the USB THREAD and MAIN THREAD both needing to respond to the CHANGE_CONFIG flag;
                /*std::unique_lock<std::mutex> usb_srv_lk(*server_thread_data->usb_srv_mtx);
                if (server_thread_data->outgoing_data->flags & CONFIG_CHANGED && server_thread_data->usb_outgoing->flags & ACK_CMD) {
                    server_thread_data->incoming_data->flags &= ~CHANGE_CONFIG;
                    server_thread_data->usb_outgoing->flags &= ~ACK_CMD;
                    server_thread_data->outgoing_data->flags &= ~(CHANGE_CONFIG | CONFIG_CHANGED | ACK_CMD);
                    printf("This Should stop CHANGE_CONFIG\n");
                }
                usb_srv_lk.unlock();*/
                std::unique_lock<std::mutex> usb_srv_lk(*server_thread_data->usb_srv_mtx);
                if (server_thread_data->usb_incoming->flags & USB_HERE) {
                    server_thread_data->outgoing_data->flags |= USB_HERE;
                    server_thread_data->outgoing_data->fps = server_thread_data->usb_incoming->fps;
                } 
                usb_srv_lk.unlock();
                
                if (server_thread_data->incoming_data->flags & STOP_LIVE) {
                    *server_thread_data->live_flags = server_thread_data->incoming_data->flags;
                }
                else {
                    *server_thread_data->live_flags = 0;
                }

                

                if (server_thread_data->incoming_data->flags & (CHANGE_CONFIG | ACQUIRE_CAMERAS | RELEASE_CAMERAS | START_CAPTURE | START_LIVE | START_Z_STACK | EXIT_THREAD)) {
                    // Wakeup main loop if one of these event flags is present
                    usb_srv_lk.lock();
                    if (server_thread_data->incoming_data->flags & CHANGE_CONFIG) {
                        printf("setting usb change_config flag\n");
                        server_thread_data->usb_outgoing->flags |= CHANGE_CONFIG;
                        server_thread_data->usb_outgoing->fps = server_thread_data->incoming_data->fps;
                    }
                    usb_srv_lk.unlock();

                    server_thread_data->signal_ptr->notify_one(); // Wakes up Main Loop

                    if (server_thread_data->incoming_data->flags & EXIT_THREAD) {
                        server_thread_data->usb_outgoing->flags |= EXIT_THREAD;
                        *server_thread_data->live_flags |= EXIT_THREAD;
                        running = false;
                    }
                    critical.unlock();
                }
                else {
                    critical.unlock();
                }
                //printf("After Mutex Lock\n");
                // Echo the buffer back to the sender
                critical.lock();
                iSendResult = send(ClientSocket, (char*)server_thread_data->outgoing_data, sizeof(TCP_IP_DAT), 0);
                critical.unlock();
                if (iSendResult == SOCKET_ERROR) {
                    printf("send failed with error: %d\n", WSAGetLastError());
                    closesocket(ClientSocket);
                    WSACleanup();
                    //return 1;
                }
                //printf("Bytes sent: %d\n\n", iSendResult);
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