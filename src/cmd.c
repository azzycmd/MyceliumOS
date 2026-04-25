#include "cmd.h"
#include "kernel.h"
#include "lib.h"
#include "io.h"
#include "vga.h"

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
    while (cmos(0x0A) & 0x80);

    unsigned char segundo = bcdtodecimal(cmos(0x00));
    unsigned char minuto  = bcdtodecimal(cmos(0x02));
    unsigned char hora    = bcdtodecimal(cmos(0x04));
    unsigned char dia     = bcdtodecimal(cmos(0x07));
    unsigned char mes     = bcdtodecimal(cmos(0x08));
    unsigned char ano     = bcdtodecimal(cmos(0x09));

    int hora_local = (int)hora - 3;
    if (hora_local < 0) hora_local += 24;

    char buffer_tempo[32];
    print("\n");
    
    itoa(dia, buffer_tempo); print_info(buffer_tempo); print_info("/");
    itoa(mes, buffer_tempo); print_info(buffer_tempo); print_info("/20");
    itoa(ano, buffer_tempo); print_info(buffer_tempo);
    
    print_info(" - ");

    itoa(hora_local, buffer_tempo); print_info(buffer_tempo); print_info(":");
    itoa(minuto, buffer_tempo);
    if (minuto < 10) {print_info("0");}
    print_info(buffer_tempo); 
    print_info(":");
    itoa(segundo, buffer_tempo);
    if (segundo < 10) {print_info("0");}
    print_info(buffer_tempo);
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

void cpuid(uint32_t code, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"                  // Chama a instrução CPUID
        : "=a"(*eax),            // Saída: registrador EAX vai para a variável eax
          "=b"(*ebx),            // Saída: registrador EBX vai para a variável ebx
          "=c"(*ecx),            // Saída: registrador ECX vai para a variável ecx
          "=d"(*edx)             // Saída: registrador EDX vai para a variável edx
        : "a"(code)              // Entrada: coloca o 'code' no registrador EAX
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
    uint32_t regs[4]; // Array para EAX, EBX, ECX, EDX
    char brand[49];   // Buffer para os 48 caracteres + finalizador \0

    for (int i = 0; i < 3; i++) {
        // Chamamos 0x80000002, depois 0x80000003, depois 0x80000004
        cpuid(0x80000002 + i, &regs[0], &regs[1], &regs[2], &regs[3]);
        
        // Copiamos os 16 bytes deste lote para o buffer
        // O cast para (uint32_t*) faz a mágica de copiar 4 bytes por vez
        ((uint32_t*)brand)[i * 4 + 0] = regs[0];
        ((uint32_t*)brand)[i * 4 + 1] = regs[1];
        ((uint32_t*)brand)[i * 4 + 2] = regs[2];
        ((uint32_t*)brand)[i * 4 + 3] = regs[3];
    }
    
    brand[48] = '\0';
    
    // Agora é só imprimir!
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
    
    cor(VERMELHO);
    print("\n     .-'~~~-.           ");
    print_error("MyceliumOS ");
    print_info(codename);
    print_info(" (")
    print_info(versao);
    print_info(")")
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
    print(":\n");
    print("      .'  ;-- `.");
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
    {"rtc", cmd_time}
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
    {"cursor", cursorstr},
    {"name", codename},
};

int num_vars = sizeof(lista_vars) / sizeof(sys_var);