#include "vga.h"
#include "io.h"
#include "lib.h"
#include <stdint.h>

char setcolor = BRANCO;
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

void set_cursor_pos(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    cursor();
}

int get_cursor_x() {
    return cursor_x;
}

int get_cursor_y() {
    return cursor_y;
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

void limpatela() {
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
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