#include "cmd.h"
#include "kernel.h"
#include "lib.h"
#include "io.h"
#include "kbd.h"

#define print_error(msg) setcolor = vermelho; print(msg); cor(coragr);
#define print_info(msg) setcolor = azul; print(msg); cor(coragr);

void cmd_ajuda(char* args) {
    (void)args;
    extern int num_comandos;
    extern cmd lista_comandos[];
    
    print("\n--- Manual do MyceliumOS ---\n");
    for (int i = 0; i < num_comandos; i++) {
        print(lista_comandos[i].name);
        if (i != num_comandos - 1) print(", ");
    }
}

void cmd_beep(char* args) {
    if (args == 0) {
        som(750); // Frequência padrão
    } else {
        int freq = atoi(args);
        if (freq > 20 && freq < 20000) {
            som(freq);
        } else { 
            print_error("\nFrequencia invalida (20-20000)");
        }
    }

    __asm__ volatile("sti");
    sleep(20); 
    parar_som();
}

void cmd_echo(char* args) {
    extern int num_vars;
    extern sys_var lista_vars[];

    if (args == 0 || args[0] == '\0') {
        print("\n");
        return;
    }

    print("\n");

    for (int i = 0; args[i] != '\0'; i++) {
        if (args[i] == '$') {
            char* var_ptr = &args[i + 1];
            int achou = 0;

            if (strdifb(var_ptr, "uptime", 6) == 0) {
                char tempo_str[16];
                itoa(timer_ticks / 100, tempo_str);
                print_info(tempo_str); print("s");
                i += 6;
                achou = 1;
            } 
            else {
                for (int j = 0; j < num_vars; j++) {
                    int len = 0;
                    while(lista_vars[j].nome[len] != '\0') len++;

                    if (strdifb(var_ptr, lista_vars[j].nome, len) == 0) {
                        print_info(lista_vars[j].valor_estatico);
                        i += len;
                        achou = 1;
                        break;
                    }
                }
            }

            if (!achou) print("$");
        } 
        else {
            char s[2] = {0, 0};
            s[0] = args[i];
            print(s); // Bem mais leve
        }
    }
}

void cmd_cpu(char* args) {
    (void)args;
    unsigned int ebx, ecx, edx;
    char vendor[13];
    __asm__ volatile("cpuid" : "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *((int*)(vendor)) = ebx;
    *((int*)(vendor + 4)) = edx;
    *((int*)(vendor + 8)) = ecx;
    vendor[12] = '\0'; 
    print("\nFabricante da CPU: ");
    print(vendor);
}

void cpufetch(char* args) {
    (void)args;
    unsigned int ebx, ecx, edx;
    char vendor[13];
    __asm__ volatile("cpuid" : "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *((int*)(vendor)) = ebx;
    *((int*)(vendor + 4)) = edx;
    *((int*)(vendor + 8)) = ecx;
    vendor[12] = '\0'; 
    print_info(vendor);
}

void cmd_oi(char* args) {
    (void)args;
    print("\nOIIIII :3");
}

void cmd_limpar(char* args) {
    (void)args;
    limpatela();
    cursorpos = 0;
}


void cmd_reboot(char* args) {
    (void)args;
    print("\nReiniciando sistema...");
    
    uint8_t temp;
    do {
        temp = inb(0x64);
    } while (temp & 0x02);
    
    outb(0x64, 0xFE);
}

void cmd_fetch(char* args) {
    (void)args;
    int t = setcolor;
    char q = vermelho;
    char w = q;
    cor(q);
    print("\n     .-'~~~-.           ");
    print_error("MyceliumOS ");
    print_error(versao);
    print("\n   .'");
    cor(w);
    print("o  oOOOo");
    cor(q);
    print(".`         ");
    print_error("---------\n");
    print("  :~~~-.");
    cor(w);
    print("oOo   o");
    cor(q);
    print(".  `     ");
    print_error("CPU: ");
    cpufetch(0);
    print("\n   `. \\ ~-.  ");
    cor(w);
    print("oOOo");
    cor(q);
    print(".      ");
    print_error("Kernel: ");
    print_info("Mycelium-x86\n");
    print("     `.; / ~.  ");
    cor(w);
    print("OO");
    cor(q);
    print(":\n");
    print("      .'  ;-- `.");
    cor(w);
    print("o");
    cor(q);
    print(".'\n");
    print("    ,'  ; ~~--'~\n");
    print("    ;  ;\n");
    cor(q);
    print("_\\\\;_\\\\//_________\n");
    cor(t);
}

typedef struct {
    char* nome;
    char valor;
} argcor;

argcor tabela_cores[] = {
    {"verde", 0x0A},
    {"azul", 0x0B},
    {"vermelho", 0x0C},
    {"rosa", 0x0D},
    {"amarelo", 0x0E},
    {"branco", 0x0F}
};

int num_cores = sizeof(tabela_cores) / sizeof(argcor);

void cmd_color(char* args) {
    if (args == 0) {
        print_error("\nUso: color <nome_da_cor>");
        return;
    }

    mtom(args);

    for (int i = 0; i < num_cores; i++) {
        if (strdif(args, tabela_cores[i].nome) == 0) {
            cor(tabela_cores[i].valor);
            return;
        }
    }

    print_error("\nCor nao reconhecida.");
}

void cmd_uptime(char* args) {
    (void)args;
    char tempo_str[16];
    int segundos = timer_ticks / 100;

    print("\nLigado faz ");
    itoa(segundos, tempo_str);
    print(tempo_str);
    print(" segundos.");
}

cmd lista_comandos[] = {
    {"ajuda", cmd_ajuda},
    {"limpar", cmd_limpar},
    {"reboot", cmd_reboot},
    {"oi", cmd_oi},
    {"beep", cmd_beep},
    {"cpuinfo", cmd_cpu},
    {"fetch", cmd_fetch},
    {"color", cmd_color},
    {"uptime", cmd_uptime},
    {"echo", cmd_echo}
};

int num_comandos = sizeof(lista_comandos) / sizeof(cmd);

void pcmd(char* input) {
    if (input[0] == '\0') return;

    char cmd_part[32];
    char args_temp[256]; // Buffer local para argumentos
    char* args_ptr = 0;
    int i = 0;

    // 1. Extrai o comando
    while (input[i] != ' ' && input[i] != '\0' && i < 31) {
        cmd_part[i] = input[i];
        i++;
    }
    cmd_part[i] = '\0';
    mtom(cmd_part); // Transforma em minúsculo

    // 2. Extrai os argumentos com segurança
    if (input[i] == ' ') {
        i++; // Pula o espaço
        int k = 0;
        while (input[i] != '\0' && k < 255) {
            args_temp[k] = input[i];
            i++;
            k++;
        }
        args_temp[k] = '\0';
        args_ptr = args_temp; // Agora args_ptr aponta para uma string válida
    }

    // 3. Busca na lista
    for (int j = 0; j < num_comandos; j++) {
        if (strdif(cmd_part, lista_comandos[j].name) == 0) {
            // Se o comando não tem argumentos, passamos uma string vazia "" em vez de NULL
            lista_comandos[j].func(args_ptr ? args_ptr : ""); 
            
            // Limpeza pós-comando
            if (strdif(cmd_part, "limpar") != 0) print("\nMyceliumOS> ");
            else print("MyceliumOS> ");
            
            return;
        }
    }

    print_error("\nComando nao encontrado.");
    print("\nMyceliumOS> ");
}

sys_var lista_vars[] = {
    {"ver", versao},
    {"os", "MyceliumOS"},
    {"cursor", cursorstr}
};

int num_vars = sizeof(lista_vars) / sizeof(sys_var);