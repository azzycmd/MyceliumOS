#define video_memory ((char *)0xb8000)
#define print_error(msg) setcolor = vermelho; print(msg); cor(coragr);
#define print_info(msg) setcolor = azul; print(msg); cor(coragr);

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

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

typedef struct {
    char* nome;
    char* valor_estatico;
} sys_var;

struct idt_entry_struct idt[256] = { [0 ... 255] = {0, 0, 0, 0, 0} };
struct idt_ptr_struct idtp = {0, 0};

char versao[10] = "v0.1.5";

char verde = 0x0A;
char azul = 0x0B;
char vermelho = 0x0C;
char rosa = 0x0D;
char amarelo = 0x0E;
char branco = 0x0F;
char setcolor = 0x0F;
int cursorpos = 0;
char buffer[256];
int buffer_index = 0;
char coragr = 0x0F;
unsigned int timer_ticks = 0;
int shifton = 0;
char cursorstr[5] = "";

unsigned char keyboard_map[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b',   
    '\t',           
    'q', 'w', 'e', 'r', 
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0,          
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   
    '\'', '`',   0,     
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',         
    'm', ',', '.', '/',   0,                
    '*',
    0,  
    ' ',    
    0,  
    0,  
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  
    0,  
    0,  
    0,  
    0,  
    0,  
    '-',
    0,  
    0,
    0,  
    '+',
    0,  
    0,  
    0,  
    0,  
    0,  
    0,   0,   0,
    0,  
    0,  
    0,  
};

unsigned char keyboard_map_shift[128] =
{
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b',   
    '\t',           
    'Q', 'W', 'E', 'R', 
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   
    0,          
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   
    '\"', '~',   0,     
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',         
    'M', '<', '>', '?',   0,                
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

typedef void (*command_func)(char*);

typedef struct {
    char* name;
    command_func func;
} cmd;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

void idt_install() {
    idtp.limit = (uint16_t)(sizeof(struct idt_entry_struct) * 256) - 1;
    idtp.base = (uint32_t)(unsigned long)&idt; 
    for(int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);
    __asm__ volatile("lidt (%0)" : : "r"(&idtp));
}

void scroll() {
    if (cursorpos >= 80 * 25 * 2) {
        for (int i = 0; i < 80 * 24 * 2; i++) {
            video_memory[i] = video_memory[i + 80 * 2];
        }

        for (int i = 80 * 24 * 2; i < 80 * 25 * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i + 1] = setcolor;
        }

        cursorpos = 80 * 24 * 2;
    }
}

int strdif(char* s1, char* s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return s1[i] - s2[i];
}

int strdifb(char* s1, char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void m2m(char* s) {
    if (!s) return;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = s[i] + 32; // Na tabela ASCII, 'a' é 'A' + 32
        }
    }
}

void itoa(int n, char* str) {
    int i = 0;
    int temp = n;
    
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (temp > 0) {
        str[i++] = (temp % 10) + '0';
        temp /= 10;
    }
    str[i] = '\0';

    for (int j = 0; j < i / 2; j++) {
        char aux = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = aux;
    }
}

unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outb(unsigned short port, unsigned char data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

void cursor() {
    unsigned short pos = (unsigned short)(cursorpos / 2);

    outb(0x3D4, 0x0F);
    // Cast explícito para uint8_t para silenciar o compilador
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    
    outb(0x3D4, 0x0E);
    // Desloca 8 bits para a direita para pegar a parte alta
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void limpatela() {
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = setcolor;
    }
    cursorpos = 0;
    cursor();
}

void cor(char cor) {
    setcolor = cor;
    coragr = cor;
}

void print(char* msg) {
    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == '\n') {
            cursorpos = ((cursorpos / 160) + 1) * 160;
        } 
        else if (msg[i] == '\b') {
            if (cursorpos > 0) {
                cursorpos -= 2;
                video_memory[cursorpos] = ' ';
                video_memory[cursorpos + 1] = setcolor;
            }
        }
        else if (msg[i] == '\t') {
            for (int j = 0; j < 4; j++) {
                video_memory[cursorpos] = ' ';
                video_memory[cursorpos + 1] = setcolor;
                cursorpos += 2;
            }
        }
        else {
            video_memory[cursorpos] = msg[i];
            video_memory[cursorpos + 1] = setcolor;
            cursorpos += 2;
        }
        
        if (cursorpos >= 160 * 25) scroll();;
        if (cursorpos < 0) scroll();;
    }
    cursor();
}

void cpit(unsigned int frequencia) {
    uint32_t divisor = 1193180 / frequencia;

    outb(0x43, 0x36);
    // Envia o byte baixo
    outb(0x40, (uint8_t)(divisor & 0xFF));
    // Envia o byte alto
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler() {
    timer_ticks++;
    outb(0x20, 0x20);
}

void sleep(uint32_t milissegundos) {
    uint32_t tempo_final = timer_ticks + milissegundos;
    while (timer_ticks < tempo_final) {
        __asm__ volatile("hlt");
    }
}

void som(unsigned int nFrequence) {
    unsigned int Div;
    unsigned char t;
    Div = 1193180 / nFrequence;
    outb(0x43, 0xB6);
    outb(0x42, (unsigned char) (Div) );
    outb(0x42, (unsigned char) (Div >> 8));
    t = inb(0x61);
    if (t != (t | 3)) {
        outb(0x61, t | 3);
    }
}

void parar_som() {
    unsigned char t = inb(0x61) & 0xFC;
    outb(0x61, t);
}

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

int atoi(char* str) {
    int res = 0;
    int i = 0;

    while (str[i] == ' ') i++;

    for (; str[i] != '\0' && str[i] >= '0' && str[i] <= '9'; ++i) {
        res = res * 10 + str[i] - '0';
    }
    return res;
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
            char c[2] = {args[i], '\0'};
            print(c);
        }
    }
}

void cmd_cpu(char* args) {
    (void)args;
    unsigned int eax, ebx, ecx, edx;
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
    unsigned int eax, ebx, ecx, edx;
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

void cmd_reboot(char* args) __attribute__((noreturn));
void cmd_reboot(char* args) {
    (void)args;
    print_info("\nReiniciando MyceliumOS...");

    sleep(100);

    // Criamos uma IDT inválida localmente
    struct idt_ptr_struct invalid_idt = {0, 0};

    // Forçamos o carregamento da IDT vazia e geramos uma interrupção
    // Isso causa um Triple Fault e reseta qualquer PC x86 instantaneamente
    __asm__ volatile (
        "lidt %0\n\t"
        "int3"
        : : "m"(invalid_idt)
    );

    // Caso o Triple Fault falhe (muito raro), tentamos o método do teclado
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);

    // Se nada funcionar, paramos a CPU
    while(1) { __asm__ volatile("hlt"); }
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

    m2m(args);

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
    __asm__ volatile("sti");

    char cmd_part[32];
    char* args_part = 0;
    int i = 0;

    while (input[i] != ' ' && input[i] != '\0' && i < 31) {
        cmd_part[i] = input[i];
        i++;
    }
    cmd_part[i] = '\0';

    m2m(cmd_part);

    if (input[i] == ' ') {
        args_part = &input[i + 1];
    }

    for (int j = 0; j < num_comandos; j++) {
        if (strdif(cmd_part, lista_comandos[j].name) == 0) {
            lista_comandos[j].func(args_part); 
            
            // Removidos os \n iniciais. O prompt aparece logo após o comando.
            if (strdif(cmd_part, "limpar") != 0) {
                print("\nMyceliumOS> "); 
            } else {
                print("MyceliumOS> "); // No limpar, não queremos pular linha nenhuma
            }
            for(int k = 0; k < 256; k++) buffer[k] = '\0';
            return;
        }
    }
    print_error("\nComando nao encontrado.");
    print("\nMyceliumOS> ");
}

void cursor_on() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void reprogramar_pic() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x0);  outb(0xA1, 0x0);
}

void teclado() {
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        unsigned char scancode = inb(0x60);

        // Detectar SHIFT pressionado (Esquerdo: 0x2A, Direito: 0x36)
        if (scancode == 0x2A || scancode == 0x36) {
            shifton = 1;
            outb(0x20, 0x20);
            return;
        }
        // Detectar SHIFT solto (Esquerdo: 0xAA, Direito: 0xB6)
        if (scancode == 0xAA || scancode == 0xB6) {
            shifton = 0;
            outb(0x20, 0x20);
            return;
        }

        // Se o bit 7 estiver ligado, é um "Break Code" de outra tecla (ignoramos)
        if (scancode & 0x80) {
            outb(0x20, 0x20);
            return;
        }
        else {
            // Escolhe qual mapa usar com base no estado do Shift
            unsigned char letra;
            if (shifton) {
                letra = keyboard_map_shift[scancode];
            } else {
                letra = keyboard_map[scancode];
            }

            if (letra == '\n') {
                buffer[buffer_index] = '\0';
                pcmd(buffer);   
                buffer_index = 0;                  
            }
            else if (letra == '\b') {
                if (buffer_index > 0) {
                    buffer_index--;
                    buffer[buffer_index] = '\0'; // Limpa o caractere do buffer
                    print("\b");
                }
            }
            else if (letra != 0) {
                if (buffer_index < 254) {
                    buffer[buffer_index++] = letra;
                    char str[2] = {letra, '\0'};
                    print(str);
                }
            }
        }
    }
    outb(0x20, 0x20); 
}

sys_var lista_vars[] = {
    {"ver", versao},
    {"os", "MyceliumOS"},
    {"cursor", cursorstr}
};

int num_vars = sizeof(lista_vars) / sizeof(sys_var);

int main() {
    extern void timer_wrapper();
    extern void keyboard_wrapper();
    idt_install();     
    reprogramar_pic();
    idt_set_gate(32, (uint32_t)(unsigned long)timer_wrapper, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)(unsigned long)keyboard_wrapper, 0x08, 0x8E);
    cpit(100);
    __asm__ volatile("sti");
    som(293 * 2);
    sleep(10);
    som(440 * 2);
    sleep(10);
    som(349 * 2);
    sleep(10);
    som(261 * 2);
    sleep(20);
    parar_som();
    cursor_on();
    
    limpatela();
    cursorpos = 0;
    cor(branco);
    print("MyceliumOS Terminal ");
    print(versao);
    cmd_fetch(0);
    print("\nMyceliumOS> ");
    buffer_index = 0;
    buffer[0] = '\0';
    while(1) {
        itoa(cursorpos, cursorstr);
        __asm__ volatile("hlt");
    }
}