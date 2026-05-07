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

extern volatile unsigned int timer_ticks;

extern uint32_t panic_vector;
extern uint32_t panic_error_code;
extern uint32_t panic_eip;
extern uint32_t panic_cs;
extern uint32_t panic_eflags;
extern uint32_t panic_cr0;
extern uint32_t panic_cr2;
extern uint32_t panic_cr3;
extern uint32_t panic_cr4;
extern char* panic_origin_file;
extern int panic_origin_line;

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
void exception_handler(uint32_t vetor, uint32_t erro, uint32_t eip,
                       uint32_t cs, uint32_t eflags);
void fpu_init();
void sleep(uint32_t milissegundos);
void timer_handler();
void reprogramar_pic();
void system_reboot();
void system_poweroff();
void kernel_panic_at(char* motivo, char* arquivo, int linha);
void kernel_panic(char* motivo);

#define panic(motivo) kernel_panic_at((motivo), (char*)__FILE__, __LINE__)
#define panic_if(condicao, motivo) do { \
    if (condicao) panic(motivo); \
} while (0)

int strdif(char* s1, char* s2);
int strdifb(char* s1, char* s2, int n);
void mtom(char* s);
void itoa(int n, char* str);
int atoi(char* str);
void prompt();
void print_hex(uint32_t n);
void print_hex_byte(uint8_t byte);
int htoi(char* str);
int strlen(char* str);
void removchar(int pos);
void movfront();
void movback();
void strcpy(char* dest, char* src);

#endif
