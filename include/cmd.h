#ifndef CMD_H
#define CMD_H
#include <stdint.h>

typedef struct {
    char* nome;
    char* valor_referencia; // Aponta para a variável original
} sys_var;

typedef void (*command_func)(int argc, char** argv);

typedef struct {
    char* name;
    command_func func;
} cmd;

extern char buffer[256];
extern int num_comandos;
extern int num_vars;

void cmd_ajuda(int argc, char** argv);
void cmd_beep(int argc, char** argv);
void cmd_echo(int argc, char** argv);
void cmd_cpu(int argc, char** argv);
void cpufetch(int argc, char** argv);
void cmd_oi(int argc, char** argv);
void cmd_limpar(int argc, char** argv);
void cmd_reboot(int argc, char** argv);
void cmd_fetch(int argc, char** argv);
void cmd_color(int argc, char** argv);
void cmd_uptime(int argc, char** argv);
void pcmd(char* input);
void cpuid(uint32_t code, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void regvar(char* nome, char* valor_inicial);
void init_vars();
void attvar(char* nome_var, char* novo_valor);
void cmd_hex(int argc, char** argv);

#endif