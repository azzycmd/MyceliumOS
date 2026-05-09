#ifndef LIB_H
#define LIB_H
#include <stdint.h>

#include "vga.h"

typedef enum {
    KSTREAM_STDOUT = 1,
    KSTREAM_STDERR = 2
} kstream_t;

extern kstream_t kstream_active;
extern uint32_t kstream_stdout_writes;
extern uint32_t kstream_stderr_writes;

void kstream_write(kstream_t stream, char* msg);
void kstream_write_color(kstream_t stream, char* msg, enum vga_color color);

#define print_error(msg) kstream_write_color(KSTREAM_STDERR, (msg), VERMELHO)
#define print_info(msg) kstream_write_color(KSTREAM_STDOUT, (msg), CIANO)

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
extern volatile uint32_t irq_timer_count;
extern volatile uint32_t irq_keyboard_count;
extern volatile uint32_t irq_mouse_count;

extern uint32_t panic_vector;
extern uint32_t panic_error_code;
extern uint32_t panic_eip;
extern uint32_t panic_cs;
extern uint32_t panic_eflags;
extern uint32_t panic_cr0;
extern uint32_t panic_cr2;
extern uint32_t panic_cr3;
extern uint32_t panic_cr4;
extern uint32_t panic_code;
extern char* panic_origin_file;
extern int panic_origin_line;

#define PANIC_CODE_UNKNOWN       0x00000001U
#define PANIC_CODE_MANUAL        0x00000002U
#define PANIC_CODE_BOOT_MAGIC    0x00001001U
#define PANIC_CODE_BOOT_MB_PTR   0x00001002U
#define PANIC_CODE_BOOT_MB_SIZE  0x00001003U
#define PANIC_CODE_VIDEO_FB      0x00001004U
#define PANIC_CODE_IDT_SELECTOR  0x00002001U
#define PANIC_CODE_IDT_NULL      0x00002002U
#define PANIC_CODE_IRQ_HANDLER   0x00002003U
#define PANIC_CODE_IDT_NOT_READY 0x00002004U
#define PANIC_CODE_SLEEP_EARLY   0x00002005U
#define PANIC_CODE_IRQSTORM_SIM  0x00002006U
#define PANIC_CODE_CPU_BASE      0x0000E000U

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
int idt_is_ready(void);
uint32_t idt_gate_base(uint8_t num);
uint16_t idt_gate_selector(uint8_t num);
uint8_t idt_gate_flags(uint8_t num);
void exception_handler(uint32_t vetor, uint32_t erro, uint32_t eip,
                       uint32_t cs, uint32_t eflags);
void fpu_init();
void sleep(uint32_t milissegundos);
void timer_handler();
void reprogramar_pic();
void system_reboot();
void system_poweroff();
void kernel_panic_code_at(uint32_t code, char* motivo, char* arquivo, int linha);
void kernel_panic_at(char* motivo, char* arquivo, int linha);
void kernel_panic(char* motivo);

#define panic(motivo) kernel_panic_at((motivo), (char*)__FILE__, __LINE__)
#define panic_code(codigo, motivo) kernel_panic_code_at((codigo), (motivo), (char*)__FILE__, __LINE__)
#define panic_if(condicao, motivo) do { \
    if (condicao) panic(motivo); \
} while (0)
#define panic_if_code(condicao, codigo, motivo) do { \
    if (condicao) panic_code((codigo), (motivo)); \
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
