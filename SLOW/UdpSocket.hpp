#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <cstring>

class UdpSocket{
public:
    // --- Dados da classe ---
    std::string     local_ip; 
    uint16_t        local_port; 
    std::string     remote_ip; 
    uint16_t        remote_port;
    int             socket_fd{-1};
    sockaddr_in     local_addr;
    sockaddr_in     remote_addr;

    // --- Construtor ---
    UdpSocket(std::string local_ip_, uint16_t local_port_, std::string remote_ip_, uint16_t remote_port_){
        //Configurando atributos
        local_ip    = local_ip_;
        local_port  = local_port_;
        remote_ip   = remote_ip_;
        remote_port = remote_port_;

        //Configurando o socket
        createSocket();
        localAddress();
        bindSocket();
        remoteAddress();
    }

    // --- Destrutor ---
    ~UdpSocket() {
        if (socket_fd >= 0) {
            ::close(socket_fd);
        }
    }

    // --- Métodos públicos ---

    //Envia um vetor de bytes da origem para o destino configurado
    void send(std::vector<uint8_t>& data){
        ssize_t sent = ::sendto(socket_fd, data.data(), data.size(), 0, reinterpret_cast<const sockaddr*>(&remote_addr), sizeof(remote_addr));
        
        if(sent < 0)
            std::cout << "Envio falhou";

        if(static_cast<size_t>(sent) != data.size())
            std::cout << "Enviou menos bytes do que o pedido";
    }

    std::vector<uint8_t> receive(size_t max_length = 1500);

private:

    //Cria o Socket
    void createSocket(){
        socket_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd < 0) {
            std::cout << "Erro na criação do socket";
        } 
    }

    //Configura o endereço local
    void localAddress(){
        
        local_addr.sin_family = AF_INET;
        local_addr.sin_port   = htons(local_port);

        if (::inet_pton(AF_INET, local_ip.c_str(), &local_addr.sin_addr) != 1) {
            ::close(socket_fd);
            std::cout << "inet_pton() failed for local IP";
        }
    }

    //Associa o socket ao endereço local
    void bindSocket(){
        ::bind(socket_fd, reinterpret_cast<sockaddr*> (&local_addr), sizeof(local_addr));
    }

    //Configura o endereço remoto para onde todas as mensagens serão enviadas
    void remoteAddress(){

        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port   = htons(remote_port);

        if (::inet_pton(AF_INET, remote_ip.c_str(), &remote_addr.sin_addr) != 1) {
            ::close(socket_fd);
            std::cout << "inet_pton() failed for remote IP";
        }
    }



};