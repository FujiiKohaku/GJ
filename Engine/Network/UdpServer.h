#pragma once

#include <string>
#include <vector>
#include <cstdint>

class UdpServer {
public:
    UdpServer() = default;
    ~UdpServer();

    // サーバーの初期化（ポート番号指定）
    bool Initialize(int port);

    // 終了処理
    void Finalize();

    // ノンブロッキングでの受信。データがあれば outMessage に入れて true を返す
    bool Receive(std::string& outMessage);

    // 指定のIP・ポートにデータを送信
    bool Send(const std::string& message, const std::string& ip, int port);

private:
    uintptr_t socket_ = ~0ULL; // INVALID_SOCKET
    bool isInitialized_ = false;
};
