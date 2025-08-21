#pragma once
#include <string>
#include <vector>

// --- Platform-Specific Includes for Networking ---
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    // Define Windows-specific types for compatibility on other platforms
    using SOCKET = int;
const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
#define closesocket close
#endif

// Manages a TCP socket for network communication.
class TcpSocket {
private:
    SOCKET sock;
#ifdef _WIN32
    WSADATA wsaData;
    bool wsa_initialized;
#endif
public:
    TcpSocket();
    ~TcpSocket();
    bool connectToServer(const std::string& ip_address, int port);
    bool listenOnPort(int port);
    SOCKET acceptConnection();
    int sendData(const std::vector<char>& data);
    std::vector<char> receiveData();
    void closeSocket();
};