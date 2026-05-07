#ifndef USB_KBD_H
#define USB_KBD_H

#include <stdint.h>

#define USBDBG_INFO(msg) do { if (usb_kbd_debug_enabled && usb_kbd_trace_enabled) print_info(msg); } while (0)
#define USBDBG_ERROR(msg) do { if (usb_kbd_debug_enabled) print_error(msg); } while (0)
#define USBDBG_PRINT(msg) do { if (usb_kbd_debug_enabled && usb_kbd_trace_enabled) print(msg); } while (0)
#define USBDBG_HEX(value) do { if (usb_kbd_debug_enabled && usb_kbd_trace_enabled) print_hex(value); } while (0)
#define USBDBG_HEX_BYTE(value) do { if (usb_kbd_debug_enabled && usb_kbd_trace_enabled) print_hex_byte(value); } while (0)

void usb_kbd_init(void);
void usb_kbd_poll(void);
void usb_kbd_feed_boot_report(const uint8_t report[8]);
void usb_kbd_set_debug(int enabled);
int usb_kbd_has_controller(void);
int usb_kbd_is_ready(void);
int usb_kbd_has_failed(void);
extern int usb_kbd_debug_enabled;
extern int usb_kbd_trace_enabled;

#endif
