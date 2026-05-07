#include "kbd.h"
#include <stdint.h>
#include "lib.h"
#include "cmd.h"
#include "vga.h"
#include "som.h"
#include "usb_kbd.h"

/* Magic number do Multiboot2 que o bootloader coloca em EAX */
#define MULTIBOOT2_MAGIC 0x36d76289
#define MULTIBOOT_TAG_TYPE_CMDLINE 1

char* boot_cmdline = "";
char* boot_video_request = "auto";
char* boot_video_mode = "unknown";
char* boot_input_mode = "auto";
int boot_safe_mode = 0;
int boot_sound_enabled = 1;
int boot_usb_debug = 0;

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) multiboot_tag_t;

static char* multiboot_cmdline(uint32_t mb_info) {
    if (mb_info == 0) return 0;

    multiboot_info_t *info = (multiboot_info_t*)mb_info;
    uint32_t offset = 8;

    while (offset + sizeof(multiboot_tag_t) <= info->total_size) {
        multiboot_tag_t *tag = (multiboot_tag_t*)(mb_info + offset);
        if (tag->type == 0 || tag->size == 0) break;

        if (tag->type == MULTIBOOT_TAG_TYPE_CMDLINE) {
            return (char*)((uint32_t)tag + sizeof(multiboot_tag_t));
        }

        offset += (tag->size + 7) & ~7;
    }

    return 0;
}

static int cmdline_has(char *cmdline, const char *needle) {
    if (!cmdline || !needle) return 0;

    for (int i = 0; cmdline[i] != '\0'; i++) {
        int j = 0;
        while (needle[j] != '\0' && cmdline[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            char before = (i == 0) ? ' ' : cmdline[i - 1];
            char after = cmdline[i + j];
            if ((before == ' ' || before == '\t') &&
                (after == '\0' || after == ' ' || after == '\t')) {
                return 1;
            }
        }
    }

    return 0;
}

int kernel_main(uint32_t magic, uint32_t mb_info) {
    extern void timer_wrapper();
    extern void keyboard_wrapper();
    extern void mouse_wrapper();

    char *cmdline = multiboot_cmdline(mb_info);
    int boot_action = 0;
    int force_text_video = cmdline_has(cmdline, "video=text") ||
                           cmdline_has(cmdline, "text-fallback");
    int force_fb_video = cmdline_has(cmdline, "video=fb") ||
                         cmdline_has(cmdline, "video=framebuffer");
    int usb_native_input = 0;
    int usb_legacy_input = 0;
    int auto_input = 0;
    int auto_usb_selected = 0;
    int usb_fallback_done = 0;

    boot_cmdline = cmdline ? cmdline : "";
    boot_safe_mode = cmdline_has(cmdline, "safe-mode") ||
                     cmdline_has(cmdline, "safe");
    if (boot_safe_mode) {
        force_text_video = 1;
        force_fb_video = 0;
        boot_sound_enabled = 0;
    } else {
        boot_sound_enabled = !cmdline_has(cmdline, "nosound");
    }

    if (!boot_safe_mode) {
        usb_native_input = cmdline_has(cmdline, "input=usb-native") ||
                           cmdline_has(cmdline, "input=usb");
        usb_legacy_input = cmdline_has(cmdline, "input=legacy");
    } else {
        usb_legacy_input = 1;
    }
    boot_usb_debug = cmdline_has(cmdline, "usbdbg") ||
                     cmdline_has(cmdline, "usbdbg=1") ||
                     cmdline_has(cmdline, "usb-kbd-debug");
    usb_kbd_set_debug(boot_usb_debug);
    usb_kbd_trace_enabled = cmdline_has(cmdline, "usbtrace") ||
                            cmdline_has(cmdline, "usbdbg=trace");
    auto_input = !usb_native_input && !usb_legacy_input && !boot_safe_mode;

    if (force_text_video) boot_video_request = "text";
    else if (force_fb_video) boot_video_request = "framebuffer";
    else boot_video_request = "auto";

    if (cmdline_has(cmdline, "action=reboot")) {
        boot_action = 1;
    }
    if (cmdline_has(cmdline, "action=poweroff")) {
        boot_action = 2;
    }

    fpu_init();
    if (force_text_video) {
        vga_init_text_fallback(mb_info);
    } else {
        vga_init_from_multiboot(mb_info);
        if (force_fb_video && !usar_framebuffer) {
            panic("video=fb solicitado, mas nenhum framebuffer foi entregue pelo bootloader");
        }
    }
    boot_video_mode = vga_mode_name();
    cursor_on();
    init_vars();
    limpatela();

    cursor_x = 0;
    cursor_y = 0;
    cor(BRANCO);
    print("Inicializando MyceliumOS...\n");

    if (boot_action == 1) {
        print_info("Opcao do bootloader: reiniciar.\n");
        system_reboot();
    }
    if (boot_action == 2) {
        print_info("Opcao do bootloader: desligar.\n");
        system_poweroff();
    }

    /* Avisa se não foi carregado por um bootloader Multiboot2 válido */
    if (magic != MULTIBOOT2_MAGIC) {
        panic("Magic Multiboot2 invalido; bootloader nao entregou o kernel corretamente");
    }

    if (usb_native_input) {
        teclado_set_ps2_enabled(0);
    } else if (usb_legacy_input) {
        teclado_init_legacy();
    } else {
        teclado_init();
    }
    idt_install();
    reprogramar_pic();
    idt_set_gate(32, (uint32_t)(unsigned long)timer_wrapper,   0x08, 0x8E);
    idt_set_gate(33, (uint32_t)(unsigned long)keyboard_wrapper, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)(unsigned long)mouse_wrapper,    0x08, 0x8E);
    cpit(100);
    __asm__ volatile("sti");

    if (boot_sound_enabled) {
        som(293 * 2);
        sleep(10);
        som(440 * 2);
        sleep(10);
        som(349 * 2);
        sleep(10);
        som(261 * 2);
        sleep(20);
        parar_som();
    }

    limpatela();

    cmd_fetch(0, 0);
    if (usb_native_input) {
        boot_input_mode = "usb-native";
        usb_kbd_init();
        if (usb_kbd_has_controller()) {
            print_info("Entrada PS/2 desativada; usando teclado USB nativo.\n");
        } else {
            teclado_init_legacy();
            boot_input_mode = "legacy-fallback";
            USBDBG_ERROR("USB nativo indisponivel; fallback para PS/2/USB legacy ativo.\n");
        }
    } else if (auto_input) {
        boot_input_mode = "auto";
        USBDBG_INFO("Entrada automatica: tentando xHCI com fallback PS/2/USB legacy.\n");
        usb_kbd_init();
        if (!usb_kbd_has_controller()) {
            boot_input_mode = "legacy";
            USBDBG_INFO("Entrada automatica: xHCI indisponivel; usando PS/2/USB legacy.\n");
        }
    } else {
        boot_input_mode = "legacy";
        USBDBG_INFO("Entrada PS/2/USB legacy ativa.\n");
    }
    prompt();
    buffer_index = 0;
    buffer[0] = '\0';

    while (1) {
        usb_kbd_poll();
        if (!usb_fallback_done && usb_kbd_has_failed()) {
            teclado_init_legacy();
            usb_fallback_done = 1;
            boot_input_mode = "legacy-fallback";
            if (usb_kbd_debug_enabled) {
                USBDBG_ERROR("Entrada USB nativa falhou; usando PS/2/USB legacy.\n");
                prompt();
            }
        }
        if (auto_input && !auto_usb_selected && usb_kbd_is_ready()) {
            teclado_set_ps2_enabled(0);
            auto_usb_selected = 1;
            boot_input_mode = "usb-native";
            if (usb_kbd_debug_enabled) {
                USBDBG_INFO("\nEntrada automatica: teclado USB nativo selecionado.\n");
                prompt();
            }
        }
        teclado_poll();
        if (usb_kbd_is_ready()) {
            __asm__ volatile("pause");
        } else {
            __asm__ volatile("hlt");
        }
    }

    return 0;
}
