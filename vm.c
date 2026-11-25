#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int pagina;
    int offset;
} Endereco;

Endereco traduzir(long v)
{
    Endereco e;
    e.pagina = (v >> 8) & 0xFF;
    e.offset = v & 0xFF;
    return e;
}

int main(int argc, char *argv[])
{
    char linha[64];
    char *fim;
    long v;
    int n = 0;
    Endereco e;

    if (argc != 4) {
        fprintf(stderr, "Uso: %s <arquivo> <fifo|lru> <fifo|lru>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[2], "fifo") != 0 && strcmp(argv[2], "lru") != 0) {
        fprintf(stderr, "Erro: algoritmo de página inválido\n");
        return 1;
    }

    if (strcmp(argv[3], "fifo") != 0 && strcmp(argv[3], "lru") != 0) {
        fprintf(stderr, "Erro: algoritmo de TLB inválido\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir %s\n", argv[1]);
        return 1;
    }

    while (fgets(linha, sizeof(linha), f) != NULL) {
        n++;
        v = strtol(linha, &fim, 10);
        if (fim == linha || (*fim != '\n' && *fim != '\0') || v < 0 || v > 65535) {
            fprintf(stderr, "Erro: formato inválido na linha %d\n", n);
            fclose(f);
            return 1;
        }
    }

    rewind(f);

    while (fgets(linha, sizeof(linha), f) != NULL) {
        v = strtol(linha, NULL, 10);
        e = traduzir(v);
        printf("Endereço: %ld Página: %d Offset: %d\n", v, e.pagina, e.offset);
    }

    fclose(f);
    return 0;
}