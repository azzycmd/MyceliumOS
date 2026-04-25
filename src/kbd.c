#include "kbd.h"
#include <stdint.h>
#include "io.h"
#include "cmd.h"
#include "vga.h"

int shifton = 0;
char buffer[256];
int buffer_index = 0;

unsigned char keyboard_map[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b',   
    '\t',           
    'q', 'w', 'e', 'r', 
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0,          
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   
    '\'', '`',   0,     
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',         
    'm', ',', '.', '/',   0,                
    '*',
    0,  
    ' ',    
    0,  
    0,  
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  
    0,  
    0,  
    0,  
    0,  
    0,  
    '-',
    0,  
    0,
    0,  
    '+',
    0,  
    0,  
    0,  
    0,  
    0,  
    0,   0,   0,
    0,  
    0,  
    0,  
};

unsigned char keyboard_map_shift[128] =
{
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b',   
    '\t',           
    'Q', 'W', 'E', 'R', 
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   
    0,          
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   
    '\"', '~',   0,     
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',         
    'M', '<', '>', '?',   0,                
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void teclado() {
    uint8_t scancode = inb(0x60);
    outb(0x20, 0x20); // EOI imediato

    // Lógica de Shift (Scancodes: 0x2A = Left Shift, 0x36 = Right Shift)
    if (scancode == 0x2A || scancode == 0x36) {
        shifton = 1;
        return;
    }
    // Quando solta o Shift (Scancode + 0x80)
    if (scancode == 0xAA || scancode == 0xB6) {
        shifton = 0;
        return;
    }

    // Ignora outras teclas soltas
    if (scancode & 0x80) return;

    if (scancode == 0x1C) { 
    buffer[buffer_index] = '\0';
    
    if (buffer_index > 0) {
        pcmd(buffer); 
    } else {
        print("\nMyceliumOS> ");
    }
    
    buffer_index = 0;
    return;
    }
    if (scancode == 0x0E) { // Backspace
        if (buffer_index > 0) {
            buffer_index--;
            char char_apagado = buffer[buffer_index];
            buffer[buffer_index] = '\0';

            if (char_apagado == '\t') {
                print("\b\b\b\b");
            } else {
                print("\b");
            }
        }
        return;
    }
    
    char letra;

    if (shifton) {
        letra = keyboard_map_shift[scancode];
    } else {
        letra = keyboard_map[scancode];
    }

    if (letra != 0) {
        char s[2] = {letra, '\0'};
        print(s);
        buffer[buffer_index++] = letra;
    }
}