#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM_PAGINA 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 256

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

    int tabela_paginas[NUM_PAGINAS];
    signed char memoria[NUM_FRAMES][TAM_PAGINA];
    int proximo_frame = 0;
    int page_faults = 0;
    int total = 0;
    int frame;
    int fisico;
    signed char valor;
    int i;

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

    FILE *backing = fopen("BACKING_STORE.bin", "rb");
    if (backing == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir BACKING_STORE.bin\n");
        fclose(f);
        return 1;
    }

    FILE *saida = fopen("correct.txt", "w");
    if (saida == NULL) {
        fprintf(stderr, "Erro: não foi possível criar correct.txt\n");
        fclose(f);
        fclose(backing);
        return 1;
    }

    for (i = 0; i < NUM_PAGINAS; i++) {
        tabela_paginas[i] = -1;
    }

    while (fgets(linha, sizeof(linha), f) != NULL) {
        v = strtol(linha, NULL, 10);
        e = traduzir(v);
        total++;

        if (tabela_paginas[e.pagina] == -1) {
            page_faults++;

            if (fseek(backing, (long)e.pagina * TAM_PAGINA, SEEK_SET) != 0 ||
                fread(memoria[proximo_frame], 1, TAM_PAGINA, backing) != TAM_PAGINA) {
                fprintf(stderr, "Erro: falha ao ler a página %d do BACKING_STORE.bin\n", e.pagina);
                fclose(f);
                fclose(backing);
                fclose(saida);
                return 1;
            }

            tabela_paginas[e.pagina] = proximo_frame;
            proximo_frame++;
        }

        frame = tabela_paginas[e.pagina];
        fisico = frame * TAM_PAGINA + e.offset;
        valor = memoria[frame][e.offset];

        fprintf(saida, "Virtual address: %ld Physical address: %d Value: %d\n", v, fisico, valor);
    }

    fprintf(saida, "Number of Translated Addresses = %d\n", total);
    fprintf(saida, "Page Faults = %d\n", page_faults);
    fprintf(saida, "Page Fault Rate = %.3f\n", (double)page_faults / total);

    fclose(f);
    fclose(backing);
    fclose(saida);
    return 0;
}