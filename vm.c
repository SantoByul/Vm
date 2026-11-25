#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TAM_PAGINA 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 128
#define TAM_LINHA 64
#define ENDERECO_MAX 65535

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

int linha_vazia(const char *s)
{
    while (*s) {
        if (!isspace((unsigned char)*s)) {
            return 0;
        }
        s++;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    char linha[TAM_LINHA];
    char *fim;
    long v;
    int n = 0;
    int validos = 0;
    int primeira_vazia = 0;
    Endereco e;

    int tabela_paginas[NUM_PAGINAS];
    int frame_para_pagina[NUM_FRAMES];
    long ultimo_uso[NUM_FRAMES];
    signed char memoria[NUM_FRAMES][TAM_PAGINA];
    int frames_usados = 0;
    int fifo_proximo = 0;
    long relogio = 0;
    int usar_lru;
    int page_faults = 0;
    int total = 0;
    int frame;
    int fisico;
    signed char valor;
    int i;

    if (argc != 4) {
        fprintf(stderr, "Erro: número de argumentos inválido (recebidos %d, esperados 3)\n", argc - 1);
        fprintf(stderr, "Uso: %s <arquivo> <fifo|lru> <fifo|lru>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[2], "fifo") != 0 && strcmp(argv[2], "lru") != 0) {
        fprintf(stderr, "Erro: algoritmo de página '%s' inválido. Use fifo ou lru\n", argv[2]);
        return 1;
    }

    if (strcmp(argv[3], "fifo") != 0 && strcmp(argv[3], "lru") != 0) {
        fprintf(stderr, "Erro: algoritmo de TLB '%s' inválido. Use fifo ou lru\n", argv[3]);
        return 1;
    }

    usar_lru = strcmp(argv[2], "lru") == 0;

    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir %s\n", argv[1]);
        return 1;
    }

    while (fgets(linha, sizeof(linha), f) != NULL) {
        n++;

        if (strchr(linha, '\n') == NULL && !feof(f)) {
            fprintf(stderr, "Erro: linha %d é longa demais\n", n);
            fclose(f);
            return 1;
        }

        if (linha_vazia(linha)) {
            if (primeira_vazia == 0) {
                primeira_vazia = n;
            }
            continue;
        }

        if (primeira_vazia != 0) {
            fprintf(stderr, "Erro: linha %d está vazia\n", primeira_vazia);
            fclose(f);
            return 1;
        }

        if (!isdigit((unsigned char)linha[0])) {
            fprintf(stderr, "Erro: formato inválido na linha %d\n", n);
            fclose(f);
            return 1;
        }

        v = strtol(linha, &fim, 10);
        while (isspace((unsigned char)*fim)) {
            fim++;
        }

        if (*fim != '\0' || v < 0 || v > ENDERECO_MAX) {
            fprintf(stderr, "Erro: formato inválido na linha %d\n", n);
            fclose(f);
            return 1;
        }

        validos++;
    }

    if (ferror(f)) {
        fprintf(stderr, "Erro: falha ao ler %s\n", argv[1]);
        fclose(f);
        return 1;
    }

    if (validos == 0) {
        fprintf(stderr, "Erro: o arquivo %s não contém endereços\n", argv[1]);
        fclose(f);
        return 1;
    }

    rewind(f);

    FILE *backing = fopen("BACKING_STORE.bin", "rb");
    if (backing == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir BACKING_STORE.bin\n");
        fclose(f);
        return 1;
    }

    if (fseek(backing, 0, SEEK_END) != 0 || ftell(backing) < (long)NUM_PAGINAS * TAM_PAGINA) {
        fprintf(stderr, "Erro: BACKING_STORE.bin inválido ou menor que %d bytes\n", NUM_PAGINAS * TAM_PAGINA);
        fclose(f);
        fclose(backing);
        return 1;
    }

    rewind(backing);

    FILE *saida = fopen("correct.txt", "w");
    if (saida == NULL) {
        fprintf(stderr, "Erro: não foi possível criar