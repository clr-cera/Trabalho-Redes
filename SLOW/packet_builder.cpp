//This can be altered, split and organized into multiple files in the future

#include <bits/stdc++.h>

class slow_packet {
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
    slow_packet()
        : sid(16, 0), sttl(0),
          connect(false), revive(false), ack(false), accept(false), more(false),
          seqnum(0), acknum(0), window(0), fid(0), fo(0), data()
    {}

    //Construtor explícito
    slow_packet(const std::vector<uint8_t>& sid_,
                uint32_t sttl_,
                bool connect_, bool revive_, bool ack_, bool accept_, bool more_,
                uint32_t seqnum_, uint32_t acknum_,
                uint16_t window_, uint8_t fid_, uint8_t fo_,
                const std::vector<uint8_t>& data_)
        : sid(sid_), sttl(sttl_),
          connect(connect_), revive(revive_), ack(ack_), accept(accept_), more(more_),
          seqnum(seqnum_), acknum(acknum_), window(window_), fid(fid_), fo(fo_), data(data_)
    {}

    // --- Interface Pública ---

    //Valida o pacote
    void validate() const {
        if (sid.size() != 16)
            throw std::invalid_argument("SID must be exactly 16 bytes");
        if (sttl > 0x07FFFFFF)
            throw std::out_of_range("STTL must fit in 27 bits");
        if (data.size() > 1440)
            throw std::out_of_range("Payload must be <= 1440 bytes");
    }

    //Converte o pacote para vetor de bytes para serem enviados
    std::vector<uint8_t> build() const {
        validate();
        std::vector<uint8_t> buf;
        buf.reserve(16 + 4 + 4 + 4 + 2 + 1 + 1 + data.size());

        insert_bytes(buf, sid);
        insert_uint32(buf, build_flags_and_sttl());
        insert_uint32(buf, seqnum);
        insert_uint32(buf, acknum);
        insert_uint16(buf, window);
        buf.push_back(fid);
        buf.push_back(fo);
        insert_bytes(buf, data);

        return buf;
    }

    //Imprime o pacote para depuração
    void print() const {
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
    static slow_packet parse(const std::vector<uint8_t>& buf) {
        constexpr size_t HEADER = 16 + 4 + 4 + 4 + 2 + 1 + 1;
        if (buf.size() < HEADER)
            throw std::invalid_argument("Buffer too small for SLOW packet");

        // SID
        std::vector<uint8_t> sid(buf.begin(), buf.begin() + 16);

        // STTL + flags
        uint32_t sf    = read_u32(buf, 16);
        uint32_t sttl_  = sf & 0x07FFFFFF;
        uint8_t  flags = (sf >> 27) & 0x1F;

        bool c  = flags & 0x01;
        bool r  = flags & 0x02;
        bool a  = flags & 0x04;
        bool ac = flags & 0x08;
        bool m  = flags & 0x10;

        // Seq e Ack
        uint32_t seq   = read_u32(buf, 20);
        uint32_t ack   = read_u32(buf, 24);

        // Window, fid, fo
        uint16_t win = read_u16(buf, 28);
        uint8_t  fid_ = buf[30];
        uint8_t  fo_  = buf[31];

        // Payload
        std::vector<uint8_t> payload(buf.begin() + 32, buf.end());

        return slow_packet(sid, sttl_, c, r, a, ac, m, seq, ack, win, fid_, fo_, payload);
    }

private:

    // --- Auxiliares para a construção do pacote ---

    //Insere 32 bits little endian
    static void insert_uint32(std::vector<uint8_t>& buf, uint32_t v) {
        buf.push_back(uint8_t( v        & 0xFF));
        buf.push_back(uint8_t((v >> 8 ) & 0xFF));
        buf.push_back(uint8_t((v >> 16) & 0xFF));
        buf.push_back(uint8_t((v >> 24) & 0xFF));
    }

    //Insere 16 bits little endian
    static void insert_uint16(std::vector<uint8_t>& buf, uint16_t v) {
        buf.push_back(uint8_t( v        & 0xFF));
        buf.push_back(uint8_t((v >> 8 ) & 0xFF));
    }

    //Insere todos os bytes de um vetor fonte no final do vetor destino
    static void insert_bytes(std::vector<uint8_t>& buf, const std::vector<uint8_t>& src) {
        buf.insert(buf.end(), src.begin(), src.end());
    }

    //Coloca todas as flags como bits em um byte
    uint8_t build_flags() const {
        uint8_t f = 0;
        if (connect) f |= 0x01;
        if (revive)  f |= 0x02;
        if (ack)     f |= 0x04;
        if (accept)  f |= 0x08;
        if (more)    f |= 0x10;
        return f;
    }

    //Junta o sttl com as flags
    uint32_t build_flags_and_sttl() const {
        return sttl | (uint32_t(build_flags()) << 27);
    }

    // --- Auxiliares para o parsing do pacote ---
    
    //Lê 32 bits little endian
    static uint32_t read_u32(const std::vector<uint8_t>& buf, size_t p) {
        return uint32_t(buf[p])
             | (uint32_t(buf[p+1]) << 8)
             | (uint32_t(buf[p+2]) << 16)
             | (uint32_t(buf[p+3]) << 24);
    }

    //Lê 16 bits little endian
    static uint16_t read_u16(const std::vector<uint8_t>& buf, size_t p) {
        return uint16_t(buf[p])
             | (uint16_t(buf[p+1]) << 8);
    }
};
