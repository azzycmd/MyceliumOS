#ifndef IO_H
#define IO_H

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);
unsigned char bcdtodecimal(unsigned char bcd);
unsigned char cmos(unsigned char reg);

#endif