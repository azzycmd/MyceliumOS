#include "io.h"
#include "kbd.h"
#include <stdint.h>
#include "lib.h"
#include "cmd.h"
#include "vga.h"

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

void cpit(unsigned int frequencia) {
    uint32_t divisor = 1193180 / frequencia;

    outb(0x43, 0x36);
    // Envia o byte baixo
    outb(0x40, (uint8_t)(divisor & 0xFF));
    // Envia o byte alto
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler() {
    timer_ticks++;
    outb(0x20, 0x20); // EOI para o Timer
}

void sleep(uint32_t milissegundos) {
    uint32_t tempo_final = timer_ticks + milissegundos;
    while (timer_ticks < tempo_final) {
        __asm__ volatile("hlt");
    }
}

void som(unsigned int nFrequence) {
    unsigned int Div;
    unsigned char t;
    Div = 1193180 / nFrequence;
    outb(0x43, 0xB6);
    outb(0x42, (unsigned char) (Div) );
    outb(0x42, (unsigned char) (Div >> 8));
    t = inb(0x61);
    if (t != (t | 3)) {
        outb(0x61, t | 3);
    }
}

void parar_som() {
    unsigned char t = inb(0x61) & 0xFC;
    outb(0x61, t);
}

void cursor_on() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void reprogramar_pic() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x0);  outb(0xA1, 0x0);
}

int main() {
    extern void timer_wrapper();
    extern void keyboard_wrapper();
    idt_install();     
    reprogramar_pic();
    idt_set_gate(32, (uint32_t)(unsigned long)timer_wrapper, 0x08, 0x8E); // 0x08 é o CS
    idt_set_gate(33, (uint32_t)(unsigned long)keyboard_wrapper, 0x08, 0x8E);
    cpit(100);
    __asm__ volatile("sti");
    som(293 * 2);
    sleep(10);
    som(440 * 2);
    sleep(10);
    som(349 * 2);
    sleep(10);
    som(261 * 2);
    sleep(20);
    parar_som();
    cursor_on();
    
    limpatela();
    cursorpos = 0;
    cor(BRANCO);
    print("MyceliumOS Terminal ");
    print(versao);
    cmd_fetch(0);
    print("\nMyceliumOS> ");
    buffer_index = 0;
    buffer[0] = '\0';
    while(1) {
        itoa(cursorpos, cursorstr);
        __asm__ volatile("hlt");
    }
}