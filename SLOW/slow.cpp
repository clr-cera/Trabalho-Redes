#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <cstring>

#define IPCRUZ "142.93.184.175"
#define PORTA 7030

class SlowPacket {
public:

    // --- Dados da classe ---
    std::vector<uint8_t> sid;
    uint32_t             sttl;
    bool                 connect;
    bool                 revive;
    bool                 ack;
    bool                 accept;
    bool                 more;
    uint32_t             seqnum;
    uint32_t             acknum;
    uint16_t             window;
    uint8_t              fid;
    uint8_t              fo;
    std::vector<uint8_t> data;

    // --- Construtores ---

    //Construtor padrão
    SlowPacket()
        : sid(16, 0), sttl(0),
          connect(false), revive(false), ack(false), accept(false), more(false),
          seqnum(0), acknum(0), window(0), fid(0), fo(0), data()
    {}

    //Construtor explícito
    SlowPacket(const std::vector<uint8_t>& sid_,
                uint32_t sttl_,
                bool connect_, bool revive_, bool ack_, bool accept_, bool more_,
                uint32_t seqnum_, uint32_t acknum_,
                uint16_t window_, uint8_t fid_, uint8_t fo_,
                const std::vector<uint8_t>& data_){
        sid     = sid_; 
        sttl    = sttl_;
        connect = connect_;
        revive  = revive_;
        ack     = ack_;
        accept  = accept_;
        more    = more_;
        seqnum  = seqnum_; 
        acknum  = acknum_; 
        window  = window_; 
        fid     = fid_; 
        fo      = fo_; 
        data    = data_;
    }

    // --- Interface Pública ---

    //Valida o pacote
    void validate() {
        if (sid.size() != 16)
            std::cout << "SID deve ser exatamente 16 bytes";
        if (sttl > 0x07FFFFFF)
            std::cout << "STTL must fit in 27 bits";
        if (data.size() > 1440)
            std::cout << "Payload must be <= 1440 bytes";
    }

    //Converte o pacote para vetor de bytes para serem enviados
    std::vector<uint8_t> build() {
        validate();
        std::vector<uint8_t> buf;
        buf.reserve(16 + 4 + 4 + 4 + 2 + 1 + 1 + data.size());

        insertBytes(buf, sid);
        insertUint32(buf, buildFlagsAndSttl());
        insertUint32(buf, seqnum);
        insertUint32(buf, acknum);
        insertUint16(buf, window);
        buf.push_back(fid);
        buf.push_back(fo);
        insertBytes(buf, data);

        return buf;
    }

    //Imprime o pacote para depuração
    void print() {
        std::cout << "SID: ";
        for (auto b : sid) std::cout << std::hex << int(b) << " ";
        std::cout << std::dec
                  << "\nSTTL: "   << sttl
                  << "\nConnect: " << connect
                  << "\nRevive: "  << revive
                  << "\nAck: "     << ack
                  << "\nAccept: "  << accept
                  << "\nMore: "    << more
                  << "\nSeqnum: "  << seqnum
                  << "\nAcknum: "  << acknum
                  << "\nWindow: "  << window
                  << "\nFID: "     << int(fid)
                  << "\nFO: "      << int(fo)
                  << "\nData size: "<< data.size()
                  << std::endl;
    }

    //Transforma um vetor de bytes em um novo pacote
    static SlowPacket parse(std::vector<uint8_t>& buf) {
        size_t HEADER = 16 + 4 + 4 + 4 + 2 + 1 + 1;
        if (buf.size() < HEADER)
            std::cout << "Buffer too small for SLOW packet";

        // SID
        std::vector<uint8_t> sid(buf.begin(), buf.begin() + 16);

        // STTL + flags
        uint32_t sf    = readU32(buf, 16);
        uint32_t sttl_  = sf & 0x07FFFFFF;
        uint8_t  flags = (sf >> 27) & 0x1F;

        bool c  = flags & 0x01;
        bool r  = flags & 0x02;
        bool a  = flags & 0x04;
        bool ac = flags & 0x08;
        bool m  = flags & 0x10;

        // Seq e Ack
        uint32_t seq   = readU32(buf, 20);
        uint32_t ack   = readU32(buf, 24);

        // Window, fid, fo
        uint16_t win = readU16(buf, 28);
        uint8_t  fid_ = buf[30];
        uint8_t  fo_  = buf[31];

        // Payload
        std::vector<uint8_t> payload(buf.begin() + 32, buf.end());

        return SlowPacket(sid, sttl_, c, r, a, ac, m, seq, ack, win, fid_, fo_, payload);
    }

private:

    // --- Auxiliares para a construção do pacote ---

    //Insere 32 bits little endian
    static void insertUint32(std::vector<uint8_t>& buf, uint32_t v) {
        buf.push_back(uint8_t( v        & 0xFF));
        buf.push_back(uint8_t((v >> 8 ) & 0xFF));
        buf.push_back(uint8_t((v >> 16) & 0xFF));
        buf.push_back(uint8_t((v >> 24) & 0xFF));
    }

    //Insere 16 bits little endian
    static void insertUint16(std::vector<uint8_t>& buf, uint16_t v) {
        buf.push_back(uint8_t( v        & 0xFF));
        buf.push_back(uint8_t((v >> 8 ) & 0xFF));
    }

    //Insere todos os bytes de um vetor fonte no final do vetor destino
    static void insertBytes(std::vector<uint8_t>& buf, const std::vector<uint8_t>& src) {
        buf.insert(buf.end(), src.begin(), src.end());
    }

    //Coloca todas as flags como bits em um byte
    uint8_t buildFlags() {
        uint8_t f = 0;
        if (connect) f |= 0x01;
        if (revive)  f |= 0x02;
        if (ack)     f |= 0x04;
        if (accept)  f |= 0x08;
        if (more)    f |= 0x10;
        return f;
    }

    //Junta o sttl com as flags
    uint32_t buildFlagsAndSttl() {
        return sttl | (uint32_t(buildFlags()) << 27);
    }

    // --- Auxiliares para o parsing do pacote ---
    
    //Lê 32 bits little endian
    static uint32_t readU32(const std::vector<uint8_t>& buf, size_t p) {
        return uint32_t(buf[p])
             | (uint32_t(buf[p+1]) << 8)
             | (uint32_t(buf[p+2]) << 16)
             | (uint32_t(buf[p+3]) << 24);
    }

    //Lê 16 bits little endian
    static uint16_t readU16(const std::vector<uint8_t>& buf, size_t p) {
        return uint16_t(buf[p])
             | (uint16_t(buf[p+1]) << 8);
    }
};

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
    ssize_t send(const std::vector<uint8_t>& data){
        ssize_t sent = ::sendto(socket_fd, data.data(), data.size(), 0, reinterpret_cast<const sockaddr*>(&remote_addr), sizeof(remote_addr));
        
        if(sent < 0)
            std::cout << "Envio falhou";

        if(static_cast<size_t>(sent) != data.size())
            std::cout << "Enviou menos bytes do que o pedido";

        return sent;
    }

    std::vector<uint8_t> receive(size_t max_length = 1500){
        std::vector<uint8_t> buffer(max_length);

        sockaddr_in src{};
        socklen_t src_len = sizeof(src);

        ssize_t received = ::recvfrom(socket_fd, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&src), &src_len);

        if(received < 0) {
            std::cout << "Recebimento falhou";
        }

        return buffer;
    }

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

//Retorna um pacote connect padrão
SlowPacket connect(){
    std::vector<uint8_t> nil_uuid(16, 0);
    SlowPacket connect_pkt(
        nil_uuid,          // sid
        0,                 // sttl
        true,              // connect
        false,             // revive
        false,             // ack
        false,             // accept
        false,             // more
        0,                 // seqnum
        0,                 // acknum
        1440,              // window
        0,                 // fid
        0,                 // fo
        {}                 // data
    );
    return connect_pkt;
}

int main(void){


    //Cria o socket
    UdpSocket sock = UdpSocket("0.0.0.0", PORTA, IPCRUZ, PORTA);

    std::cout << "Socket criado com sucesso!" << std::endl;

    //Cria um pacote connect
    SlowPacket connect_pkt = connect();

    std::cout << "Pacote Connect:" << std::endl;

    //Imprime o pacote connect
    connect_pkt.print();

    std::cout << "Enviando pacote" << std::endl;
    sock.send(connect_pkt.build());
    std::cout << "Pacote enviado" << std::endl;

    std::cout << "Aguardando resposta..." << std::endl;
    std::vector<uint8_t> reply_raw = sock.receive();
    std::cout << "✔ Recebido " << reply_raw.size() << " bytes.\n\n";

    //Transforma o vetor de bytes recebido em um pacote SlowPacket
    SlowPacket reply_pkt = SlowPacket::parse(reply_raw);
    std::cout << "Pacote recebido:" << std::endl;
    //Imprime o pacote recebido
    reply_pkt.print();

    if (!reply_pkt.connect   && 
            !reply_pkt.revive    &&
            !reply_pkt.ack       &&
            reply_pkt.accept     &&
            !reply_pkt.more)
        {
            std::cout << "[✓] This is a valid Setup (Accept) packet.\n";
        } else {
            std::cout << "[✗] Reply is NOT a Setup/Accept packet.  Flags are:\n"
                 << "    connect=" << reply_pkt.connect
                 << "  revive="   << reply_pkt.revive
                 << "  ack="      << reply_pkt.ack
                 << "  accept="   << reply_pkt.accept
                 << "  more="     << reply_pkt.more
                 << "\n";
        }

    return 0;
}
