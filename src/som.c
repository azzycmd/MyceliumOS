#include "som.h"
#include "io.h"
#include <stdint.h>

void cpit(unsigned int frequencia) {
    uint32_t divisor = 1193180 / frequencia;

    outb(0x43, 0x36);
    // Envia o byte baixo
    outb(0x40, (uint8_t)(divisor & 0xFF));
    // Envia o byte alto
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
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