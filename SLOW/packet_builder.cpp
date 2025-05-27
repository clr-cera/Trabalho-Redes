//This can be altered, split and organized into multiple files in the future

#include <bits/stdc++.h>

// Função auxiliar para inserir um valor de 32 bits em um vetor de bytes little endian
void insert_uint32(std::vector<uint8_t>& vetor, uint32_t valor) {
    vetor.push_back(uint8_t( valor        & 0xFF));
    vetor.push_back(uint8_t((valor >> 8 ) & 0xFF));
    vetor.push_back(uint8_t((valor >> 16) & 0xFF));
    vetor.push_back(uint8_t((valor >> 24) & 0xFF));
}
// Função auxiliar para inserir um valor de 16 bits em um vetor de bytes little endian
void insert_uint16(std::vector<uint8_t>& vec, uint16_t value) {
    vec.push_back(static_cast<uint8_t>(value));
    vec.push_back(static_cast<uint8_t>(value >> 8));
}
// Função auxiliar para inserir um vetor de bytes em um outro vetor de bytes
void insert_bytes(std::vector<uint8_t>& vec, const std::vector<uint8_t>& bytes) {
    vec.insert(vec.end(), bytes.begin(), bytes.end());
}

//Struct do pacote SLOW
struct slow_packet{
    const std::vector<uint8_t> sid;
    uint32_t sttl;
    
    //Flags
    bool connect;
    bool revive;
    bool ack;
    bool accept;
    bool more;

    uint32_t seqnum;
    uint32_t acknum;

    uint16_t window;
    uint8_t fid;
    uint8_t fo;

    std::vector<uint8_t> data;

};
typedef struct slow_packet* Slow_packet;

void validate_slow_packet(const slow_packet* p) {
  if (!p) throw std::invalid_argument("ponteiro nulo.");
  if (p->sid.size() != 16)
    throw std::invalid_argument("SID deve ser 16 bytes");
  if (p->sttl > 0x07FFFFFF)
    throw std::out_of_range("STTL deve caber em 27 bits");
  if (p->data.size() > 1440)
    throw std::out_of_range("Dados devem ser <= 1440 bytes");
}

uint8_t build_flags(Slow_packet packet) {
    uint8_t flags = 0;
    if (packet->connect) flags |= 0x01; // bit 0
    if (packet->revive) flags |= 0x02; // bit 1
    if (packet->ack) flags |= 0x04; // bit 2
    if (packet->accept) flags |= 0x08; // bit 3
    if (packet->more) flags |= 0x10; // bit 4
    return flags;
}

uint32_t build_flags_and_sttl(Slow_packet packet) {
    // Inicia apenas com o sttl de 32 bits
    uint32_t sttl_and_flags = packet->sttl;

    // Adiciona as flags ao sttl_and_flags
    sttl_and_flags |= (static_cast<uint32_t>(build_flags(packet)) << 27); // As flags ocupam os 5 bits mais significativos
    
    // Retorna o sttl e as flags combinados
    return sttl_and_flags;
}
//Função para transformar um pacote SLOW em um vetor de bytes para ser transmitido
std::vector<uint8_t> build_slow_packet(Slow_packet packet) {
    
    std::vector<uint8_t> result;

    result.reserve(16 + 4 + 4 + 4 + 2 + 1 + 1 + packet->data.size());

    // Adiciona o SID
    insert_bytes(result, packet->sid);

    // Adiciona o STTL e as flags
    insert_uint32(result, build_flags_and_sttl(packet));

    // Adiciona o seqnum e acknum
    insert_uint32(result, packet->seqnum);
    insert_uint32(result, packet->acknum);

    // Adiciona a janela, fid e fo
    insert_uint16(result, packet->window);
    result.push_back(packet->fid);
    result.push_back(packet->fo);

    // Adiciona os dados
    insert_bytes(result, packet->data);

    return result;
}

std::vector<uint8_t> validate_and_build_slow_packet(Slow_packet packet) {
    
    validate_slow_packet(packet);

    return build_slow_packet(packet);
}