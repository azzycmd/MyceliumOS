#include "io.h"

unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outb(unsigned short port, unsigned char data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

unsigned char bcdtodecimal(unsigned char bcd) {
    return ((bcd / 16) * 10) + (bcd % 16);
}

unsigned char cmos(unsigned char reg) {
    outb(0x70, reg);
    return inb(0x71);
}