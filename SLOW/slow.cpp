#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include "packet.hpp"

#define IPCRUZ "142.93.184.175"
#define PORTA 7030

int create_socket(){
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        std::cerr << "Erro na criação do socket" << std::endl;
        return 1;
    }
    return sockfd;
}

sockaddr_in local_address(){
    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(PORTA);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    return local;
}

bool bind_socket(int sockfd, const sockaddr_in& addr) {
    if (::bind(sockfd,
               reinterpret_cast<const sockaddr*>(&addr),
               sizeof(addr)) < 0)
    {
        std::cerr << "bind() failed: " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

int main(void){



}
