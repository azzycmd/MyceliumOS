#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>

extern unsigned int timer_ticks;

void som(unsigned int nFrequence);
void parar_som();
void sleep(uint32_t milissegundos);

#endif