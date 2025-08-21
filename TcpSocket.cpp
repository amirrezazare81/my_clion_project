#include "TcpSocket.h"
#include <stdexcept>

TcpSocket::TcpSocket() : sock(INVALID_SOCKET) {
#ifdef _WIN32
    wsa_initialized = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    if (!wsa_initialized) throw std::runtime_error("WSAStartup failed.");
#endif
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) throw std::runtime_error("Socket creation failed.");
}

TcpSocket::~TcpSocket() {
    closeSocket();
#ifdef _WIN32
    if (wsa_initialized) WSACleanup();
#endif
}

bool TcpSocket::connectToServer(const std::string& ip_address, int port) {
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_address.c_str(), &server_addr.sin_addr);
    return connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != SOCKET_ERROR;
}

bool TcpSocket::listenOnPort(int port) {
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) return false;
    listen(sock, SOMAXCONN);
    return true;
}

SOCKET TcpSocket::acceptConnection() {
    return accept(sock, NULL, NULL);
}

int TcpSocket::sendData(const std::vector<char>& data) {
    return send(sock, data.data(), (int)data.size(), 0);
}

std::vector<char> TcpSocket::receiveData() {
    char buffer[4096];
    int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) return std::vector<char>(buffer, buffer + bytes_received);
    return {};
}

void TcpSocket::closeSocket() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}
