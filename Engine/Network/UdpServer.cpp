#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "UdpServer.h"
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

UdpServer::~UdpServer() {
    Finalize();
}

bool UdpServer::Initialize(int port) {
    if (isInitialized_) return true;

    // WinSockの初期化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    // ソケットの作成
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }
    socket_ = static_cast<uintptr_t>(sock);

    // アドレスとポートの設定
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // 全てのインターフェースで受信
    serverAddr.sin_port = htons(port);

    // バインド
    if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        socket_ = ~0ULL;
        return false;
    }

    // ノンブロッキングモードに設定 (ioctlsocket)
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        WSACleanup();
        socket_ = ~0ULL;
        return false;
    }

    isInitialized_ = true;
    return true;
}

void UdpServer::Finalize() {
    if (!isInitialized_) return;

    if (socket_ != ~0ULL) {
        closesocket(static_cast<SOCKET>(socket_));
        socket_ = ~0ULL;
    }
    WSACleanup();
    isInitialized_ = false;
}

bool UdpServer::Receive(std::string& outMessage) {
    if (!isInitialized_) return false;

    char buffer[1024];
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    // データを受信（ノンブロッキングなのでデータが無ければすぐに SOCKET_ERROR / WSAEWOULDBLOCK が返る）
    SOCKET sock = static_cast<SOCKET>(socket_);
    int recvLen = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&clientAddr, &clientAddrLen);
    
    if (recvLen == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            // 受信データなし
            return false;
        }
        // その他のエラー
        return false;
    }

    if (recvLen > 0) {
        buffer[recvLen] = '\0';
        outMessage = std::string(buffer);
        return true;
    }

    return false;
}

bool UdpServer::Send(const std::string& message, const std::string& ip, int port) {
    if (!isInitialized_) return false;

    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &destAddr.sin_addr);

    int sendLen = sendto(static_cast<SOCKET>(socket_), message.c_str(), static_cast<int>(message.length()), 0, (sockaddr*)&destAddr, sizeof(destAddr));
    return sendLen != SOCKET_ERROR;
}
