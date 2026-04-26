#include "lib.h"
#include "io.h"
#include "vga.h"

char smod[2] = "0";

unsigned char segundo = 0;
unsigned char minuto  = 0;
unsigned char hora    = 0;
unsigned char dia     = 0;
unsigned char mes     = 0;
unsigned char ano     = 0;
char buffer_tempo[32];

int prompt_y = 0; 
int prompt_x_inicio = 23;

char cursorstr[5] = "";
char versao[] = "v0.2.2";
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

    prompt_y = cursor_y;
}

void print_hex(uint32_t n) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buffer[9 - i] = hex_chars[n & 0xF];
        n >>= 4;
    }
    buffer[10] = '\0';
    print(buffer);
}

int htoi(char* str) {
    if (!str) return 0;
    
    unsigned int res = 0;
    int i = 0;

    // Pula espaços iniciais se houver
    while (str[i] == ' ') i++;

    // Pula o "0x" se o usuário digitou
    if (str[i] == '0' && (str[i+1] == 'x' || str[i+1] == 'X')) {
        i += 2;
    }

    while (str[i] != '\0') {
        char c = str[i];
        int valor = 0;

        if (c >= '0' && c <= '9') {
            valor = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            valor = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            valor = c - 'A' + 10;
        } else {
            // Se encontrar qualquer coisa que não seja HEX (espaço, letra errada), para
            break;
        }

        // Verifica se o próximo passo vai estourar o limite de 32 bits
        // (Prevenção de overflow)
        res = (res << 4) | (valor & 0xF);
        i++;
    }
    return (int)res;
}

int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}