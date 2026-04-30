#include "kbd.h"
#include <stdint.h>
#include "lib.h"
#include "cmd.h"
#include "vga.h"
#include "som.h"

int main() {
    extern void timer_wrapper();
    extern void keyboard_wrapper();
    idt_install();     
    reprogramar_pic();
    idt_set_gate(32, (uint32_t)(unsigned long)timer_wrapper, 0x08, 0x8E); // 0x08 é o CS
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

    init_vars();
    
    limpatela();
    set_cursor_pos(0, 0);
    cmd_fetch(0, 0);
    prompt();
    buffer_index = 0;
    buffer[0] = '\0';

    while(1) {
        __asm__ volatile("hlt");
    }
}