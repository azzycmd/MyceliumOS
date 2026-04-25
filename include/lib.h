#ifndef LIB_H
#define LIB_H
#include <stdint.h>

#include "vga.h"

#define print_error(msg) setcolor = VERMELHO; print(msg); cor(coragr);
#define print_info(msg) setcolor = CIANO; print(msg); cor(coragr);

extern char versao[];
extern char cursorstr[5];
extern char codename[];

struct idt_entry_struct {
    uint16_t base_lo;           
    uint16_t sel;              
    uint8_t  always0;          
    uint8_t  flags;    
    uint16_t base_hi;            
} __attribute__((packed));

struct idt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)); 

int strdif(char* s1, char* s2);
int strdifb(char* s1, char* s2, int n);
void mtom(char* s);
void itoa(int n, char* str);
int atoi(char* str);

#endif