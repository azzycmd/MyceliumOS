#include "cmd.h"
#include "kernel.h"
#include "lib.h"
#include "io.h"
#include "vga.h"

extern char smod[];

void attvar(char* nome_var, char* novo_valor);

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

void cmd_time(char* args) {
    (void)args;
    char s = setcolor;
    cor(CIANO);
    updatertc();
    datat();
    print(" - ");
    horat();
    print("\n");
    cor(s);
}

void cmd_beep(char* args) {
    if (args == 0) {
        som(750);
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
                        print_info(lista_vars[j].valor_referencia);
                        i += len;
                        achou = 1;
                        break;
                    }
                }
            }

            if (!achou) {
                char s[2] = {'$', '\0'};
                print(s);
            }
        } 
        else {
            char s[2] = {0, 0};
            s[0] = args[i];
            print(s);
        }
    }
}

void cpuid(uint32_t code, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax),
          "=b"(*ebx),
          "=c"(*ecx),
          "=d"(*edx)
        : "a"(code)
    );
}

void cmd_cpu(char* args) {
    (void)args;
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);

    char vendor[13];
    ((uint32_t*)vendor)[0] = ebx;
    ((uint32_t*)vendor)[1] = edx;
    ((uint32_t*)vendor)[2] = ecx;
    vendor[12] = '\0';

    print("\nFabricante: ");
    print_info(vendor);

    print("\nNome: ");
    cpufetch(0);
}

void cpufetch(char* args) {
    (void)args;
    uint32_t regs[4];
    char brand[49];

    for (int i = 0; i < 3; i++) {
        cpuid(0x80000002 + i, &regs[0], &regs[1], &regs[2], &regs[3]);
        
        ((uint32_t*)brand)[i * 4 + 0] = regs[0];
        ((uint32_t*)brand)[i * 4 + 1] = regs[1];
        ((uint32_t*)brand)[i * 4 + 2] = regs[2];
        ((uint32_t*)brand)[i * 4 + 3] = regs[3];
    }
    
    brand[48] = '\0';
    print_info(brand);
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
    print_info("\nReiniciando sistema...");
    sleep(100);
    
    uint8_t temp;
    do {
        temp = inb(0x64);
    } while (temp & 0x02);
    
    outb(0x64, 0xFE);
}

void attvar(char* nome_var, char* novo_valor) {
    extern int num_vars;
    extern sys_var lista_vars[];

    for (int i = 0; i < num_vars; i++) {
        if (strdif(nome_var, lista_vars[i].nome) == 0) {
            int j = 0;
            while (novo_valor[j] != '\0' && j < 127) {
                lista_vars[i].valor_referencia[j] = novo_valor[j];
                j++;
            }
            lista_vars[i].valor_referencia[j] = '\0';
            return;
        }
    }
}

void cmd_set(char* args) {    
    if (args == 0 || args[0] == '\0') {
        print_error("\nUso: set <nome> <valor>");
        return;
    }

    char nome[32];
    char valor[128];
    int i = 0, k = 0;

    while (args[i] != ' ' && args[i] != '\0' && i < 31) {
        nome[i] = args[i];
        i++;
    }
    nome[i] = '\0';

    if (args[i] == ' ') {
        i++;
        while (args[i] != '\0' && k < 127) {
            valor[k++] = args[i++];
        }
    }
    valor[k] = '\0';

    attvar(nome, valor);

    if (strdif(smod, "1") == 0) {
        limpatela();
        cor(VERDE);
        print("MyceliumOS Terminal ");
        print(versao);
        cmd_fetch(0);
    }
}

void cmd_fetch(char* args) {
    (void)args;
    int t = setcolor;
    
    cor(VERMELHO);
    print("\n     .-'~~~-.           ");
    print_error("MyceliumOS ");
    print_info(codename);
    print_info(" (");
    print_info(versao);
    print_info(")");
    print("\n   .'");
    print("o  oOOOo");
    print(".`         ");
    print_error("---------\n");
    print("  :~~~-.");
    print("oOo   o");
    print(".  `     ");
    print_error("CPU: ");
    cpufetch(0);
    print("\n   `. \\ ~-.  ");
    print("oOOo");
    print(".      ");
    print_error("Kernel: ");
    print_info("Mycelium-x86\n");
    print("     `.; / ~.  ");
    print("OO");
    print(":      ");
    print_error("Data/Hora: ");
    cmd_time(0);
    print("\n      .'  ;-- `.");
    print("o");
    print(".'\n");
    print("    ,'  ; ~~--'~\n");
    print("    ;  ;\n");
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
    {"echo", cmd_echo},
    {"set", cmd_set},
    {"rtc", cmd_time}
};

int num_comandos = sizeof(lista_comandos) / sizeof(cmd);

void pcmd(char* input) {
    if (input[0] == '\0') return;

    char cmd_part[32];
    char args_temp[256];
    char* args_ptr = 0;
    int i = 0;

    while (input[i] != ' ' && input[i] != '\0' && i < 31) {
        cmd_part[i] = input[i];
        i++;
    }
    cmd_part[i] = '\0';
    mtom(cmd_part);

    if (input[i] == ' ') {
        i++;
        int k = 0;
        while (input[i] != '\0' && k < 255) {
            args_temp[k] = input[i];
            i++;
            k++;
        }
        args_temp[k] = '\0';
        args_ptr = args_temp;
    }

    for (int j = 0; j < num_comandos; j++) {
        if (strdif(cmd_part, lista_comandos[j].name) == 0) {
            lista_comandos[j].func(args_ptr ? args_ptr : ""); 
            
            if (strdif(cmd_part, "limpar") != 0) {
                prompt();
            } else {
                print("[");
                updatertc();
                horat();
                print("] MyceliumOS> ");
            }
            
            return;
        }
    }

    print_error("\nComando nao encontrado.");
    prompt();
}

sys_var lista_vars[10];
int num_vars = 0;

void registrar_var(char* nome, char* valor_inicial) {
    if (num_vars >= 10) return;
    lista_vars[num_vars].nome = nome;
    lista_vars[num_vars].valor_referencia = valor_inicial;
    num_vars++;
}

void init_vars() {
    num_vars = 0;
    registrar_var("ver", versao);
    registrar_var("name", codename);
    registrar_var("cx", cursor_x_str);
    registrar_var("cy", cursor_y_str);
    registrar_var("smod", smod);
}