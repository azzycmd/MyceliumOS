#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>

extern char azul;
extern char vermelho;
extern char coragr;
extern char setcolor;
extern int cursorpos;
extern unsigned int timer_ticks;
extern char versao[13];
extern char cursorstr[5];

void print(char* msg);
void som(unsigned int nFrequence);
void parar_som();
void cor(char cor);
void limpatela();
void sleep(uint32_t milissegundos);

#endif