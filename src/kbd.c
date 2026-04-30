#include "kbd.h"
#include <stdint.h>
#include "io.h"
#include "cmd.h"
#include "kernel.h"
#include "vga.h"
#include "lib.h"

int shifton = 0;
char buffer[256] = {0};
int buffer_index = 0;
int ctrlon;
int alton = 0;

static int cursor_buffer = 0;
static char historico[256] = {0};
static int tem_historico = 0;

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
    __asm__ volatile("sti");
    static int estendido = 0;

    if (scancode == 0x38) { alton = 1; return; }
    if (scancode == 0xB8) { alton = 0; return; }

    if (scancode == 0xE0) {
        estendido = 1;
        outb(0x20, 0x20);
        return;
    }

    // Scancode 0x1D é o Left Control
    if (scancode == 0x1D) {
        ctrlon = 1;
        return;
    }
    // 0x9D é o Control sendo solto (0x1D + 0x80)
    if (scancode == 0x9D) {
        ctrlon = 0;
        return;
    }

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

    if (ctrlon && shifton && alton && scancode == 0x10) {
        uint8_t good = 0x02;
        while (good & 0x02)
            good = inb(0x64);
        outb(0x64, 0xFE);
        sleep(1000);
        __asm__ volatile("hlt");
    }

    if (estendido) {
        estendido = 0;

        // --- CTRL + SETA ESQUERDA (Pular palavra para trás) ---
        if (scancode == 0x4B && ctrlon) {
            while (cursor_buffer > 0 && buffer[cursor_buffer - 1] == ' ') {
                cursor_buffer--;
                movback();
            }
            while (cursor_buffer > 0 && buffer[cursor_buffer - 1] != ' ') {
                cursor_buffer--;
                movback();
            }
            return;
        }

        if (scancode == 0x4D && ctrlon) {
            while (cursor_buffer < buffer_index && buffer[cursor_buffer] == ' ') {
                cursor_buffer++;
                movfront();
            }
            while (cursor_buffer < buffer_index && buffer[cursor_buffer] != ' ') {
                cursor_buffer++;
                movfront();
            }
            return;
        }
            
        // SETA DIREITA
        if (scancode == 0x4D) { 
            if (cursor_buffer < buffer_index) {
                cursor_buffer++;
                int cx = get_cursor_x();
                int cy = get_cursor_y();
                
                if (cx < 79) set_cursor_pos(cx + 1, cy);
                else set_cursor_pos(0, cy + 1);
            }
            return;
        }

        // SETA ESQUERDA
        if (scancode == 0x4B) { 
            if (cursor_buffer > 0) {
                cursor_buffer--;
                int cx = get_cursor_x();
                int cy = get_cursor_y();                    
                if (cx > 0) set_cursor_pos(cx - 1, cy);
                else if (cy > 0) set_cursor_pos(79, cy - 1);
                cursor();
            }
            return;
        }

        if (scancode == 0x48) { // SETA PARA CIMA
            if (tem_historico) {
                while (cursor_buffer > 0) {
                    print("\b");
                    cursor_buffer--;
                }
                for (int i = 0; i < buffer_index; i++) print(" ");
                for (int i = 0; i < buffer_index; i++) print("\b");

                strcpy(buffer, historico);
                buffer_index = strlen(buffer);
                cursor_buffer = buffer_index;

                print(buffer);
            }
            return;
        }
    }

    if (scancode & 0x80) return;

    if (scancode == 0x1C) { // ENTER
        buffer[buffer_index] = '\0';
        
        if (buffer_index > 0) {
            strcpy(historico, buffer); 
            tem_historico = 1;
            pcmd(buffer);
        } else {
            prompt();
        }
        
        buffer_index = 0;
        cursor_buffer = 0;
        return;
    }

    if (ctrlon && keyboard_map[scancode] == 'l') {
        limpatela();
        char s = setcolor;
        cor(CIANO); print("["); updatertc(); horat(); print("]"); cor(s); print(" MyceliumOS> ");
        buffer_index = 0;
        cursor_buffer = 0;
        return;
    }

    if (ctrlon && keyboard_map[scancode] == 'c') {
        parar_som();
        prompt();
        return;
    }

// --- BACKSPACE (Normal ou Ctrl) ---
    if (scancode == 0x0E) {
        if (ctrlon) {
            // CTRL + BACKSPACE: Apaga a palavra à ESQUERDA
            while (cursor_buffer > 0 && buffer[cursor_buffer - 1] == ' ') {
                removchar(cursor_buffer - 1);
                cursor_buffer--;
                movback();
            }
            while (cursor_buffer > 0 && buffer[cursor_buffer - 1] != ' ') {
                removchar(cursor_buffer - 1);
                cursor_buffer--;
                movback();
            }
        } else {
            // BACKSPACE NORMAL
            if (cursor_buffer > 0) {
                removchar(cursor_buffer - 1);
                cursor_buffer--;
                movback();
            }
        }

        int x_alvo = get_cursor_x();
        int y_alvo = get_cursor_y();
        print(&buffer[cursor_buffer]);
        print("           ");
        set_cursor_pos(x_alvo, y_alvo);
        
        return;
    }
    
    char letra;

    if (shifton) {
        letra = keyboard_map_shift[scancode];
    } else {
        letra = keyboard_map[scancode];
    }

    if (letra != 0 && letra != '\b' && letra != '\n') {
        if (buffer_index < 254) {
            for (int i = buffer_index; i > cursor_buffer; i--) {
                buffer[i] = buffer[i - 1];
            }

            buffer[cursor_buffer] = letra;
            buffer_index++;
            buffer[buffer_index] = '\0';

            int x_focar = get_cursor_x();
            int y_focar = get_cursor_y();

            print(&buffer[cursor_buffer]);

            x_focar++;
            if (x_focar >= 80) { x_focar = 0; y_focar++; }

            cursor_buffer++;
            set_cursor_pos(x_focar, y_focar);
        }
    }
}