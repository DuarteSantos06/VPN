#ifndef CONFIG_H
#define CONFIG_H

// A chave partilhada (32 bytes para XChaCha20)
static const unsigned char VPN_KEY[32] = {
    0x53, 0x9b, 0xde, 0x42, 0x11, 0x09, 0x88, 0x77,
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00
};

#define MSG_HANDSHAKE_REQ 1
#define MSG_HANDSHAKE_ACK 2
#define MSG_DATA 3

typedef struct {
    uint8_t type;          // Tipo de mensagem (Handshake ou Dados)
    uint32_t virtual_ip;   // O IP virtual do cliente (útil no handshake)
    // No futuro podes adicionar aqui: uint32_t seq_number, etc.
} vpn_header_t;

// Tamanhos da cifra (baseado na libsodium)
#define NONCE_LEN 24  // Para XChaCha20
#define TAG_LEN 16

#endif