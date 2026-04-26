#include "kbd.h"
#include <stdint.h>
#include "io.h"
#include "cmd.h"
#include "vga.h"
#include "lib.h"

int ctrl_on = 0;
int shifton = 0;
char buffer[256];
int buffer_index = 0;
char last_command[256] = ""; 
int has_history = 0;

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
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
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

void bufferins(char letra) {
    if (buffer_index < (80 - prompt_x_inicio - 1)) {
        if (buffer_index < 255) {
            int pos = cursor_x - prompt_x_inicio;
            
            // Empurra o buffer para a direita
            for (int i = buffer_index; i >= pos; i--) {
                buffer[i + 1] = buffer[i];
            }
            
            buffer[pos] = letra;
            buffer_index++;

            // Renderiza na VGA sem mover o cursor global precocemente
            uint16_t *video_mem = (uint16_t *)0xB8000;
            for (int i = pos; i < buffer_index; i++) {
                video_mem[cursor_y * 80 + (prompt_x_inicio + i)] = ((uint8_t)setcolor << 8) | buffer[i];
            }

            cursor_x++;
            cursormov(cursor_x, cursor_y);
        }
    }
}

void teclado() {
    uint8_t scancode = inb(0x60);
    outb(0x20, 0x20);
    __asm__ volatile("sti");

    // --- 1. MODIFICADORES ---
    if (scancode == 0x2A || scancode == 0x36) { shifton = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shifton = 0; return; }
    if (scancode == 0x1D) { ctrl_on = 1; return; }
    if (scancode == 0x9D) { ctrl_on = 0; return; }

    // --- 2. ATALHOS DE CTRL (Prioridade) ---
    if (ctrl_on) {
        // CTRL + SETA ESQUERDA (Pular palavra para trás)
        if (scancode == 0x4B) {
            while (cursor_x > prompt_x_inicio && buffer[cursor_x - prompt_x_inicio - 1] == ' ') cursor_x--;
            while (cursor_x > prompt_x_inicio && buffer[cursor_x - prompt_x_inicio - 1] != ' ') cursor_x--;
            cursormov(cursor_x, cursor_y);
            return;
        }
        // CTRL + SETA DIREITA (Pular palavra para frente)
        if (scancode == 0x4D) {
            while (cursor_x < prompt_x_inicio + buffer_index && buffer[cursor_x - prompt_x_inicio] != ' ') cursor_x++;
            while (cursor_x < prompt_x_inicio + buffer_index && buffer[cursor_x - prompt_x_inicio] == ' ') cursor_x++;
            cursormov(cursor_x, cursor_y);
            return;
        }
        // CTRL + DELETE (Apagar para frente)
        if (scancode == 0x0E) {
            while (cursor_x > prompt_x_inicio && buffer[cursor_x - prompt_x_inicio - 1] == ' ') {
                int pos_no_buffer = cursor_x - prompt_x_inicio - 1;
                for (int i = pos_no_buffer; i < buffer_index; i++) buffer[i] = buffer[i + 1];
                buffer_index--;
                cursor_x--;
            }

            while (cursor_x > prompt_x_inicio && buffer[cursor_x - prompt_x_inicio - 1] != ' ') {
                int pos_no_buffer = cursor_x - prompt_x_inicio - 1;
                for (int i = pos_no_buffer; i < buffer_index; i++) buffer[i] = buffer[i + 1];
                buffer_index--;
                cursor_x--;
            }

            uint16_t *video_mem = (uint16_t *)0xB8000;
            for (int i = (cursor_x - prompt_x_inicio); i < 80; i++) {
                if (i < buffer_index)
                    video_mem[cursor_y * 80 + (prompt_x_inicio + i)] = ((uint8_t)setcolor << 8) | buffer[i];
                else
                    video_mem[cursor_y * 80 + (prompt_x_inicio + i)] = ((uint8_t)setcolor << 8) | ' ';
            }

            cursormov(cursor_x, cursor_y);
            return;
        }
        // CTRL + C (Abortar)
        if (scancode == 0x2E) {
            buffer_index = 0;
            buffer[0] = '\0';
            prompt();
            return;
        }
        // CTRL + L (Limpar tela)
        if (scancode == 0x26) {
            limpatela();
            prompt();
            return;
        }
    }

    // Ignora "key release" codes (exceto modificadores tratados acima)
    if (scancode & 0x80) return;

    // --- 3. TECLAS DE NAVEGAÇÃO COMUM ---
    if (scancode == 0x4B) { // Esquerda
        if (cursor_x > prompt_x_inicio) {
            cursor_x--;
            cursormov(cursor_x, cursor_y);
        }
        return;
    } 
    if (scancode == 0x4D) { // Direita
        if (cursor_x < prompt_x_inicio + buffer_index) {
            cursor_x++;
            cursormov(cursor_x, cursor_y);
        }
        return;
    }
    if (scancode == 0x48) { // Seta para Cima (Histórico)
        if (has_history) {
            // Limpa linha atual
            uint16_t *video_mem = (uint16_t *)0xB8000;
            for (int i = prompt_x_inicio; i < 80; i++) video_mem[cursor_y * 80 + i] = ((uint8_t)setcolor << 8) | ' ';
            
            int i = 0;
            while (last_command[i] != '\0') {
                buffer[i] = last_command[i];
                i++;
            }
            buffer[i] = '\0';
            buffer_index = i;
            cursor_x = prompt_x_inicio;
            cursormov(cursor_x, cursor_y);
            print(buffer);
            cursor_x = prompt_x_inicio + buffer_index;
            cursormov(cursor_x, cursor_y);
        }
        return;
    }

    // --- 4. TECLAS DE FUNÇÃO (Enter, Tab, Backspace) ---
    if (scancode == 0x1C) { // ENTER
        buffer[buffer_index] = '\0';
        if (buffer_index > 0) {
            int i = 0;
            while (buffer[i] != '\0') {
                last_command[i] = buffer[i];
                i++;
            }
            last_command[i] = '\0';
            has_history = 1;
            pcmd(buffer); 
        } else {
            prompt();
        }
        buffer_index = 0;
        return;
    }

    if (scancode == 0x0F) { // TAB
        for(int i = 0; i < 4; i++) bufferins(' ');
        return;
    }

    if (scancode == 0x0E) { // BACKSPACE
        if (cursor_x > prompt_x_inicio) {
            int quantidade = 1;
            int pos_atual = cursor_x - prompt_x_inicio;
            
            // Detecta se estamos apagando um TAB (4 espaços)
            if (pos_atual >= 4 && 
                buffer[pos_atual-1] == ' ' && buffer[pos_atual-2] == ' ' &&
                buffer[pos_atual-3] == ' ' && buffer[pos_atual-4] == ' ') {
                quantidade = 4;
            }

            for (int q = 0; q < quantidade; q++) {
                int pos_no_buffer = cursor_x - prompt_x_inicio - 1;
                for (int i = pos_no_buffer; i < buffer_index; i++) buffer[i] = buffer[i + 1];
                buffer_index--;
                cursor_x--;
            }

            // Redesenha linha
            uint16_t *video_mem = (uint16_t *)0xB8000;
            for (int i = (cursor_x - prompt_x_inicio); i < 80; i++) {
                if (i < buffer_index)
                    video_mem[cursor_y * 80 + (prompt_x_inicio + i)] = ((uint8_t)setcolor << 8) | buffer[i];
                else
                    video_mem[cursor_y * 80 + (prompt_x_inicio + i)] = ((uint8_t)setcolor << 8) | ' ';
            }
            cursormov(cursor_x, cursor_y);
        }
        return;
    }

    // --- 5. ENTRADA DE TEXTO ---
    char letra;
    if (shifton) letra = keyboard_map_shift[scancode];
    else letra = keyboard_map[scancode];

    if (letra != 0 && letra != '\n' && !ctrl_on) {
        bufferins(letra);
    }
}