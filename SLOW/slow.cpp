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
#include <bitset>

#define REMOTE_IP "142.93.184.175" //IP de destino
#define PORTA 7033 //Porta de destino
#define MAX_DATA_SIZE 1440 //Máximo de dados em um pacote
#define RETRY_LIMIT 3 //Limite máximo de retransmissão
#define TIMEOUT_MS 2000 //Timeout para retransmitir



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
        : sid(16, 0), sttl(0), connect(false), revive(false), ack(false), accept(false), more(false),
          seqnum(0), acknum(0), window(0), fid(0), fo(0) {}

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
        uint32_t sf_combined = buildFlagsAndSttl();
        std::bitset<32> bits(sf_combined);
        std::cout << "SID: ";
        for (auto b : sid) std::cout << std::hex << int(b) << " ";
        std::cout << std::dec
                  << "\nSTTL: "   << sttl
                  << "\nSTTL+Flags (binary): " << bits
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
        if (!data.empty()) {
            std::cout << "Texto dos dados: ";
            for (uint8_t byte : data) {
                // Transforma cada byte em caracter
                std::cout << static_cast<char>(byte);
            }
            std::cout << std::endl;
        }
    }

    //Transforma um vetor de bytes em um novo pacote
    static SlowPacket parse(const std::vector<uint8_t>& buf) {
        size_t HEADER = 16 + 4 + 4 + 4 + 2 + 1 + 1;
        if (buf.size() < HEADER)
            std::cout << "Buffer too small for SLOW packet";

        // SID
        std::vector<uint8_t> sid(buf.begin(), buf.begin() + 16);

        // STTL + flags
        uint32_t sf    = readU32(buf, 16);
        uint8_t  flags = sf & 0x1F;
        uint32_t sttl_ = sf >> 5;

        bool m  = flags & 0x01;
        bool ac  = flags & 0x02;
        bool a  = flags & 0x04;
        bool r = flags & 0x08;
        bool c  = flags & 0x10;

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
        buf.push_back(uint8_t( v        & 0xFF));   // byte-0
        buf.push_back(uint8_t((v >> 8 ) & 0xFF));   // byte-1
        buf.push_back(uint8_t((v >> 16) & 0xFF));   // byte-2
        buf.push_back(uint8_t((v >> 24) & 0xFF));   // byte-3
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

    //Junta as flags com o sttl
    uint32_t buildFlagsAndSttl() const {
        uint32_t word = (sttl & 0x07FFFFFF) << 5;

        if (connect) word |= (1u << 4);
        if (revive)  word |= (1u << 3);
        if (ack)     word |= (1u << 2);
        if (accept)  word |= (1u << 1);
        if (more)    word |= (1u << 0);

        return word;
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
        if(max_length == 0){
            std::cout << "Tamanho máximo inválido";
        }
        
        std::vector<uint8_t> buffer(max_length);

        sockaddr_in src{};
        socklen_t src_len = sizeof(src);

        ssize_t received = ::recvfrom(socket_fd, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&src), &src_len);

        if(received < 0) {
            std::cout << "Recebimento falhou";
        }

        buffer.resize(static_cast<size_t>(received));

        return buffer;
    }

    void setReceiveTimeout(int milliseconds){
        if (milliseconds < 0) {
            std::cout << "Timeout must be non-negative";
        }
        timeval tv{};
        tv.tv_sec  = milliseconds / 1000;
        tv.tv_usec = (milliseconds % 1000) * 1000;

        ::setsockopt(socket_fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
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

class SlowConnection{
public:

    // --- Dados da classe ---
    UdpSocket               socket;
    std::vector<uint8_t>    sid;
    uint32_t                sttl;
    uint32_t                next_seqnum;
    uint32_t                last_acknum;
    uint16_t                peer_window;
    bool                    session_active;
    bool                    revived;

    // --- Construtores e destrutores ---
    SlowConnection(const std::string& local_ip, uint16_t local_port, const std::string& remote_ip, uint16_t remote_port)
        : socket(local_ip, local_port, remote_ip, remote_port)
    {
        std::vector<uint8_t> nil_uuid(16, 0);
        sid = nil_uuid;
        sttl = 0;
        next_seqnum = 0;
        last_acknum = 0;
        peer_window = 0;
        session_active = false;
        revived = false;

        socket.setReceiveTimeout(TIMEOUT_MS);
    }

    // --- Métodos públicos ---

    //Conecta ao servidor remoto
    bool connect(){
        SlowPacket pkt = pktConnect();
        for(int attempt = 0; attempt < RETRY_LIMIT; attempt++){
            socket.send(pkt.build());
            SlowPacket response;

            if(receivePacket(response)){
                if(response.accept && !response.connect && !response.revive){
                    sid = response.sid;
                    sttl = response.sttl;
                    next_seqnum = response.seqnum + 1;
                    last_acknum = response.acknum;
                    peer_window = response.window;
                    session_active = true;
                    revived = false;
                    return true; // Conexão bem-sucedida
                }
                waitRetry();
        }
    }

    bool sendData(const std::vector<uint8_t>& data_chunk){
        if(!session_active){
            std::cout << "Sessão não está ativa. Conecte primeiro.";
            return false;
        }

        uint8_t fid = 0; // ID do fragmento de dados
        uint8_t fo = 0;  // Offset do fragmento de dados
        uint8_t total = payload.size();
        uint8_t offset = 0;

        while(offset < total){
            size_t chunk_size = std::min(MAX_DATA_SIZE, total - offset);
            bool more_flag = ((offset + chunk_size) < total);

            // WIP
        }
    }

private:

    // --- Auxiliares para a construção de pacotes ---

    //Cria um pacote Connect padrão
    SlowPacket pktConnect(){
        SlowPacket connect_pkt(sid, 0, true, false, false, false, false, 0, 0, 1440, 0, 0, {});
        return connect_pkt;
    }

    //Cria um pacote Disconnect padrão
    SlowPacket pktDisconnect(){
        SlowPacket disconnect_pkt(sid, sttl, true, true, true, false, false, next_seqnum, last_acknum, 0, 0, 0, {});
        return disconnect_pkt;
    }

    //Cria um pacote de dados
    SlowPacket pktData(const std::vector<uint8_t>& data_chunk, uint8_t fid, uint8_t fo, bool more_flag){
        bool revive_flag = false;
        if(!session_active){
            revive_flag = true;
        }
        SlowPacket data_pkt(sid, sttl, false, revive_flag, true, false, more_flag, next_seqnum, last_acknum, 1440, fid, fo, data_chunk);
        return data_pkt;
    }

    // --- Auxiliares para operações ---

    //Recebe um pacote e salva os dados no pacote passado por parametro
    bool receivePacket(SlowPacket& pkt) {
        std::vector<uint8_t> raw_data = socket.receive();
        if (raw_data.empty()) {
            std::cout << "Nenhum dado recebido";
            return false;
        }
        pkt = SlowPacket::parse(raw_data);
        return true;
    }

    //Aguarda TIMEOUT_MS
    void waitRetry(){
        usleep(TIMEOUT_MS * 1000);
    }

};

int main(void){


    //Cria o socket
    UdpSocket sock = UdpSocket("0.0.0.0", PORTA, REMOTE_IP, PORTA);

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

    bool connected = false;

    if (!reply_pkt.connect   && 
            !reply_pkt.revive    &&
            !reply_pkt.ack       &&
            reply_pkt.accept     &&
            !reply_pkt.more)
        {
            std::cout << "Conexão iniciada.\n";
            connected = true;
        } else {
            std::cout << "Conexão rejeitada\n"
                 << "    connect=" << reply_pkt.connect
                 << "  revive="   << reply_pkt.revive
                 << "  ack="      << reply_pkt.ack
                 << "  accept="   << reply_pkt.accept
                 << "  more="     << reply_pkt.more
                 << "\n";
        }
    
    if(!connected) {
        std::cout << "Conexão falhou.\n";
        return 1;
    }
    std::cout << "Conexão estabelecida com sucesso!\n";
    std::cout << "Enviando pacote de teste..." << std::endl;
    // Envia um pacote de teste
    SlowPacket test_pkt(
        reply_pkt.sid,      // sid
        0,                  // sttl
        false,              // connect
        false,              // revive
        false,              // ack
        false,              // accept
        false,              // more
        1,                  // seqnum
        0,                  // acknum
        1440,               // window
        0,                  // fid
        0,                  // fo
        {'T', 'e', 's', 't'}// data
    );
    test_pkt.print();
    sock.send(test_pkt.build());
    std::cout << "Pacote de teste enviado." << std::endl;
    std::cout << "Aguardando resposta do pacote de teste..." << std::endl;
    std::vector<uint8_t> test_reply_raw = sock.receive();
    std::cout << "✔ Recebido " << test_reply_raw.size() << " bytes.\n\n";
    SlowPacket::parse(test_reply_raw).print();
    std::cout << "Resposta do pacote de teste recebida." << std::endl;
    std::cout << "Fechando conexão..." << std::endl;
    // Encerra a conexão
    SlowPacket close_pkt(
        reply_pkt.sid,      // sid
        0,                  // sttl
        true,              // connect
        true,              // revive
        true,              // ack
        false,              // accept
        false,              // more
        2,                  // seqnum
        0,                  // acknum
        1440,               // window
        0,                  // fid
        0,                  // fo
        {}                  // data
    );
    close_pkt.print();
    sock.send(close_pkt.build());
    std::cout << "Conexão encerrada." << std::endl;
    std::cout << "Fechando socket..." << std::endl;
    // Fecha o socket
    sock.~UdpSocket();
    std::cout << "Socket fechado." << std::endl;
    std::cout << "Programa finalizado com sucesso." << std::endl;

    // Retorna 0 para indicar sucesso
    return 0;
}
