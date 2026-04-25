#ifndef KBD_H
#define KBD_H

extern int shifton;
extern char buffer[256];
extern int buffer_index;

extern unsigned char keyboard_map[128];
extern unsigned char keyboard_map_shift[128];

void teclado();

#endif