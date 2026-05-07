#include "kbd.h"
#include <stdint.h>
#include "io.h"
#include "cmd.h"
#include "kernel.h"
#include "som.h"
#include "vga.h"
#include "lib.h"

int shifton = 0;
char buffer[256] = {0};
int buffer_index = 0;
int ctrlon = 0;
int alton = 0;

static int cursor_buffer = 0;
static char historico[256] = {0};
static int tem_historico = 0;

static volatile uint8_t fila_scancodes[64];
static volatile int fila_inicio = 0;
static volatile int fila_fim    = 0;
static int ps2_keyboard_enabled = 1;

#define KBD_STATUS 0x64
#define KBD_DATA   0x60
#define KBD_OUTPUT_FULL 0x01
#define KBD_INPUT_FULL  0x02
#define KBD_AUX_DATA    0x20
#define KBD_WAIT        100000

static void print_spaces(int count) {
    char spaces[256];
    if (count <= 0) return;
    if (count > 255) count = 255;

    for (int i = 0; i < count; i++) {
        spaces[i] = ' ';
    }
    spaces[count] = '\0';
    print(spaces);
}

static void redesenhar_linha_a_partir_cursor(int old_buffer_index) {
    int xa = get_cursor_x();
    int ya = get_cursor_y();

    print(&buffer[cursor_buffer]);
    print_spaces(old_buffer_index - buffer_index);
    set_cursor_pos(xa, ya);
}

unsigned char keyboard_map[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b',
    '\t',
    'q', 'w', 'e', 'r',
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`',  0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/',  0,
    '*', 0, ' ', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+',
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
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
    '"', '~',  0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M', '<', '>', '?',  0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void kbd_wait_write() {
    for (int i = 0; i < KBD_WAIT; i++)
        if (!(inb(KBD_STATUS) & KBD_INPUT_FULL)) return;
}

static int kbd_send_cmd(uint8_t cmd) {
    for (int t = 0; t < 3; t++) {
        kbd_wait_write();
        outb(KBD_DATA, cmd);
        for (int i = 0; i < KBD_WAIT; i++) {
            if (inb(KBD_STATUS) & KBD_OUTPUT_FULL) {
                uint8_t r = inb(KBD_DATA);
                if (r == 0xFA) return 1;
                if (r == 0xFE) break;
            }
        }
    }
    return 0;
}

void teclado_init() {
    ps2_keyboard_enabled = 1;

    uint8_t config = 0;
    kbd_wait_write(); outb(KBD_STATUS, 0x20);
    for (int i = 0; i < KBD_WAIT; i++) {
        if (inb(KBD_STATUS) & KBD_OUTPUT_FULL) { config = inb(KBD_DATA); break; }
    }

    /* Preserva USB legacy: nao resetamos o 8042 nem o teclado. */
    config |=  0x01;
    config |=  0x40;
    config &= ~0x02;

    kbd_wait_write(); outb(KBD_STATUS, 0x60);
    kbd_wait_write(); outb(KBD_DATA, config);

    kbd_wait_write(); outb(KBD_STATUS, 0xAE);
    (void)kbd_send_cmd(0xF4);

    shifton = 0; ctrlon = 0; alton = 0;
    fila_inicio = 0; fila_fim = 0;
}

void teclado_init_legacy() {
    ps2_keyboard_enabled = 1;
    shifton = 0;
    ctrlon = 0;
    alton = 0;
    fila_inicio = 0;
    fila_fim = 0;
}

void teclado_set_ps2_enabled(int enabled) {
    ps2_keyboard_enabled = enabled ? 1 : 0;
    if (!ps2_keyboard_enabled) {
        fila_inicio = 0;
        fila_fim = 0;
    }
}

static void enfileirar(uint8_t sc) {
    int prox = (fila_fim + 1) % 64;
    if (prox == fila_inicio) return;
    fila_scancodes[fila_fim] = sc;
    fila_fim = prox;
}

static int desenfileirar(uint8_t *sc) {
    if (fila_inicio == fila_fim) return 0;
    *sc = fila_scancodes[fila_inicio];
    fila_inicio = (fila_inicio + 1) % 64;
    return 1;
}

static void teclado_drenar_controlador(void) {
    for (int i = 0; i < 16; i++) {
        uint8_t status = inb(KBD_STATUS);
        if (!(status & KBD_OUTPUT_FULL)) break;
        uint8_t sc = inb(KBD_DATA);
        if (!(status & KBD_AUX_DATA)) enfileirar(sc);
    }
}

static void processar_set1(uint8_t scancode) {
    static int estendido = 0;

    if (scancode == 0xFA || scancode == 0xFE) return;

    if (scancode == 0x38) { alton = 1; return; }
    if (scancode == 0xB8) { alton = 0; return; }
    if (scancode == 0xE0) { estendido = 1; return; }
    if (scancode == 0x1D) { ctrlon = 1; return; }
    if (scancode == 0x9D) { ctrlon = 0; return; }
    if (scancode == 0x2A || scancode == 0x36) { shifton = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shifton = 0; return; }

    if (ctrlon && shifton && alton && scancode == 0x10) {
        uint8_t good = 0x02;
        while (good & 0x02) good = inb(0x64);
        outb(0x64, 0xFE);
        __asm__ volatile("hlt");
    }

    if (estendido) {
        estendido = 0;
        if (scancode == 0x4B && ctrlon) {
            while (cursor_buffer > 0 && buffer[cursor_buffer-1] == ' ') { cursor_buffer--; movback(); }
            while (cursor_buffer > 0 && buffer[cursor_buffer-1] != ' ') { cursor_buffer--; movback(); }
            return;
        }
        if (scancode == 0x4D && ctrlon) {
            while (cursor_buffer < buffer_index && buffer[cursor_buffer] == ' ')  { cursor_buffer++; movfront(); }
            while (cursor_buffer < buffer_index && buffer[cursor_buffer] != ' ')  { cursor_buffer++; movfront(); }
            return;
        }
        if (scancode == 0x4D) {
            if (cursor_buffer < buffer_index) {
                cursor_buffer++;
                int cx = get_cursor_x(), cy = get_cursor_y();
                if (cx < max_cols - 1) set_cursor_pos(cx+1, cy); else set_cursor_pos(0, cy+1);
            }
            return;
        }
        if (scancode == 0x4B) {
            if (cursor_buffer > 0) {
                cursor_buffer--;
                int cx = get_cursor_x(), cy = get_cursor_y();
                if (cx > 0) set_cursor_pos(cx-1, cy); else if (cy > 0) set_cursor_pos(max_cols - 1, cy-1);
            }
            return;
        }
        if (scancode == 0x48) {
            if (tem_historico) {
                while (cursor_buffer > 0) { print("\b"); cursor_buffer--; }
                for (int i = 0; i < buffer_index; i++) print(" ");
                for (int i = 0; i < buffer_index; i++) print("\b");
                strcpy(buffer, historico);
                buffer_index = strlen(buffer);
                cursor_buffer = buffer_index;
                print(buffer);
            }
            return;
        }
        return;
    }

    if (scancode & 0x80) return;

    if (scancode == 0x1C) {
        buffer[buffer_index] = '\0';
        if (buffer_index > 0) {
            strcpy(historico, buffer);
            tem_historico = 1;
            pcmd(buffer);
        } else {
            prompt();
        }
        buffer_index = 0; cursor_buffer = 0;
        return;
    }

    if (ctrlon && keyboard_map[scancode] == 'l') {
        limpatela();
        char s = setcolor;
        cor(CIANO); print("["); updatertc(); horat(); print("]"); cor(s); print(" MyceliumOS> ");
        buffer_index = 0; cursor_buffer = 0;
        return;
    }
    if (ctrlon && keyboard_map[scancode] == 'c') {
        parar_som(); prompt();
        return;
    }

    if (scancode == 0x0E) {
        int old_buffer_index = buffer_index;
        if (ctrlon) {
            while (cursor_buffer > 0 && buffer[cursor_buffer-1] == ' ') { removchar(cursor_buffer-1); cursor_buffer--; movback(); }
            while (cursor_buffer > 0 && buffer[cursor_buffer-1] != ' ') { removchar(cursor_buffer-1); cursor_buffer--; movback(); }
        } else {
            if (cursor_buffer > 0) { removchar(cursor_buffer-1); cursor_buffer--; movback(); }
        }
        redesenhar_linha_a_partir_cursor(old_buffer_index);
        return;
    }

    char letra = shifton ? keyboard_map_shift[scancode] : keyboard_map[scancode];
    if (letra != 0 && letra != '\b' && letra != '\n') {
        if (buffer_index < 254) {
            for (int i = buffer_index; i > cursor_buffer; i--) buffer[i] = buffer[i-1];
            buffer[cursor_buffer] = letra;
            buffer_index++;
            buffer[buffer_index] = '\0';
            int xf = get_cursor_x(), yf = get_cursor_y();
            print(&buffer[cursor_buffer]);
            xf++;
            if (xf >= max_cols) { xf = 0; yf++; }
            cursor_buffer++;
            set_cursor_pos(xf, yf);
        }
    }
}

void kbd_ascii_input(char letra) {
    if (letra == '\n') {
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

    if (letra == '\b') {
        int old_buffer_index = buffer_index;
        if (cursor_buffer > 0) {
            removchar(cursor_buffer - 1);
            cursor_buffer--;
            movback();
        }
        redesenhar_linha_a_partir_cursor(old_buffer_index);
        return;
    }

    if (letra == '\t') {
        for (int j = 0; j < 4; j++) {
            kbd_ascii_input(' ');
        }
        return;
    }

    if (letra == 0) {
        return;
    }

    if (buffer_index < 254) {
        for (int i = buffer_index; i > cursor_buffer; i--) {
            buffer[i] = buffer[i - 1];
        }
        buffer[cursor_buffer] = letra;
        buffer_index++;
        buffer[buffer_index] = '\0';

        int xf = get_cursor_x(), yf = get_cursor_y();
        print(&buffer[cursor_buffer]);
        xf++;
        if (xf >= max_cols) {
            xf = 0;
            yf++;
        }
        cursor_buffer++;
        set_cursor_pos(xf, yf);
    }
}

void kbd_scancode_input(uint8_t scancode) {
    processar_set1(scancode);
}

/* ISR — só lê e enfileira */
void teclado() {
    if (ps2_keyboard_enabled) {
        teclado_drenar_controlador();
    } else {
        for (int i = 0; i < 16; i++) {
            if (!(inb(KBD_STATUS) & KBD_OUTPUT_FULL)) break;
            (void)inb(KBD_DATA);
        }
    }
    outb(0x20, 0x20);
}

/* Loop principal — processa a fila */
void teclado_poll() {
    uint8_t sc;
    if (!ps2_keyboard_enabled) return;
    teclado_drenar_controlador();
    while (desenfileirar(&sc))
        processar_set1(sc);
}

void mouse_handler() {
    for (int i = 0; i < 8; i++) {
        if (!(inb(KBD_STATUS) & KBD_OUTPUT_FULL)) break;
        (void)inb(KBD_DATA);
    }
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
