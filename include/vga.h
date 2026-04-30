#ifndef VGA_H
#define VGA_H

#define video_memory ((char*)0xB8000)
#define max_rows 25
#define max_cols 80

extern char setcolor;
extern int cursorpos;
extern char coragr;
extern int cursor_x;
extern int cursor_y;
extern char cursor_x_str[10];
extern char cursor_y_str[10];

enum vga_color {
    PRETO = 0,
    AZUL_ESCURO = 1,
    VERDE_ESCURO = 2,
    CIANO_ESCURO = 3,
    VERMELHO_ESCRO = 4,
    MAGENTA_ESCURO = 5,
    MARROM = 6,
    CINZA = 7,
    CINZA_ESCURO = 8,
    AZUL = 9,
    VERDE = 10,
    CIANO = 11,
    VERMELHO = 12,
    MAGENTA = 13,
    AMARELO = 14,
    BRANCO = 15
};

void print(char* msg);
void scroll();
void cursor();
void limpatela();
void cursor_on();
void cor(enum vga_color cor);
void cursor_on();
void set_cursor_pos(int x, int y);
int get_cursor_x();
int get_cursor_y();

#endif