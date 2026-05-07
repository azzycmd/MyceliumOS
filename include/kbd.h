#ifndef KBD_H
#define KBD_H

#include <stdint.h>

extern int shifton;
extern char buffer[256];
extern int buffer_index;

extern unsigned char keyboard_map[128];
extern unsigned char keyboard_map_shift[128];

void teclado();
void teclado_poll();
void teclado_init();
void teclado_init_legacy();
void teclado_set_ps2_enabled(int enabled);
void mouse_handler();
void kbd_ascii_input(char letra);
void kbd_scancode_input(uint8_t scancode);

#endif
