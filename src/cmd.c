#include "cmd.h"
#include "kernel.h"
#include "lib.h"
#include "io.h"
#include "vga.h"
#include "som.h"

extern char smod[];

void attvar(char* nome_var, char* novo_valor) {
    extern int num_vars;
    extern sys_var lista_vars[];
    for (int i = 0; i < num_vars; i++) {
        if (strdif(nome_var, lista_vars[i].nome) == 0) {
            int j = 0;
            // Copia o novo valor para o ponteiro de referência
            while (novo_valor[j] != '\0' && j < 127) {
                lista_vars[i].valor_referencia[j] = novo_valor[j];
                j++;
            }
            lista_vars[i].valor_referencia[j] = '\0';
            return;
        }
    }
}

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
    if (argc > 2) {
        int duracao = atoi(argv[2]);
        if (duracao > 1) sleep(duracao);
        else sleep(20);
    } else {
        sleep(20);
    } 
    parar_som();
}

void cmd_hex(int argc, char** argv) {
    char s = setcolor;
    if (argc < 2) {
        print_error("\nUso: hex [flag] <endereco> [largura]");
        print("\nFlags: -a (hex/ascii - all), -s (string), -i (int)");
        return;
    }

    int arg_idx = 1;
    char modo = 'a';

    if (argv[1][0] == '-') {
        modo = argv[1][1];
        arg_idx = 2;
    }

    if (argc <= arg_idx) {
        print_error("\nErro: Endereco nao fornecido.");
        return;
    }

    uint32_t endereco = (uint32_t)htoi(argv[arg_idx]);

    int largura = 16;
    if (argc > arg_idx + 1) {
        largura = atoi(argv[arg_idx + 1]);
    }

    unsigned char* ptr = (unsigned char*)endereco;
    print("\n");

    if (modo == 'i') {
        int* val_ptr = (int*)ptr;
        char buf[16];
        itoa(*val_ptr, buf);
        print("Inteiro (32-bit): "); print_info(buf);
    } 
    else if (modo == 's') {
        print("String: ");
        cor(VERDE);
        for(int i = 0; i < largura && ptr[i] != '\0'; i++) {
            char s[2] = {ptr[i], '\0'};
            print(s);
        }
    } 
    else {
        static const char hex_chars[] = "0123456789ABCDEF";
        for (int i = 0; i < largura; i++) {
            unsigned char b = ptr[i];
            char out[4] = { hex_chars[(b >> 4) & 0x0F], hex_chars[b & 0x0F], ' ', '\0' };
            print(out);

            if ((i + 1) % 17 == 0) print("\n");
        }
    }
    cor(s);
}

void cmd_echo(int argc, char** argv) {
    if (argc < 2) {
        print("\n");
        return;
    }
    print("\n");
    for (int i = 1; i < argc; i++) {
        char* word = argv[i];
        for (int j = 0; word[j] != '\0'; j++) {
            if (word[j] == '$') {
                char* var_ptr = &word[j + 1];
                int achou = 0;
                extern int num_vars;
                extern sys_var lista_vars[];

                if (strdifb(var_ptr, "uptime", 6) == 0) {
                    char tempo_str[16];
                    itoa(timer_ticks / 100, tempo_str);
                    print_info(tempo_str); print("s");
                    j += 6; achou = 1;
                } else {
                    for (int v = 0; v < num_vars; v++) {
                        int len = 0;
                        while(lista_vars[v].nome[len] != '\0') len++;
                        if (strdifb(var_ptr, lista_vars[v].nome, len) == 0) {
                            print_info(lista_vars[v].valor_referencia);
                            j += len; achou = 1; break;
                        }
                    }
                }
                if (!achou) print("$");
            } else {
                char s[2] = {word[j], '\0'};
                print(s);
            }
        }
        if (i < argc - 1) print(" ");
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
    cpufetch(0, 0);
}

void cpufetch(int argc, char** argv) {
    (void)argc; (void)argv;
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

void cmd_limpar(int argc, char** argv) {
    (void)argc; (void)argv;
    limpatela();
    cursor_x = 0;
    cursor_y = 0;
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

void cmd_fetch(int argc, char** argv) {
    (void)argc; (void)argv;
    int t = setcolor;

    // 2. Posiciona o Cogumelo (Lado esquerdo da caixa)
    // Note que removi os \n do início das strings para controlar com cursormov
    cor(VERMELHO);
    cursormov(4, 1);  print("     .-'~~~-.           ");
    cursormov(4, 2);  print("   .'o  oOOOo.`         ");
    cursormov(4, 3);  print("  :~~~-.oOo   o.  `     ");
    cursormov(4, 4);  print("   `. \\ ~-.  oOOo.      ");
    cursormov(4, 5);  print("     `.; / ~.  OO:      ");
    cursormov(4, 6);  print("      .'  ;-- `.o.'     ");
    cursormov(4, 7);  print("    ,'  ; ~~--'~        ");
    cursormov(4, 8); print("    ;  ;                ");
    cursormov(4, 9); print("_\\;_\\//_________        ");

    cursormov(30, 2);
    print_error("MyceliumOS ");
    print_info(codename);
    print_info(" ("); print_info(versao); print_info(")");
    
    cursormov(30,3);
    print_error("---------");

    cursormov(30, 4);
    print_error("CPU: "); cpufetch(0, 0);

    cursormov(30, 5);
    print_error("Kernel: "); print_info("Mycelium-x86");

    cursormov(30, 6);
    print_error("Data/Hora: "); 
    updatertc();
    datat();
    print(" ");
    horat();

    cursormov(0, 10);
    cor(t);
}

void cmd_color(int argc, char** argv) {
    if (argc < 2) {
        print_error("\nUso: color <cor>");
        return;
    }

    // Definindo a estrutura localmente para o compilador reconhecer o tipo
    typedef struct { 
        char* nome; 
        char valor; 
    } argcor;

    static argcor tabela_cores[] = { 
        {"verde", 10}, {"azul", 9}, {"vermelho", 12}, 
        {"rosa", 13}, {"amarelo", 14}, {"branco", 15} 
    };

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
    {"ajuda", cmd_ajuda}, {"limpar", cmd_limpar}, {"reboot", cmd_reboot},
    {"oi", cmd_oi}, {"beep", cmd_beep}, {"cpuinfo", cmd_cpu},
    {"fetch", cmd_fetch}, {"color", cmd_color}, {"uptime", cmd_uptime},
    {"echo", cmd_echo}, {"set", cmd_set}, {"rtc", cmd_time}, {"hex", cmd_hex}
};
int num_comandos = sizeof(lista_comandos) / sizeof(cmd);

void pcmd(char* input) {
    if (input[0] == '\0') return;
    
    static char input_copy[256];
    int z = 0;
    while(input[z] != '\0' && z < 255) { input_copy[z] = input[z]; z++; }
    input_copy[z] = '\0';

    char* argv[10];
    int argc = 0;
    char* ptr = input_copy;

    while (*ptr != '\0' && argc < 10) {
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;
        argv[argc++] = ptr;
        while (*ptr != ' ' && *ptr != '\0') ptr++;
        if (*ptr == ' ') { *ptr = '\0'; ptr++; }
    }

    if (argc == 0) return;
    mtom(argv[0]);

    for (int j = 0; j < num_comandos; j++) {
        if (strdif(argv[0], lista_comandos[j].name) == 0) {
            lista_comandos[j].func(argc, argv);
            if (strdif(argv[0], "limpar") != 0) prompt();
            else { print("["); updatertc(); horat(); print("] MyceliumOS> "); }
            return;
        }
    }
    print_error("\nComando nao encontrado.");
    prompt();
}

sys_var lista_vars[10];
int num_vars = 0;
void registrar_var(char* nome, char* valor_inicial) {
    if (num_vars < 10) {
        lista_vars[num_vars].nome = nome;
        lista_vars[num_vars].valor_referencia = valor_inicial;
        num_vars++;
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