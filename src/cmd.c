#include "cmd.h"
#include "kernel.h"
#include "lib.h"
#include "vga.h"
#include "som.h"
#include "io.h"
#include "kbd.h"

extern char smod[];

void attvar(char* nome_var, char* novo_valor);
void cpufetch();

enum {
    CMD_OK = 0,
    CMD_ERR_USAGE = 1,
    CMD_ERR_NOT_FOUND = 2,
    CMD_ERR_PARSE = 3,
    CMD_ERR_DENIED = 4,
    CMD_ERR_RANGE = 5
};

#define CMD_MAX_ARGS 10
#define CMD_INPUT_MAX 256

static int ultimo_status = CMD_OK;
static char ultimo_status_str[16] = "0";

typedef struct {
    char* alias;
    char* canonical;
} cmd_alias;

static cmd_alias lista_aliases[] = {
    {"help", "ajuda"},
    {"clear", "cls"},
    {"shutdown", "desligar"}
};

static int num_aliases = sizeof(lista_aliases) / sizeof(cmd_alias);

static char* resolver_alias(char* nome) {
    for (int i = 0; i < num_aliases; i++) {
        if (strdif(nome, lista_aliases[i].alias) == 0) {
            return lista_aliases[i].canonical;
        }
    }
    return nome;
}

static void print_status(int status) {
    ultimo_status = status;
    itoa(ultimo_status, ultimo_status_str);
    if (status == CMD_OK) return;

    print_error(" (error: ");
    print_error(ultimo_status_str);
    print_error(")");
}

static void print_cmd_help(cmd* c) {
    print("\n");
    print_info(c->name);
    print("\nUso: ");
    print(c->usage);
    print("\n");
    print(c->help);
}

int cmd_ajuda(int argc, char** argv) {
    extern int num_comandos;
    extern cmd lista_comandos[];

    if (argc >= 2) {
        mtom(argv[1]);
        argv[1] = resolver_alias(argv[1]);
        for (int i = 0; i < num_comandos; i++) {
            if (strdif(argv[1], lista_comandos[i].name) == 0) {
                print_cmd_help(&lista_comandos[i]);
                return CMD_OK;
            }
        }
        print_error("\nAjuda indisponivel para: ");
        print_error(argv[1]);
        return CMD_ERR_NOT_FOUND;
    }
    
    print("\n--- Manual do MyceliumOS ---\n");
    for (int i = 0; i < num_comandos; i++) {
        print(lista_comandos[i].name);
        if (i != num_comandos - 1) print(", ");
    }
    print("\nUse: ajuda <comando>");
    return CMD_OK;
}

int cmd_time(int argc, char** argv) {
    (void)argc; (void)argv;
    char s = setcolor;
    cor(CIANO);
    updatertc();
    datat();
    print(" - ");
    horat();
    print("\n");
    cor(s);
    return CMD_OK;
}

int cmd_beep(int argc, char** argv) {
    if (argc < 2) {
        som(750);
    } else {
        int freq = atoi(argv[1]);
        if (freq > 20 && freq < 20000) {
            som(freq);
        } else { 
            print_error("\nFrequencia invalida (20-20000)");
            return CMD_ERR_RANGE;
        }
    }
    __asm__ volatile("sti");
    sleep(20); 
    parar_som();
    return CMD_OK;
}

int cmd_echo(int argc, char** argv) {
    extern int num_vars;
    extern sys_var lista_vars[];

    if (argc < 2) {
        print("\n");
        return CMD_OK;
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
    return CMD_OK;
}

void cpuid(uint32_t code, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile ("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(code));
}

int cmd_cpu(int argc, char** argv) {
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
    return CMD_OK;
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

int cmd_oi(int argc, char** argv) {
    (void)argc; (void)argv;
    print("\nOIIIII :3");
    return CMD_OK;
}

int cmd_cls(int argc, char** argv) {
    (void)argc; (void)argv;
    limpatela();
    set_cursor_pos(0, 0);
    return CMD_OK;
}

int cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    print_info("\nReiniciando sistema...");
    sleep(20);
    system_reboot();
    return CMD_OK;
}

int cmd_desligar(int argc, char** argv) {
    (void)argc; (void)argv;
    print_info("\nDesligando sistema...");
    sleep(20);
    system_poweroff();
    return CMD_OK;
}

int cmd_panic(int argc, char** argv) {
    static char motivo[192];
    int pos = 0;
    if (strdif(smod, "1") == 0) {
        if (argc >= 3 && strdif(argv[1], "--simulate") == 0 &&
            strdif(argv[2], "irqstorm") == 0) {
            kernel_panic_code_at(PANIC_CODE_IRQSTORM_SIM,
                                 "panic simulado: irqstorm", "shell", 0);
            return CMD_OK;
        }

        if (argc < 2) {
            kernel_panic_code_at(PANIC_CODE_MANUAL,
                                 "panic manual solicitado pelo usuario", "shell", 0);
            return CMD_OK;
        }

        for (int a = 1; a < argc && pos < 191; a++) {
            if (a > 1) {
                motivo[pos++] = ' ';
            }

            for (int i = 0; argv[a][i] != '\0' && pos < 191; i++) {
                motivo[pos++] = argv[a][i];
            }
        }

        motivo[pos] = '\0';
        kernel_panic_code_at(PANIC_CODE_MANUAL, motivo, "shell", 0);
    } else {
        print_error("\nsmod: Voce nao tem permissao para esse comando");
        return CMD_ERR_DENIED;
    }
    return CMD_OK;
}

int cmd_set(int argc, char** argv) {    
    if (argc < 3) {
        print_error("\nUso: set <nome> <valor>");
        return CMD_ERR_USAGE;
    }
    attvar(argv[1], argv[2]);

    if (strdif(smod, "1") == 0) {
        limpatela();
        cor(VERDE);
        cmd_fetch(0, 0);
    }
    return CMD_OK;
}

#define HEX_DUMP_BYTES 64
#define HEX_DUMP_MAX_BYTES 256
#define HEX_SAFE_MIN 0x00000400U
#define HEX_SAFE_MAX 0x04000000U

static int parse_hex_u32(char* str, uint32_t* out) {
    uint32_t res = 0;
    int i = 0;
    int digits = 0;

    while (str[i] == ' ') i++;
    if (str[i] == '0' && (str[i + 1] == 'x' || str[i + 1] == 'X')) i += 2;

    while (str[i] != '\0') {
        int v;
        if (str[i] >= '0' && str[i] <= '9') v = str[i] - '0';
        else if (str[i] >= 'a' && str[i] <= 'f') v = str[i] - 'a' + 10;
        else if (str[i] >= 'A' && str[i] <= 'F') v = str[i] - 'A' + 10;
        else return 0;

        if (digits >= 8) return 0;
        res = (res << 4) | (uint32_t)v;
        digits++;
        i++;
    }

    if (digits == 0) return 0;
    *out = res;
    return 1;
}

int cmd_hex(int argc, char** argv) {
    if (argc < 2) {
        print_error("\nUso: hex <endereco> [bytes] (Ex: hex 0x100000 128)");
        return CMD_ERR_USAGE;
    }

    uint32_t endereco;
    if (!parse_hex_u32(argv[1], &endereco)) {
        print_error("\nErro: endereco hexadecimal invalido.");
        return CMD_ERR_PARSE;
    }

    uint32_t bytes = HEX_DUMP_BYTES;
    if (argc >= 3) {
        bytes = (uint32_t)atoi(argv[2]);
        if (bytes == 0 || bytes > HEX_DUMP_MAX_BYTES) {
            print_error("\nErro: bytes fora do intervalo (1-256).");
            return CMD_ERR_RANGE;
        }
    }
    
    if (endereco < HEX_SAFE_MIN || endereco > (HEX_SAFE_MAX - bytes)) {
        print_error("\nErro: endereco fora da area segura (0x00000400-0x03FFFFFF).");
        return CMD_ERR_RANGE;
    }

    uint8_t* ptr = (uint8_t*)endereco;

    print("\nDump: ");
    print_hex(endereco);
    print("\n");

    for (uint32_t i = 0; i < bytes; i++) {
        if ((i % 16) == 0) {
            print_hex(endereco + i);
            print(": ");
        }
        print_hex_byte(ptr[i]); 
        print(" ");
        if ((i + 1) % 8 == 0) print(" ");
        if ((i + 1) % 16 == 0) print("\n");
    }
    return CMD_OK;
}

int cmd_fetch(int argc, char** argv) {
    (void)argc; (void)argv;
    int t = setcolor;
    if (!booting) print("\n");
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
    return CMD_OK;
}

int cmd_color(int argc, char** argv) {
    typedef struct { char* nome; char valor; } argcor;
    static argcor tabela_cores[] = {
        {"verde", 0x0A}, {"azul", 0x0B}, {"vermelho", 0x0C},
        {"rosa", 0x0D}, {"amarelo", 0x0E}, {"branco", 0x0F}
    };

    if (argc < 2) {
        print_error("\nUso: color <nome_da_cor>");
        return CMD_ERR_USAGE;
    }
    mtom(argv[1]);
    for (int i = 0; i < 6; i++) {
        if (strdif(argv[1], tabela_cores[i].nome) == 0) {
            cor(tabela_cores[i].valor);
            return CMD_OK;
        }
    }
    print_error("\nCor nao reconhecida.");
    return CMD_ERR_NOT_FOUND;
}

int cmd_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    char tempo_str[16];
    itoa(timer_ticks / 100, tempo_str);
    print("\nLigado faz "); print(tempo_str); print(" segundos.");
    return CMD_OK;
}

int cmd_ticks(int argc, char** argv) {
    (void)argc; (void)argv;
    char num[16];

    itoa((int)timer_ticks, num);
    print("\nTicks PIT: ");
    print_info(num);
    print("\nSegundos aproximados: ");
    itoa((int)(timer_ticks / 100), num);
    print_info(num);
    return CMD_OK;
}

int cmd_cpuregs(int argc, char** argv) {
    (void)argc; (void)argv;
    uint32_t cr0, cr2, cr3, cr4, eflags;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("pushf; pop %0" : "=r"(eflags));

    print("\n--- CPU registers ---");
    print("\nCR0="); print_hex(cr0);
    print(" CR2="); print_hex(cr2);
    print("\nCR3="); print_hex(cr3);
    print(" CR4="); print_hex(cr4);
    print("\nEFLAGS="); print_hex(eflags);
    return CMD_OK;
}

int cmd_idt(int argc, char** argv) {
    char num[16];
    int start = 0;
    int end = 47;

    if (argc >= 2) {
        start = atoi(argv[1]);
        if (start < 0 || start > 255) {
            print_error("\nVetor IDT fora do intervalo (0-255)");
            return CMD_ERR_RANGE;
        }
        end = start;
    }

    print("\n--- IDT ---");
    print("\nready: ");
    print_info(idt_is_ready() ? "sim" : "nao");
    for (int i = start; i <= end; i++) {
        uint32_t base = idt_gate_base((uint8_t)i);
        uint16_t sel = idt_gate_selector((uint8_t)i);
        uint8_t flags = idt_gate_flags((uint8_t)i);
        if (argc < 2 && base == 0 && flags == 0) continue;

        print("\n#");
        itoa(i, num);
        print_info(num);
        print(" base=");
        print_hex(base);
        print(" sel=");
        print_hex((uint32_t)sel);
        print(" flags=");
        print_hex((uint32_t)flags);
    }
    return CMD_OK;
}

int cmd_irq(int argc, char** argv) {
    (void)argc; (void)argv;
    char num[16];

    print("\n--- IRQ counters ---");
    print("\nIRQ0 timer: ");
    itoa((int)irq_timer_count, num);
    print_info(num);
    print("\nIRQ1 teclado: ");
    itoa((int)irq_keyboard_count, num);
    print_info(num);
    print("\nIRQ12 mouse: ");
    itoa((int)irq_mouse_count, num);
    print_info(num);
    return CMD_OK;
}

int cmd_kbdinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    char num[16];

    print("\n--- Keyboard info ---");
    print("\nbackend boot: ");
    print_info(boot_input_mode);
    print("\nPS/2 ativo: ");
    print_info(kbd_ps2_enabled() ? "sim" : "nao");
    print("\nshift: ");
    print_info(shifton ? "on" : "off");
    print("\nlinha: ");
    itoa(buffer_index, num);
    print_info(num);
    print("\nfila scancode: ");
    itoa(kbd_queue_count(), num);
    print_info(num);
    print("\nhistorico: ");
    itoa(kbd_history_count(), num);
    print_info(num);
    return CMD_OK;
}

int cmd_memmap(int argc, char** argv) {
    (void)argc; (void)argv;
    char num[16];

    print("\n--- Memmap basico ---");
    print("\nmultiboot info: ");
    print_hex(boot_multiboot_info);
    print("\nmultiboot size: ");
    print_hex(boot_multiboot_total_size);
    print("\nhex safe range: ");
    print_info("0x00000400-0x03FFFFC0");
    print("\nframebuffer: ");
    print_info(usar_framebuffer ? "sim" : "nao");
    if (usar_framebuffer) {
        print("\nfb size: ");
        itoa((int)vga_framebuffer_width(), num);
        print_info(num);
        print("x");
        itoa((int)vga_framebuffer_height(), num);
        print_info(num);
        print("x");
        itoa((int)vga_framebuffer_bpp(), num);
        print_info(num);
    }
    print("\nnormalizado: ");
    print_info("nao (planejado para v0.3.x)");
    return CMD_OK;
}

static uint32_t pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000U |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)dev << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

int cmd_pci(int argc, char** argv) {
    (void)argc; (void)argv;
    char num[16];
    int found = 0;

    print("\n--- PCI bus scan ---");
    for (int bus = 0; bus < 256 && found < 32; bus++) {
        for (int dev = 0; dev < 32 && found < 32; dev++) {
            for (int func = 0; func < 8 && found < 32; func++) {
                uint32_t id = pci_read_config((uint8_t)bus, (uint8_t)dev,
                                              (uint8_t)func, 0x00);
                if ((id & 0xFFFF) == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }

                uint32_t class_reg = pci_read_config((uint8_t)bus, (uint8_t)dev,
                                                     (uint8_t)func, 0x08);
                print("\n");
                itoa(bus, num); print_info(num);
                print(":");
                itoa(dev, num); print_info(num);
                print(".");
                itoa(func, num); print_info(num);
                print(" id=");
                print_hex(id);
                print(" class=");
                print_hex(class_reg >> 8);
                found++;

                if (func == 0) {
                    uint32_t header = pci_read_config((uint8_t)bus, (uint8_t)dev,
                                                      0, 0x0C);
                    if (((header >> 16) & 0x80) == 0) break;
                }
            }
        }
    }

    if (found == 0) {
        print_error("\nNenhum dispositivo PCI encontrado.");
        return CMD_ERR_NOT_FOUND;
    }
    if (found >= 32) {
        print("\n(limite de exibicao: 32 dispositivos)");
    }
    return CMD_OK;
}

int cmd_bench(int argc, char** argv) {
    (void)argc; (void)argv;
    volatile uint32_t acc = 0;
    uint32_t start = timer_ticks;
    char num[16];

    for (uint32_t i = 0; i < 500000U; i++) {
        acc = (acc * 33U) ^ i;
    }

    print("\n--- Bench simples ---");
    print("\nticks: ");
    itoa((int)(timer_ticks - start), num);
    print_info(num);
    print("\nacc: ");
    print_hex(acc);
    return CMD_OK;
}

static void print_bool(char* nome, int valor) {
    print("\n");
    print(nome);
    print(": ");
    print_info(valor ? "sim" : "nao");
}

int cmd_bootinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    char num[16];

    print("\n--- Bootinfo ---");
    print("\ncmdline: ");
    print_info((boot_cmdline && boot_cmdline[0]) ? boot_cmdline : "(vazia)");
    print("\nvideo pedido: ");
    print_info(boot_video_request);
    print("\nvideo ativo: ");
    print_info(boot_video_mode);
    print("\nentrada: ");
    print_info(boot_input_mode);
    print_bool("safe-mode", boot_safe_mode);
    print_bool("som", boot_sound_enabled);
    print_bool("usbdbg", boot_usb_debug);
    print_bool("framebuffer", usar_framebuffer);
    print("\nstdout writes: ");
    itoa((int)kstream_stdout_writes, num);
    print_info(num);
    print("\nstderr writes: ");
    itoa((int)kstream_stderr_writes, num);
    print_info(num);

    print("\nterminal: ");
    itoa(max_cols, num);
    print_info(num);
    print("x");
    itoa(max_rows, num);
    print_info(num);

    if (usar_framebuffer) {
        print("\nfb: ");
        itoa((int)vga_framebuffer_width(), num);
        print_info(num);
        print("x");
        itoa((int)vga_framebuffer_height(), num);
        print_info(num);
        print("x");
        itoa((int)vga_framebuffer_bpp(), num);
        print_info(num);
    }
    return CMD_OK;
}

cmd lista_comandos[] = {
    {"ajuda", "ajuda [comando]", "Mostra a lista de comandos ou o uso detalhado de um comando.", cmd_ajuda},
    {"cls", "cls", "Limpa a tela e reposiciona o cursor.", cmd_cls},
    {"reboot", "reboot", "Reinicia a maquina via controlador de teclado.", cmd_reboot},
    {"desligar", "desligar", "Tenta desligar a maquina via portas ACPI/QEMU conhecidas.", cmd_desligar},
    {"oi", "oi", "Mostra uma saudacao simples.", cmd_oi},
    {"beep", "beep [frequencia]", "Toca o speaker na frequencia informada ou usa 750 Hz.", cmd_beep},
    {"cpuinfo", "cpuinfo", "Mostra fabricante e nome da CPU via CPUID.", cmd_cpu},
    {"fetch", "fetch", "Mostra resumo visual do sistema.", cmd_fetch},
    {"color", "color <verde|azul|vermelho|rosa|amarelo|branco>", "Altera a cor ativa do texto.", cmd_color},
    {"uptime", "uptime", "Mostra o tempo ligado em segundos.", cmd_uptime},
    {"ticks", "ticks", "Mostra uptime bruto em ticks do PIT.", cmd_ticks},
    {"echo", "echo <texto>", "Imprime texto e expande variaveis com $nome.", cmd_echo},
    {"set", "set <nome> <valor>", "Atualiza uma variavel registrada do sistema.", cmd_set},
    {"rtc", "rtc", "Mostra data e hora lidas do RTC.", cmd_time},
    {"hex", "hex <endereco> [bytes]", "Mostra dump hexadecimal paginado de memoria segura.", cmd_hex},
    {"panic", "panic [motivo]|--simulate irqstorm", "Dispara kernel panic manual ou simulacao quando smod=1.", cmd_panic},
    {"bootinfo", "bootinfo", "Mostra opcoes detectadas no boot e modo ativo.", cmd_bootinfo},
    {"cpuregs", "cpuregs", "Mostra registradores CR0/CR2/CR3/CR4 e EFLAGS.", cmd_cpuregs},
    {"idt", "idt [vetor]", "Mostra entradas instaladas da IDT ou um vetor especifico.", cmd_idt},
    {"irq", "irq", "Mostra contadores basicos de IRQ.", cmd_irq},
    {"memmap", "memmap", "Mostra mapa de memoria basico conhecido pelo kernel.", cmd_memmap},
    {"pci", "pci", "Escaneia dispositivos PCI pelo config space.", cmd_pci},
    {"kbdinfo", "kbdinfo", "Mostra backend e estado basico do teclado.", cmd_kbdinfo},
    {"bench", "bench", "Executa benchmark simples de CPU/ticks.", cmd_bench}
};

int num_comandos = sizeof(lista_comandos) / sizeof(cmd);

static int append_char(char* out, int* pos, char c) {
    if (*pos >= CMD_INPUT_MAX - 1) return 0;
    out[*pos] = c;
    *pos = *pos + 1;
    out[*pos] = '\0';
    return 1;
}

static int parse_args(char* input, char* out, char** argv, int* argc) {
    int pos = 0;
    int quote = 0;
    int escape = 0;
    int in_arg = 0;

    *argc = 0;
    out[0] = '\0';

    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];

        if (escape) {
            if (!in_arg) {
                if (*argc >= CMD_MAX_ARGS) return CMD_ERR_USAGE;
                argv[*argc] = &out[pos];
                *argc = *argc + 1;
                in_arg = 1;
            }
            if (!append_char(out, &pos, c)) return CMD_ERR_PARSE;
            escape = 0;
            continue;
        }

        if (c == '\\') {
            escape = 1;
            continue;
        }

        if (c == '"') {
            if (!in_arg) {
                if (*argc >= CMD_MAX_ARGS) return CMD_ERR_USAGE;
                argv[*argc] = &out[pos];
                *argc = *argc + 1;
                in_arg = 1;
            }
            quote = !quote;
            continue;
        }

        if (c == '|' && !quote) {
            print_error("\nErro: pipes ainda nao estao implementados.");
            return CMD_ERR_PARSE;
        }

        if ((c == ' ' || c == '\t') && !quote) {
            if (in_arg) {
                if (!append_char(out, &pos, '\0')) return CMD_ERR_PARSE;
                in_arg = 0;
            }
            continue;
        }

        if (!in_arg) {
            if (*argc >= CMD_MAX_ARGS) return CMD_ERR_USAGE;
            argv[*argc] = &out[pos];
            *argc = *argc + 1;
            in_arg = 1;
        }

        if (!append_char(out, &pos, c)) return CMD_ERR_PARSE;
    }

    if (escape) {
        print_error("\nErro: escape sem caractere apos barra invertida.");
        return CMD_ERR_PARSE;
    }
    if (quote) {
        print_error("\nErro: aspas nao fechadas.");
        return CMD_ERR_PARSE;
    }
    if (*argc >= CMD_MAX_ARGS && in_arg) {
        int j = 0;
        while (input[j] == ' ' || input[j] == '\t') j++;
    }

    return CMD_OK;
}

static int has_extra_arg_over_limit(char* input) {
    int argc = 0;
    int quote = 0;
    int escape = 0;
    int in_arg = 0;

    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        if (escape) {
            escape = 0;
            if (!in_arg) {
                argc++;
                in_arg = 1;
            }
            continue;
        }
        if (c == '\\') {
            escape = 1;
            continue;
        }
        if (c == '"') {
            quote = !quote;
            if (!in_arg) {
                argc++;
                in_arg = 1;
            }
            continue;
        }
        if ((c == ' ' || c == '\t') && !quote) {
            in_arg = 0;
            continue;
        }
        if (!in_arg) {
            argc++;
            in_arg = 1;
            if (argc > CMD_MAX_ARGS) return 1;
        }
    }
    return 0;
}

int cmd_autocomplete(char* input, int* index) {
    int len;
    int matches = 0;
    char* match = 0;

    if (!input || !index) return 0;
    len = strlen(input);
    if (*index != len) return 0;
    for (int i = 0; i < len; i++) {
        if (input[i] == ' ' || input[i] == '\t' || input[i] == '"' || input[i] == '\\') return 0;
    }

    for (int i = 0; i < num_comandos; i++) {
        if (strdifb(lista_comandos[i].name, input, len) == 0) {
            matches++;
            match = lista_comandos[i].name;
        }
    }
    for (int i = 0; i < num_aliases; i++) {
        if (strdifb(lista_aliases[i].alias, input, len) == 0) {
            matches++;
            match = lista_aliases[i].alias;
        }
    }

    if (matches == 1 && match) {
        while (input[len] != '\0') len++;
        int mlen = strlen(match);
        for (int i = *index; i < mlen && i < CMD_INPUT_MAX - 1; i++) {
            input[i] = match[i];
        }
        input[mlen] = '\0';
        *index = mlen;
        return 1;
    }

    if (matches > 1) {
        print("\n");
        for (int i = 0; i < num_comandos; i++) {
            if (strdifb(lista_comandos[i].name, input, len) == 0) {
                print(lista_comandos[i].name);
                print(" ");
            }
        }
        for (int i = 0; i < num_aliases; i++) {
            if (strdifb(lista_aliases[i].alias, input, len) == 0) {
                print(lista_aliases[i].alias);
                print(" ");
            }
        }
        prompt();
        print(input);
        return 0;
    }

    return 0;
}

void pcmd(char* input) {
    if (input[0] == '\0') return;

    static char input_copy[256];
    char* argv[CMD_MAX_ARGS];
    int argc = 0;
    int parse_status;

    if (has_extra_arg_over_limit(input)) {
        print_error("\nErro: limite de 10 argumentos excedido.");
        print_status(CMD_ERR_USAGE);
        prompt();
        return;
    }

    parse_status = parse_args(input, input_copy, argv, &argc);
    if (parse_status != CMD_OK) {
        if (parse_status == CMD_ERR_USAGE) {
            print_error("\nErro: limite de 10 argumentos excedido.");
        }
        print_status(parse_status);
        prompt();
        return;
    }

    if (argc == 0) return;

    mtom(argv[0]);
    argv[0] = resolver_alias(argv[0]);

    for (int j = 0; j < num_comandos; j++) {
        if (strdif(argv[0], lista_comandos[j].name) == 0) {
            int status = lista_comandos[j].func(argc, argv);
            print_status(status);
            
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
    print_status(CMD_ERR_NOT_FOUND);
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
    registrar_var("status", ultimo_status_str);
}
