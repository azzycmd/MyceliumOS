#ifndef LIB_H
#define LIB_H
#include <stdint.h>

#include "vga.h"

#define print_error(msg) do { \
    coragr = setcolor; \
    cor(VERMELHO); \
    print(msg); \
    cor(coragr); \
} while(0)

#define print_info(msg) do { \
    coragr = setcolor; \
    cor(CIANO); \
    print(msg); \
    cor(coragr); \
} while(0)

extern char versao[];
extern char codename[];
extern char smod[2];

extern unsigned char segundo;
extern unsigned char minuto;
extern unsigned char hora;
extern unsigned char dia;
extern unsigned char mes;
extern unsigned char ano;
extern char buffer_tempo[32];

void updatertc();
void horat();
void datat();

extern unsigned int timer_ticks;

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

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_install();
void sleep(uint32_t milissegundos);
void timer_handler();
void reprogramar_pic();
int strdif(char* s1, char* s2);
int strdifb(char* s1, char* s2, int n);
void mtom(char* s);
void itoa(int n, char* str);
int atoi(char* str);
void prompt();
void print_hex(uint32_t n);
int htoi(char* str);
void print_hex_byte(uint8_t byte);
void strcpy(char* dest, char* src);
int strlen(char* s);
void removchar(int pos);
void movfront();
void movback();

#endif