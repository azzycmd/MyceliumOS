#ifndef IO_H
#define IO_H

void outb(unsigned short port, unsigned char val);
void outw(unsigned short port, unsigned short val);
void outl(unsigned short port, unsigned int val);
unsigned char inb(unsigned short port);
unsigned int inl(unsigned short port);
unsigned char bcdtodecimal(unsigned char bcd);
unsigned char cmos(unsigned char reg);

#endif
