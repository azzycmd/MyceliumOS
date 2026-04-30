#include "cmd.h"
#include "kernel.h"
#include "lib.h"
#include "io.h"
#include "vga.h"
#include "som.h"

extern char smod[];

// Protótipos auxiliares
void attvar(char* nome_var, char* novo_valor);
void cpufetch();

void cmd_ajuda(int argc, char** argv) {
    (void)argc; (void)argv;
    extern int num_comandos;
    extern cmd lista_comandos[];
    
    print("\n--- Manual do MyceliumOS ---\n");
    for (int i = 0; i < num_comandos; i++) {
        print(lista_comandos[i].name);
        if (i != num_comandos - 1) print(", ");
    }
}

void cmd_time(int argc, char** argv) {
    (void)argc; (void)argv;
    char s = setcolor;
    cor(CIANO);
    updatertc();
    datat();
    print(" - ");
    horat();
    print("\n");
    cor(s);
}

void cmd_beep(int argc, char** argv) {
    if (argc < 2) {
        som(750);
    } else {
        int freq = atoi(argv[1]);
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

void cmd_echo(int argc, char** argv) {
    extern int num_vars;
    extern sys_var lista_vars[];

    if (argc < 2) {
        print("\n");
        return;
    }

    print("\n");
    for (int a = 1; a < argc; a++) {
        char* args = argv[a];
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
                if (!achou) print("$");
            } 
            else {
                char s[2] = {args[i], 0};
                print(s);
            }
        }
        if (a < argc - 1) print(" "); // Espaço entre argumentos
    }
}

void cpuid(uint32_t code, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile ("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(code));
}

void cmd_cpu(int argc, char** argv) {
    (void)argc; (void)argv;
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
    cpufetch();
}

void cpufetch() {
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

void cmd_oi(int argc, char** argv) {
    (void)argc; (void)argv;
    print("\nOIIIII :3");
}

void cmd_cls(int argc, char** argv) {
    (void)argc; (void)argv;
    limpatela();
    set_cursor_pos(0, 0);
}

void cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    print_info("\nReiniciando sistema...");
    sleep(100);
    uint8_t temp;
    do { temp = inb(0x64); } while (temp & 0x02);
    outb(0x64, 0xFE);
}

void cmd_set(int argc, char** argv) {    
    if (argc < 3) {
        print_error("\nUso: set <nome> <valor>");
        return;
    }
    attvar(argv[1], argv[2]);

    if (strdif(smod, "1") == 0) {
        limpatela();
        cor(VERDE);
        print("MyceliumOS Terminal ");
        print(versao);
        cmd_fetch(0, 0);
    }
}

void cmd_hex(int argc, char** argv) {
    if (argc < 2) {
        print_error("\nUso: hex <endereco> (Ex: hex 0x100000)");
        return;
    }

    uint32_t endereco = (uint32_t)htoi(argv[1]);
    
    if (endereco < 0x400 && endereco != 0) {
        print_error("\nErro: Endereco protegido ou invalido.");
        return;
    }

    uint8_t* ptr = (uint8_t*)endereco;

    print("\nDump: ");
    print_hex(endereco);
    print("\n");

    for (int i = 0; i < 64; i++) {
        print_hex_byte(ptr[i]); 
        print(" ");
        if ((i + 1) % 8 == 0) print(" ");
        if ((i + 1) % 16 == 0) print("\n");
    }
}

void cmd_fetch(int argc, char** argv) {
    (void)argc; (void)argv;
    int t = setcolor;
    print("MyceliumOS "); print(codename); print(" "); print(versao);
    cor(VERMELHO);
    print("\n     .-'~~~-.           ");
    print_error("MyceliumOS "); print_info(codename);
    print_info(" ("); print_info(versao); print_info(")");
    print("\n   .`o  oOOOo.`         ");
    print_error("---------\n");
    print("  :~~~-.oOo   o.  `     ");
    print_error("CPU: "); cpufetch();
    print("\n   `. \\ ~-.  oOOo.      ");
    print_error("Kernel: "); print_info("Mycelium-x86\n");
    print("     `.; / ~.  OO:      ");
    print_error("Data/Hora: "); cmd_time(0, 0);
    print("\n      .'  ;-- `.o.'\n");
    print("    ,'  ; ~~--'~\n");
    print("    ;  ;\n");
    print("_\\;_\\//_________\n");
    cor(t);
}

void cmd_color(int argc, char** argv) {
    typedef struct { char* nome; char valor; } argcor;
    static argcor tabela_cores[] = {
        {"verde", 0x0A}, {"azul", 0x0B}, {"vermelho", 0x0C},
        {"rosa", 0x0D}, {"amarelo", 0x0E}, {"branco", 0x0F}
    };

    if (argc < 2) {
        print_error("\nUso: color <nome_da_cor>");
        return;
    }
    mtom(argv[1]);
    for (int i = 0; i < 6; i++) {
        if (strdif(argv[1], tabela_cores[i].nome) == 0) {
            cor(tabela_cores[i].valor);
            return;
        }
    }
    print_error("\nCor nao reconhecida.");
}

void cmd_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    char tempo_str[16];
    itoa(timer_ticks / 100, tempo_str);
    print("\nLigado faz "); print(tempo_str); print(" segundos.");
}

cmd lista_comandos[] = {
    {"ajuda", cmd_ajuda}, {"cls", cmd_cls}, {"reboot", cmd_reboot},
    {"oi", cmd_oi}, {"beep", cmd_beep}, {"cpuinfo", cmd_cpu},
    {"fetch", cmd_fetch}, {"color", cmd_color}, {"uptime", cmd_uptime},
    {"echo", cmd_echo}, {"set", cmd_set}, {"rtc", cmd_time}, {"hex", cmd_hex}
};

int num_comandos = sizeof(lista_comandos) / sizeof(cmd);

void pcmd(char* input) {
    if (input[0] == '\0') return;

    static char input_copy[256];
    int z = 0;
    while(input[z]) { input_copy[z] = input[z]; z++; }
    input_copy[z] = '\0';

    char* argv[10];
    int argc = 0;
    char* ptr = input_copy;

    while (*ptr != '\0' && argc < 10) {
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;
        
        argv[argc++] = ptr;
        
        while (*ptr != ' ' && *ptr != '\0') ptr++;
        
        if (*ptr == ' ') { 
            *ptr = '\0';
            ptr++; 
        }
    }

    if (argc == 0) return;

    mtom(argv[0]);

    for (int j = 0; j < num_comandos; j++) {
        if (strdif(argv[0], lista_comandos[j].name) == 0) {
            lista_comandos[j].func(argc, argv);
            
            if (strdif(argv[0], "cls") != 0) {
                prompt();
            } else {
                char s = setcolor;
                cor(CIANO); print("["); updatertc(); horat(); print("]"); cor(s); print(" MyceliumOS> ");
            }
            return;
        }
    }

    print_error("\nComando nao encontrado: ");
    print_error(argv[0]);
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

void attvar(char* nome_var, char* novo_valor) {
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

void init_vars() {
    num_vars = 0;
    registrar_var("ver", versao);
    registrar_var("name", codename);
    registrar_var("cx", cursor_x_str);
    registrar_var("cy", cursor_y_str);
    registrar_var("smod", smod);
}