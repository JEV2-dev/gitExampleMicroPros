#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "caesar.h"

#define MAX_PAYLOAD 1024

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <server_ip> <puerto> <shift0-25> <texto_plano>\n", argv[0]);
        fprintf(stderr, "Ej:  %s 192.168.1.50 3333 4 Bren_123\n", argv[0]);
        return 1;
    }
    const char *srv_ip = argv[1];
    int port = atoi(argv[2]);
    int shift = atoi(argv[3]) % 26;
    const char *plain = argv[4];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, srv_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "IP invalida: %s\n", srv_ip);
        return 1;
    }
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); close(fd); return 1; }

    size_t n = strlen(plain);
    char cipher[MAX_PAYLOAD];
    if (n > MAX_PAYLOAD - 1) n = MAX_PAYLOAD - 1;
    caesar_encrypt(plain, cipher, n, (uint8_t)shift);

    // buffer = [shift][cipher...]
    char buf[MAX_PAYLOAD];
    buf[0] = (char)(uint8_t)shift;
    memcpy(buf + 1, cipher, n);

    ssize_t sent = send(fd, buf, (int)(n + 1), 0);
    if (sent < 0) { perror("send"); close(fd); return 1; }

    printf("[client] Enviado %zd bytes a %s:%d (shift=%d, plain=\"%s\")\n",
           sent, srv_ip, port, shift, plain);

    close(fd);
    return 0;
}
