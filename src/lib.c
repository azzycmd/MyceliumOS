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
char versao[] = "v0.2.4";
char codename[] = "Amanita";

struct idt_entry_struct idt[256] = { [0 ... 255] = {0, 0, 0, 0, 0} };
struct idt_ptr_struct idtp = {0, 0};

volatile unsigned int timer_ticks = 0;

uint32_t panic_vector = 0xFFFFFFFF;
uint32_t panic_error_code = 0;
uint32_t panic_eip = 0;
uint32_t panic_cs = 0;
uint32_t panic_eflags = 0;
uint32_t panic_cr0 = 0;
uint32_t panic_cr2 = 0;
uint32_t panic_cr3 = 0;
uint32_t panic_cr4 = 0;
char* panic_origin_file = 0;
int panic_origin_line = 0;

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

#define SET_EXCEPTION_GATE(n) idt_set_gate(n, (uint32_t)(unsigned long)isr##n, 0x08, 0x8E)

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
    SET_EXCEPTION_GATE(0);
    SET_EXCEPTION_GATE(1);
    SET_EXCEPTION_GATE(2);
    SET_EXCEPTION_GATE(3);
    SET_EXCEPTION_GATE(4);
    SET_EXCEPTION_GATE(5);
    SET_EXCEPTION_GATE(6);
    SET_EXCEPTION_GATE(7);
    SET_EXCEPTION_GATE(8);
    SET_EXCEPTION_GATE(9);
    SET_EXCEPTION_GATE(10);
    SET_EXCEPTION_GATE(11);
    SET_EXCEPTION_GATE(12);
    SET_EXCEPTION_GATE(13);
    SET_EXCEPTION_GATE(14);
    SET_EXCEPTION_GATE(15);
    SET_EXCEPTION_GATE(16);
    SET_EXCEPTION_GATE(17);
    SET_EXCEPTION_GATE(18);
    SET_EXCEPTION_GATE(19);
    SET_EXCEPTION_GATE(20);
    SET_EXCEPTION_GATE(21);
    SET_EXCEPTION_GATE(22);
    SET_EXCEPTION_GATE(23);
    SET_EXCEPTION_GATE(24);
    SET_EXCEPTION_GATE(25);
    SET_EXCEPTION_GATE(26);
    SET_EXCEPTION_GATE(27);
    SET_EXCEPTION_GATE(28);
    SET_EXCEPTION_GATE(29);
    SET_EXCEPTION_GATE(30);
    SET_EXCEPTION_GATE(31);
    __asm__ volatile("lidt (%0)" : : "r"(&idtp));
}

void fpu_init() {
    uint32_t cr0;
    uint32_t cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1U << 2);
    cr0 &= ~(1U << 3);
    cr0 |=  (1U << 1);
    cr0 |=  (1U << 5);
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1U << 9);
    cr4 |= (1U << 10);
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    __asm__ volatile("fninit");
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
    outb(0x21, 0xFC); outb(0xA1, 0xFF);
}

void system_reboot() {
    uint8_t temp;

    __asm__ volatile("cli");
    do {
        temp = inb(0x64);
    } while (temp & 0x02);

    outb(0x64, 0xFE);
    while (1) __asm__ volatile("hlt");
}

void system_poweroff() {
    __asm__ volatile("cli");

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    while (1) __asm__ volatile("hlt");
}

static void panic_append(char* destino, int* pos, char* texto, int limite) {
    if (!destino || !pos || !texto || limite <= 0) return;

    for (int i = 0; texto[i] != '\0' && *pos < limite - 1; i++) {
        destino[*pos] = texto[i];
        *pos = *pos + 1;
    }
    destino[*pos] = '\0';
}

static void panic_append_hex(char* destino, int* pos, uint32_t valor, int limite) {
    char* hex = "0123456789ABCDEF";
    panic_append(destino, pos, "0x", limite);

    for (int i = 28; i >= 0; i -= 4) {
        char c[2];
        c[0] = hex[(valor >> i) & 0x0F];
        c[1] = '\0';
        panic_append(destino, pos, c, limite);
    }
}

void kernel_panic_at(char* motivo, char* arquivo, int linha) {
    static char panic_motivo[256];
    int pos = 0;

    __asm__ volatile("cli");
    panic_origin_file = arquivo;
    panic_origin_line = linha;
    panic_vector = 0xFFFFFFFF;
    panic_error_code = 0;
    panic_eip = 0;
    panic_cs = 0;
    panic_eflags = 0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(panic_cr0));
    __asm__ volatile("mov %%cr2, %0" : "=r"(panic_cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(panic_cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r"(panic_cr4));

    panic_append(panic_motivo, &pos,
                 motivo ? motivo : "falha fatal desconhecida", 256);

    vga_panic_screen(panic_motivo);
    while (1) {
        __asm__ volatile("hlt");
    }
}

void kernel_panic(char* motivo) {
    kernel_panic_at(motivo, 0, 0);
}

void exception_handler(uint32_t vetor, uint32_t erro, uint32_t eip,
                       uint32_t cs, uint32_t eflags) {
    static char motivo[192];
    static char* nomes[] = {
        "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
        "Overflow", "Bound Range Exceeded", "Invalid Opcode",
        "Device Not Available", "Double Fault", "Coprocessor Segment Overrun",
        "Invalid TSS", "Segment Not Present", "Stack Segment Fault",
        "General Protection Fault", "Page Fault", "Reserved",
        "x87 Floating Point", "Alignment Check", "Machine Check",
        "SIMD Floating Point", "Virtualization", "Control Protection",
        "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
        "Reserved", "Hypervisor Injection", "VMM Communication",
        "Security Exception", "Reserved"
    };
    char numero[16];
    int pos = 0;

    panic_vector = vetor;
    panic_error_code = erro;
    panic_eip = eip;
    panic_cs = cs;
    panic_eflags = eflags;

    panic_append(motivo, &pos, "Excecao de CPU: ", 192);
    if (vetor < 32) {
        panic_append(motivo, &pos, nomes[vetor], 192);
    } else {
        panic_append(motivo, &pos, "Desconhecida", 192);
    }
    panic_append(motivo, &pos, " (#", 192);
    itoa((int)vetor, numero);
    panic_append(motivo, &pos, numero, 192);
    panic_append(motivo, &pos, ") erro=", 192);
    panic_append_hex(motivo, &pos, erro, 192);

    if (vetor == 14) {
        uint32_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        panic_append(motivo, &pos, " cr2=", 192);
        panic_append_hex(motivo, &pos, cr2, 192);
    }

    kernel_panic_at(motivo, "cpu exception", 0);
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

int strlen(char* str) {
    int i = 0;

    if (!str) return 0;

    while (str[i] != '\0') {
        i++;
    }

    return i;
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
    char out[11];

    out[0] = '0';
    out[1] = 'x';
    for (int i = 28; i >= 0; i -= 4) {
        int nibble = (n >> i) & 0x0F;
        out[2 + ((28 - i) / 4)] = hex_chars[nibble];
    }
    out[10] = '\0';
    print(out);
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

void movback() {
    int cx = get_cursor_x();
    int cy = get_cursor_y();
    if (cx > 0) set_cursor_pos(cx - 1, cy);
    else if (cy > 0) set_cursor_pos(max_cols - 1, cy - 1);
}

void movfront() {
    int cx = get_cursor_x();
    int cy = get_cursor_y();
    if (cx < max_cols - 1) set_cursor_pos(cx + 1, cy);
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
