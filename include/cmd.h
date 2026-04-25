#ifndef CMD_H
#define CMD_H
#include <stdint.h>

typedef struct {
    char* nome;
    char* valor_estatico;
} sys_var;

typedef void (*command_func)(char*);

typedef struct {
    char* name;
    command_func func;
} cmd;

extern char buffer[256];
extern int num_comandos;
extern int num_vars;

void cmd_ajuda(char* args);
void cmd_beep(char* args);
void cmd_echo(char* args);
void cmd_cpu(char* args);
void cpufetch(char* args);
void cmd_oi(char* args);
void cmd_limpar(char* args);
void cmd_reboot(char* args);
void cmd_fetch(char* args);
void cmd_color(char* args);
void cmd_uptime(char* args);
void pcmd(char* input);
void cpuid(uint32_t code, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

#endif