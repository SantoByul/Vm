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
        fprintf(stderr, "Erro: não foi possível criar correct.txt\n");
        fclose(f);
        fclose(backing);
        return 1;
    }

    for (i = 0; i < NUM_PAGINAS; i++) {
        tabela_paginas[i] = -1;
    }

    for (i = 0; i < NUM_FRAMES; i++) {
        frame_para_pagina[i] = -1;
        ultimo_uso[i] = 0;
    }

    while (fgets(linha, sizeof(linha), f) != NULL) {
        if (linha_vazia(linha)) {
            continue;
        }

        v = strtol(linha, NULL, 10);
        e = traduzir(v);
        total++;

        if (tabela_paginas[e.pagina] == -1) {
            page_faults++;

            if (frames_usados < NUM_FRAMES) {
                frame = frames_usados;
                frames_usados++;
            } else if (usar_lru) {
                frame = 0;
                for (i = 1; i < NUM_FRAMES; i++) {
                    if (ultimo_uso[i] < ultimo_uso[frame]) {
                        frame = i;
                    }
                }
                tabela_paginas[frame_para_pagina[frame]] = -1;
            } else {
                frame = fifo_proximo;
                tabela_paginas[frame_para_pagina[frame]] = -1;
                fifo_proximo = (fifo_proximo + 1) % NUM_FRAMES;
            }

            if (fseek(backing, (long)e.pagina * TAM_PAGINA, SEEK_SET) != 0 ||
                fread(memoria[frame], 1, TAM_PAGINA, backing) != TAM_PAGINA) {
                fprintf(stderr, "Erro: falha ao ler a página %d do BACKING_STORE.bin\n", e.pagina);
                fclose(f);
                fclose(backing);
                fclose(saida);
                remove("correct.txt");
                return 1;
            }

            tabela_paginas[e.pagina] = frame;
            frame_para_pagina[frame] = e.pagina;
        }

        frame = tabela_paginas[e.pagina];
        relogio++;
        ultimo_uso[frame] = relogio;

        fisico = frame * TAM_PAGINA + e.offset;
        valor = memoria[frame][e.offset];

        fprintf(saida, "Virtual address: %ld Physical address: %d Value: %d\n", v, fisico, valor);
    }

    fprintf(saida, "Number of Translated Addresses = %d\n", total);
    fprintf(saida, "Page Faults = %d\n", page_faults);
    fprintf(saida, "Page Fault Rate = %.3f\n", (double)page_faults / total);

    fclose(f);
    fclose(backing);

    if (fclose(saida) != 0) {
        fprintf(stderr, "Erro: falha ao gravar correct.txt\n");
        return 1;
    }

    return 0;
}