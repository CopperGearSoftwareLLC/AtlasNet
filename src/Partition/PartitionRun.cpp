#include "Partition/Partition.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "pch.hpp"
int main(void)
{
     int port = 1235; // Port to check
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // Try to bind the socket to the port
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "Port " << port << " is in use.\n";
    } else {
        std::cout << "Port " << port << " is free.\n";
    }

    close(sock);
    Partition part;
    part.Run();
}
