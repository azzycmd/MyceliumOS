#ifndef CMD_H
#define CMD_H
#include <stdint.h>

typedef struct {
    char* nome;
    char* valor_referencia; // Aponta para a variável original
} sys_var;

typedef int (*cmd_func)(int argc, char** argv);
typedef struct {
    char* name;
    char* usage;
    char* help;
    cmd_func func;
} cmd;

extern char buffer[256];
extern int num_comandos;
extern int num_vars;

int cmd_ajuda(int argc, char** argv);
int cmd_beep(int argc, char** argv);
int cmd_echo(int argc, char** argv);
int cmd_cpu(int argc, char** argv);
void cpufetch();
int cmd_oi(int argc, char** argv);
int cmd_reboot(int argc, char** argv);
int cmd_desligar(int argc, char** argv);
int cmd_fetch(int argc, char** argv);
int cmd_color(int argc, char** argv);
int cmd_uptime(int argc, char** argv);
int cmd_bootinfo(int argc, char** argv);
int cmd_ticks(int argc, char** argv);
int cmd_cpuregs(int argc, char** argv);
int cmd_idt(int argc, char** argv);
int cmd_irq(int argc, char** argv);
int cmd_memmap(int argc, char** argv);
int cmd_pci(int argc, char** argv);
int cmd_kbdinfo(int argc, char** argv);
int cmd_bench(int argc, char** argv);
void pcmd(char* input);
int cmd_autocomplete(char* input, int* index);
void cpuid(uint32_t code, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void init_vars();
void attvar(char* nome_var, char* novo_valor);

#endif
