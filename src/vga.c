#include "vga.h"
#include "io.h"
#include "lib.h"
#include <iso646.h>
#include <stdint.h>

char setcolor = BRANCO;
int cursorpos = 0;
char coragr = BRANCO;
int cursor_x;
int cursor_y;
char cursor_x_str[10];
char cursor_y_str[10];

void print(char* msg) {
    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        }
        else if (msg[i] == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = 79;
            }

            int offset = (cursor_y * 80 + cursor_x) * 2;
            video_memory[offset] = ' ';
            video_memory[offset + 1] = setcolor; 
        }
        else if (msg[i] == '\t') {
            int espacos_tab = 4;
            for (int j = 0; j < espacos_tab; j++) {
                int offset = (cursor_y * 80 + cursor_x) * 2;
                video_memory[offset] = ' ';
                video_memory[offset + 1] = setcolor;
        
                cursor_x++;
                if (cursor_x >= 80) {
                    cursor_x = 0;
                    cursor_y++;
                    break;
                }
            }
        }
        else {
            int offset = (cursor_y * 80 + cursor_x) * 2;
            video_memory[offset] = msg[i];
            video_memory[offset + 1] = setcolor;
            cursor_x++;
        }

        if (cursor_x >= 80) {
            cursor_x = 0;
            cursor_y++;
        }

        if (cursor_y >= 25) {
            scroll();
            cursor_y = 24;
        }
    }
    cursor();
}

void scroll() {
    for (int i = 0; i < 80 * 24 * 2; i++) {
        video_memory[i] = video_memory[i + 80 * 2];
    }
    for (int i = 80 * 24 * 2; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i + 1] = setcolor;
    }
}

void cursor() {
    uint16_t pos = (cursor_y * 80) + cursor_x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));

    itoa(cursor_x, cursor_x_str);
    itoa(cursor_y, cursor_y_str);
}

void cursormov(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    
    uint16_t pos = (cursor_y * 80) + cursor_x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));

    itoa(cursor_x, cursor_x_str);
    itoa(cursor_y, cursor_y_str);
}

void limpatela() {
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';itoa(cursor_x, cursor_x_str); 
        video_memory[i+1] = setcolor;
    }
    cursor_x = 0;
    cursor_y = 0;
    cursor();
}

void cursor_on() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void cor(enum vga_color cor) {
    setcolor = cor;
}

void draw_box(int x, int y, int w, int h, char cor_borda) {
    char s_cor = setcolor;
    cor(cor_borda);

    cursormov(x, y);
    char topo_esq[2] = {0xDA, '\0'}; print(topo_esq);
    for(int i = 0; i < w; i++) {
        char linha[2] = {0xC4, '\0'}; print(linha);
    }
    char topo_dir[2] = {0xBF, '\0'}; print(topo_dir);

    for(int i = 1; i < h; i++) {
        cursormov(x, y + i);
        char lat[2] = {0xB3, '\0'}; print(lat);
        
        cursormov(x + w + 1, y + i);
        print(lat);
    }

    cursormov(x, y + h);
    char baixo_esq[2] = {0xC0, '\0'}; print(baixo_esq);
    for(int i = 0; i < w; i++) {
        char linha[2] = {0xC4, '\0'}; print(linha);
    }
    char baixo_dir[2] = {0xD9, '\0'}; print(baixo_dir);

    cor(s_cor);
}