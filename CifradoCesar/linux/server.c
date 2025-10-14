#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "caesar.h"

#define MAX_PAYLOAD 1024

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <bind_ip> <puerto>\nEj:   %s 0.0.0.0 3333\n", argv[0], argv[0]);
        return 1;
    }
    const char *bind_ip = argv[1];
    int port = atoi(argv[2]);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "IP invalida: %s\n", bind_ip);
        return 1;
    }
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, 1) < 0) { perror("listen"); return 1; }
    printf("[server] Escuchando en %s:%d ...\n", bind_ip, port);

    struct sockaddr_in cli; socklen_t clen = sizeof(cli);
    int cli_fd = accept(srv, (struct sockaddr*)&cli, &clen);
    if (cli_fd < 0) { perror("accept"); return 1; }

    char cip[MAX_PAYLOAD];
    ssize_t n = recv(cli_fd, cip, sizeof(cip), 0);
    if (n <= 0) { perror("recv"); close(cli_fd); close(srv); return 1; }

    uint8_t shift = (uint8_t)cip[0];
    size_t m = (size_t)(n - 1);
    char plain[MAX_PAYLOAD];
    caesar_decrypt(cip + 1, plain, m, shift);

    // Asegurar null-terminacion para imprimir
    if (m >= sizeof(plain)) m = sizeof(plain) - 1;
    plain[m] = '\0';

    printf("[server] shift=%u, descifrado=\"%s\"\n", shift, plain);

    close(cli_fd);
    close(srv);
    return 0;
}
