#include "lib.h"
#include "io.h"
#include "kbd.h"
#include "vga.h"

char smod[2] = "0";

unsigned char segundo = 0;
unsigned char minuto  = 0;
unsigned char hora    = 0;
unsigned char dia     = 0;
unsigned char mes     = 0;
unsigned char ano     = 0;
char buffer_tempo[32];

char cursorstr[5] = "";
char versao[] = "v0.2.3";
char codename[] = "Amanita";

struct idt_entry_struct idt[256] = { [0 ... 255] = {0, 0, 0, 0, 0} };
struct idt_ptr_struct idtp = {0, 0};

unsigned int timer_ticks = 0;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

void idt_install() {
    idtp.limit = (uint16_t)(sizeof(struct idt_entry_struct) * 256) - 1;
    idtp.base = (uint32_t)(unsigned long)&idt; 
    for(int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);
    __asm__ volatile("lidt (%0)" : : "r"(&idtp));
}

void sleep(uint32_t milissegundos) {
    uint32_t tempo_final = timer_ticks + milissegundos;
    while (timer_ticks < tempo_final) {
        __asm__ volatile("hlt");
    }
}

void timer_handler() {
    timer_ticks++;
    outb(0x20, 0x20); // EOI para o Timer
}

void reprogramar_pic() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x0);  outb(0xA1, 0x0);
}

int strdif(char* s1, char* s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return s1[i] - s2[i];
}

int strdifb(char* s1, char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void mtom(char* s) {
    if (!s) return;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = s[i] + 32; // Na tabela ASCII, 'a' é 'A' + 32
        }
    }
}

void itoa(int n, char* str) {
    int i = 0;
    int temp = n;
    
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (temp > 0) {
        str[i++] = (temp % 10) + '0';
        temp /= 10;
    }
    str[i] = '\0';

    for (int j = 0; j < i / 2; j++) {
        char aux = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = aux;
    }
}

int atoi(char* str) {
    int res = 0;
    int i = 0;

    while (str[i] == ' ') i++;

    for (; str[i] != '\0' && str[i] >= '0' && str[i] <= '9'; ++i) {
        res = res * 10 + str[i] - '0';
    }
    return res;
}

void updatertc() {
    segundo = bcdtodecimal(cmos(0x00));
    minuto  = bcdtodecimal(cmos(0x02));
    hora    = bcdtodecimal(cmos(0x04));
    dia     = bcdtodecimal(cmos(0x07));
    mes     = bcdtodecimal(cmos(0x08));
    ano     = bcdtodecimal(cmos(0x09));
}

void horat() {
    int hora_local = (int)hora - 3;
    if (hora_local < 0) hora_local += 24;
    
    itoa(hora_local, buffer_tempo); print(buffer_tempo); print(":");
    itoa(minuto, buffer_tempo);
    if (minuto < 10) {print("0");}
    print(buffer_tempo); 
    print(":");
    itoa(segundo, buffer_tempo);
    if (segundo < 10) {print("0");}
    print(buffer_tempo);
}

void datat() {
    itoa(dia, buffer_tempo); print(buffer_tempo); print("/");
    itoa(mes, buffer_tempo); print(buffer_tempo); print("/20");
    itoa(ano, buffer_tempo); print(buffer_tempo);
}

void prompt() {
    char s = setcolor;
    cor(CIANO);
    print("\n[");
    updatertc();
    horat();
    print("] ");
    
    if (smod[0] == '1') {
        cor(VERDE);
        print("MyceliumOS\\ ");
    } else {
        cor(s);
        print("MyceliumOS> ");

    }
}

void print_hex(uint32_t n) {
    char* hex_chars = "0123456789ABCDEF";
    
    print("0x");

    // Imprime os 8 nibbles (4 bits cada) um por um, do mais significativo para o menos
    for (int i = 28; i >= 0; i -= 4) {
        int nibble = (n >> i) & 0x0F;
        char c[2];
        c[0] = hex_chars[nibble];
        c[1] = '\0';
        print(c);
    }
}

void print_hex_byte(uint8_t byte) {
    char* hex_chars = "0123456789ABCDEF";
    char c[3];
    c[0] = hex_chars[(byte >> 4) & 0x0F];
    c[1] = hex_chars[byte & 0x0F];
    c[2] = '\0';
    print(c);
}

int htoi(char* str) {
    unsigned int res = 0;
    int i = 0;
    while (str[i] == ' ') i++;

    if (str[i] == '0' && (str[i+1] == 'x' || str[i+1] == 'X')) i += 2;

    while (str[i] != '\0') {
        int v = 0;
        if (str[i] >= '0' && str[i] <= '9') v = str[i] - '0';
        else if (str[i] >= 'a' && str[i] <= 'f') v = str[i] - 'a' + 10;
        else if (str[i] >= 'A' && str[i] <= 'F') v = str[i] - 'A' + 10;
        else break;
        res = (res << 4) | (v & 0xF);
        i++;
    }
    return res;
        }

void strcpy(char* dest, char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int strlen(char* s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
}

void movback() {
    int cx = get_cursor_x();
    int cy = get_cursor_y();
    if (cx > 0) set_cursor_pos(cx - 1, cy);
    else if (cy > 0) set_cursor_pos(79, cy - 1);
}

void movfront() {
    int cx = get_cursor_x();
    int cy = get_cursor_y();
    if (cx < 79) set_cursor_pos(cx + 1, cy);
    else set_cursor_pos(0, cy + 1);
    }

void removchar(int pos) {
    if (pos < 0 || pos >= buffer_index) return;

    for (int i = pos; i < buffer_index - 1; i++) {
        buffer[i] = buffer[i + 1];
    }
    buffer_index--;
    buffer[buffer_index] = '\0';
}