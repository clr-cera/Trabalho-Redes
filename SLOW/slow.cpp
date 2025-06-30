/*
--- INTRODUÇÃO ---

O trabalho foi desenvolvido pelos alunos:

Clara Ernesto de Carvalho       (14559479)
Gabriel Barbosa dos Santos      (14613991)
Renan Parpinelli Scarpin        (14712188)

O código está dividido entre 3 classes:
SlowPacket <- Abstração do segmento para facilitar manipulação e operações mais baixo nível
UdpSocket <- Wrapper de um socket UDP simplificado para realizar apenas as operações necessárias em uma conexão SLOW
SlowConnection <- Classe principal do projeto que implementa o protocolo utilizando as auxiliares.

Após as 3 classes, algumas funções e a Main são definidas apenas para propósitos de testes.
O pensamento foi desenvolver uma biblioteca útil para a troca de dados utilizando o protocolo SLOW.
Portanto não há aplicação com interface nem nada do tipo, apenas envio de mensagens de testes.
Gerenciamento de memória também foi delegado ao programador que estiver desenvolvendo a aplicação.
*/

// --- Importanto bibliotecas necessárias ---
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
#include <fstream>

// --- Definindo constantes que podem facilmente ser alteradas para teste ---
#define REMOTE_IP "142.93.184.175" //IP de destino (slow.gmelodie.com)
#define PORTA 7033 //Porta de destino
#define MAX_DATA_SIZE 1440 //Máximo de dados em um pacote
#define RETRY_LIMIT 3 //Limite máximo de tentativas de retransmissão
#define TIMEOUT_MS 2000 //Timeout para retransmitir

/*
A Classe SlowPacket faz uma abstração do segmento Slow,
para que ele possa ser interpretado e manipulado facilmente como um objeto.

Para ser transmitido o objeto deve ser transformado em vetor de bytes usando o método build.

Um vetor de bytes pode ser transformado de volta em objeto para interpretação e manipulação mais fácil,
utilizando o método parse.
*/
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

    //Valida se o segmento obedece as restrições das especificações
    void validate() {
        if (sid.size() != 16)
            std::cout << "SID deve ser exatamente 16 bytes";
        if (sttl > 0x07FFFFFF)
            std::cout << "STTL deve caber em 27 bits";
        if (data.size() > MAX_DATA_SIZE)
            std::cout << "Payload deve ser menor ou igual que" << MAX_DATA_SIZE << "bytes";
    }

    //Converte o segmento em um vetor de bytes para que possa ser transmitido
    std::vector<uint8_t> build() {
        validate(); //valida o segmento
        std::vector<uint8_t> buf; //Buffer que guardará a versão serializada do segmento
        buf.reserve(16 + 4 + 4 + 4 + 2 + 1 + 1 + data.size()); //reserva o espaço no buffer

        //Insere todos os dados no buffer

        insertBytes(buf, sid);
        insertUint32(buf, buildFlagsAndSttl());
        insertUint32(buf, seqnum);
        insertUint32(buf, acknum);
        insertUint16(buf, window);
        buf.push_back(fid);
        buf.push_back(fo);
        insertBytes(buf, data);

        //Retorna o buffer
        return buf;
    }

    //Imprime o segmento para depuração
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

    //Recebe um segmento serializado em um vetor de bytes em retorna um objeto pacote com as informações do segmento serializado
    static SlowPacket parse(const std::vector<uint8_t>& buf) {
        
        size_t HEADER = 16 + 4 + 4 + 4 + 2 + 1 + 1; //Calcula o tamanho do header
        if (buf.size() < HEADER) //Segmento não pode ser menor que o header
            std::cout << "Buffer muito pequeno para um segmento SLOW";

        // Lê o SID do segmento serializado
        std::vector<uint8_t> sid(buf.begin(), buf.begin() + 16);

        // Lê o STTL e as flags
        uint32_t sf    = readU32(buf, 16);
        uint8_t  flags = sf & 0x1F;
        uint32_t sttl_ = sf >> 5;

        // Interpreta as flags
        bool m  = flags & 0x01;
        bool ac  = flags & 0x02;
        bool a  = flags & 0x04;
        bool r = flags & 0x08;
        bool c  = flags & 0x10;

        // Lê o Seq e Ack
        uint32_t seq   = readU32(buf, 20);
        uint32_t ack   = readU32(buf, 24);

        // Lê o Window, fid, fo
        uint16_t win = readU16(buf, 28);
        uint8_t  fid_ = buf[30];
        uint8_t  fo_  = buf[31];

        // Lê o Payload
        std::vector<uint8_t> payload(buf.begin() + 32, buf.end());

        // Constroi e retorna um segmento Slow em forma de objeto com os dados lidos do buffer
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

/*
Essa classe é um wrapper do socket que cria uma sintaxe mais simples 
e limitada as necessidades de uma conexão SLOW, simplificando o desenvolvimento do Slow Connection.
*/
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

    //Recebe um vetor de bytes qualquer que chege no socket
    std::vector<uint8_t> receive(size_t max_length = 1500){
        if(max_length == 0){
            std::cout << "Tamanho máximo inválido";
        }
        
        //Cria o buffer
        std::vector<uint8_t> buffer(max_length);

        sockaddr_in src{};
        socklen_t src_len = sizeof(src);

        ssize_t received = ::recvfrom(socket_fd, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&src), &src_len);

        if(received < 0) {
            std::cout << "Recebimento falhou";
            return {};
        }

        //Redimensiona o buffer para apenas o tamanho necessário
        buffer.resize(static_cast<size_t>(received));

        return buffer;
    }

    // Configura um timeout de recebimento no socket
    void setReceiveTimeout(int milliseconds){
        if (milliseconds < 0) {
            std::cout << "Timeout não pode ser negativo";
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
            std::cout << "inet_pton() falhou para o IP local";
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
            std::cout << "inet_pton() falhou para o IP remoto";
        }
    }
};

/*
CLASSE PRINCIPAL DO TRABALHO
Essa classe gerencia uma conexão slow com métodos para tudo o que se possa fazer em uma conexão SLOW.
*/
class SlowConnection{
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
    SlowPacket pktData(const std::vector<uint8_t>& data_chunk, uint8_t fid, uint8_t fo, bool more_flag = false, bool revive_flag = false){
        SlowPacket data_pkt(sid, sttl, false, revive_flag, true, false, more_flag, next_seqnum, last_acknum, 1440, fid, fo, data_chunk);
        return data_pkt;
    }

    // --- Auxiliares para operações ---

    //Recebe um pacote e salva os dados no objeto pacote passado por parametro
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

public:

    // --- Dados da classe ---
    UdpSocket               socket;
    std::vector<uint8_t>    sid;
    uint32_t                sttl;
    uint32_t                next_seqnum;
    uint32_t                last_acknum;
    uint16_t                peer_window;
    bool                    session_active;

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

        socket.setReceiveTimeout(TIMEOUT_MS);
    }

    // --- Métodos públicos ---

    //Conecta ao servidor remoto
    bool connect(){

        //Controi o pacote de conexão
        SlowPacket pkt = pktConnect();

        //Envia o pacote de conexão
        for(int attempt = 0; attempt < RETRY_LIMIT; attempt++){
            socket.send(pkt.build());
            SlowPacket response;

            //Verifica se recebeu uma resposta
            if(receivePacket(response)){

                //Resposta é um setup válido
                if(response.accept && !response.connect && !response.revive){

                    //Configura os dados da conexão
                    sid = response.sid;
                    sttl = response.sttl;
                    next_seqnum = response.seqnum + 1;
                    last_acknum = response.acknum;
                    peer_window = response.window;
                    session_active = true;
                    break;
                }
                waitRetry();
            }
        }

        //Enviar um pacote de dados vazio para finalizar o handshake
        for(int attempt = 0; attempt < RETRY_LIMIT; attempt++){
            SlowPacket empty_pkt(sid, sttl, false, false, false, false, false, next_seqnum, last_acknum, 0, 0, 0, {});
            socket.send(empty_pkt.build());
            SlowPacket response;

            //Verifica se recebeu uma resposta
            if(receivePacket(response)){

                //Resposta é um ACK válido
                if(response.ack && response.acknum == empty_pkt.seqnum){
                        next_seqnum++;
                        last_acknum = response.seqnum;
                        peer_window = response.window;
                        return true; // Pacote de dados vazio aceito e handshake finalizado
                }
                waitRetry();
            }
        }

        //Se chegar aqui o limite de tentativas foi excedido e a conexão falhou
        return false;
    }

    //Desconectar
    bool disconnect(){

        //Já foi desconectado
        if(!session_active){
            return true;
        }
        
        //Constroi e envia um segmento de desconexão
        SlowPacket disc_pkt = pktDisconnect();
        socket.send(disc_pkt.build());

        //Recebeu uma resposta, confirmando a desconexão
        SlowPacket response;
        if(receivePacket(response)){

        }
        session_active = false;
        return true;
    }

    //Enviar uma mensagem que pode ser dividida em diversos fragmentos
    bool sendData(const std::vector<uint8_t>& message) {

        //Se a sessão estiver inativa, tenta reviver
        bool revive = false;
        if (!session_active) {
            revive = true;
        }

        //Struct necessária para gerenciar a janela deslizante
        struct Pending {
            uint32_t seqnum;
            uint8_t  fid, fo;
            size_t   offset, length;
            bool     more;
        };

        //Dados necessários para gerenciar o envio
        std::vector<Pending> pending;
        size_t total = message.size();
        size_t offset = 0;
        uint8_t fid = 0, fo = 0;
        uint32_t bytes_in_flight = 0;

        // Loop até enviar todos os dados E todos os segmentos serem reconhecidos
        while (offset < total || !pending.empty()) {

            //Preenche a janela enviando os novos segmentos
            while (offset < total && bytes_in_flight < peer_window) {

                //Fragmenta os dados
                size_t chunk_size = std::min<size_t>(MAX_DATA_SIZE, total - offset);

                // Respeita a janela do Central
                if (bytes_in_flight + chunk_size > peer_window) {
                    chunk_size = peer_window - bytes_in_flight;
                    if (chunk_size == 0) break;
                }
                bool more_flag = (offset + chunk_size < total);

                // Serializa e envia
                std::vector<uint8_t> chunk(
                    message.begin() + offset,
                    message.begin() + offset + chunk_size
                );
                SlowPacket pkt = pktData(chunk, fid, fo, more_flag, revive);
                socket.send(pkt.build());

                // Debugging
                //std::cout << "Enviando Pacote";

                // Buffer para a retransmissão se necessário
                pending.push_back({pkt.seqnum, fid, fo, offset, chunk_size, more_flag});
                bytes_in_flight += chunk_size;
                offset += chunk_size;
                ++fo;
            }

            //Aguarda o próximo ACK ou o timeout
            SlowPacket ack_pkt;
            if (receivePacket(ack_pkt) && ack_pkt.ack) { //Se receber o ACK

                // Debugging
                //std::cout << "ACK RECEBIDO";

                // Atualiza a janela e o ultimo acknum
                peer_window = ack_pkt.window;
                last_acknum = ack_pkt.acknum;

                // Remove segmentos pendentes até acknum
                uint32_t acked = ack_pkt.acknum;
                size_t freed = 0;
                auto it = pending.begin();
                while (it != pending.end()) {
                    if (it->seqnum <= acked) {
                        freed += it->length;
                        it = pending.erase(it);
                    } else {
                        ++it;
                    }
                }
                bytes_in_flight -= freed;
            } else {
                // Timeout ou ack inválido, logo retransmitir todos segmentos não reconhecidos
                for (auto& p : pending) {
                    std::vector<uint8_t> chunk(
                        message.begin() + p.offset,
                        message.begin() + p.offset + p.length
                    );
                    SlowPacket retry_pkt = pktData(chunk, p.fid, p.fo, p.more);
                    socket.send(retry_pkt.build());

                    // Debugging
                    //std::cout << "Re-Enviando Pacote porque não recebeu ACK";
                }
                waitRetry();
            }
        }

        return true;
    }

};

// Converte uma string para um vetor de bytes a ser enviado, apenas facilita testes
std::vector<uint8_t> stringToBytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

// Gera um vetor uint8_t de dados aleatórios para testes
std::vector<uint8_t> generateRandomData(size_t size) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = rand() % 256; // Gera um byte aleatório
    }
    return data;
}

//Main para testar a biblioteca
int main() {
    // Testando a classe e seus métodos com prints de debug

    std::cout << "Iniciando testes do protocolo SLOW:" << std::endl;

    // Conexão
    std::cout << "\n---- Conectando ----" << std::endl;

    SlowConnection conn("0.0.0.0", 7033, REMOTE_IP, PORTA);
    if (conn.connect()) {
        std::cout << "Conexão estabelecida com sucesso" << std::endl;
    } else {
        std::cout << "Conexão falhou!" << std::endl;
        return 1;
    }

    // Enviando dados
    std::cout << "\n---- Enviando dados ----" << std::endl;

    //Aqui é gerado 12 mil bytes aleatórios de dados para testes
    //A quantidade é para que eles não caibam em um único segmento e a fragmentação e janela deslizante entre em efeito
    std::vector<uint8_t> message = generateRandomData(12000);
    if (conn.sendData(message)) {
        std::cout << "Dados enviados com sucesso!" << std::endl;
    } else {
        std::cout << "Envio de dados falhou!" << std::endl;
        return 1;
    }

    // Desconectando
    std::cout << "\n---- Desconectando ----" << std::endl;

    try {
        conn.disconnect();
        std::cout << "Desconectado com sucesso!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Dexconexão falhou" << std::endl;
        return 1;
    }

    //Enviando dados depois de desconectar através do revive automático
    std::cout << "\n---- Revivendo e enviando dados após desconexão ----" << std::endl;
    if (conn.sendData(message)) {
        std::cout << "Dados enviados com revive com sucesso!" << std::endl;
    } else {
        std::cout << "Envio de dados com revive falhou!" << std::endl;
        return 1;
    }

    std::cout << "\n Testes concluídos!" << std::endl;
    return 0;
}
