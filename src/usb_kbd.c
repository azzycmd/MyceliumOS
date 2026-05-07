#include "usb_kbd.h"
#include "io.h"
#include "lib.h"
#include "kbd.h"
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_COMMAND        0x04
#define PCI_BAR0           0x10
#define PCI_COMMAND_MEMORY 0x00000002
#define PCI_COMMAND_MASTER 0x00000004

/*
 * vga.c usa 0xE0000000 como endereco virtual do framebuffer quando
 * ativa paging. Se o xHCI tambem for realocado para la, o MMIO pode
 * acabar apontando para o framebuffer em vez do controlador.
 *
 * Enquanto nao existir um VMM geral, mantenha o BAR na PCI hole e use
 * vga_map_mmio_region() para expor essa faixa quando paging estiver ativo.
 */
#define XHCI_LOW_MMIO_BASE 0xF0000000U
#define XHCI_LOW_MMIO_LIMIT 0xF1000000U
#define XHCI_LOW_MMIO_MIN 0x80000000U

#define XHCI_CMD_RING_SIZE 64
#define XHCI_EP_RING_SIZE 64
#define XHCI_EVENT_RING_SIZE 256
#define XHCI_CONTEXT_BYTES 64
#define XHCI_MAX_SCRATCHPADS 64
#define XHCI_PAGE_BYTES 4096

#define XHCI_USBCMD        0x00
#define XHCI_USBSTS        0x04
#define XHCI_PAGESIZE      0x08
#define XHCI_CRCR          0x18
#define XHCI_DCBAAP        0x30
#define XHCI_CONFIG        0x38
#define XHCI_PORTSC_BASE   0x400

#define XHCI_HCCPARAMS1    0x10
#define XHCI_HCSPARAMS2    0x08

#define XHCI_EXT_CAP_LEGACY 0x01
#define XHCI_LEGACY_BIOS_OWNED (1U << 16)
#define XHCI_LEGACY_OS_OWNED   (1U << 24)

#define XHCI_USBCMD_RS     0x00000001
#define XHCI_USBCMD_HCRST  0x00000002
#define XHCI_USBCMD_INTE   0x00000004
#define XHCI_USBSTS_HCH    0x00000001
#define XHCI_USBSTS_CNR    0x00000800

#define XHCI_PORTSC_CCS    0x00000001
#define XHCI_PORTSC_PED    0x00000002
#define XHCI_PORTSC_PR     0x00000010
#define XHCI_PORTSC_PLS_MASK 0x000001E0
#define XHCI_PORTSC_PLS_SHIFT 5
#define XHCI_PORTSC_PP     0x00000200
#define XHCI_PORTSC_RW_PRESERVE (XHCI_PORTSC_PP)
#define XHCI_PORTSC_SPEED_MASK 0x00003C00
#define XHCI_PORTSC_SPEED_SHIFT 10
#define XHCI_PORTSC_CHANGE_BITS 0x00FE0000

#define XHCI_INTR_IMAN     0x00
#define XHCI_INTR_ERSTSZ   0x08
#define XHCI_INTR_ERSTBA   0x10
#define XHCI_INTR_ERDP     0x18

#define XHCI_TRB_ENABLE_SLOT          9
#define XHCI_TRB_DISABLE_SLOT         10
#define XHCI_TRB_NORMAL               1
#define XHCI_TRB_SETUP_STAGE          2
#define XHCI_TRB_DATA_STAGE           3
#define XHCI_TRB_STATUS_STAGE         4
#define XHCI_TRB_LINK                 6
#define XHCI_TRB_CONFIGURE_ENDPOINT   12
#define XHCI_TRB_TRANSFER_EVENT       32
#define XHCI_TRB_COMMAND_COMPLETION   33
#define XHCI_TRB_PORT_STATUS_CHANGE   34

#define XHCI_CC_SUCCESS               1
#define XHCI_CC_SHORT_PACKET          13

#define XHCI_EP_TYPE_CONTROL          4
#define XHCI_EP_TYPE_INTERRUPT_IN     7

#define XHCI_TRB_IOC                  (1U << 5)
#define XHCI_TRB_IDT                  (1U << 6)
#define XHCI_TRB_DIR_IN               (1U << 16)
#define XHCI_TRB_TRT_IN               (3U << 16)
#define XHCI_TRB_TRT_OUT              (2U << 16)
#define XHCI_TRB_LINK_TC              (1U << 1)

enum {
    XHCI_ENUM_IDLE = 0,
    XHCI_ENUM_RESET_SENT,
    XHCI_ENUM_ENABLE_SLOT_SENT,
    XHCI_ENUM_ADDRESS_BSR_SENT,
    XHCI_ENUM_ADDRESS_SENT,
    XHCI_ENUM_GET_DEVICE8_SENT,
    XHCI_ENUM_GET_DEVICE_SENT,
    XHCI_ENUM_GET_CONFIG8_SENT,
    XHCI_ENUM_GET_CONFIG_FULL_SENT,
    XHCI_ENUM_SET_CONFIG_SENT,
    XHCI_ENUM_SET_PROTOCOL_SENT,
    XHCI_ENUM_CONFIGURE_ENDPOINT_SENT,
    XHCI_ENUM_READY
};

enum {
    XHCI_PENDING_NONE = 0,
    XHCI_PENDING_ENABLE_SLOT,
    XHCI_PENDING_DISABLE_SLOT,
    XHCI_PENDING_ADDRESS_DEVICE_BSR,
    XHCI_PENDING_ADDRESS_DEVICE,
    XHCI_PENDING_CONFIGURE_ENDPOINT
};

typedef struct {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
    uint64_t base;
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed)) xhci_erst_entry_t;

static int controller_present = 0;
static int transport_ready = 0;
static int transport_failed = 0;
int usb_kbd_debug_enabled = 0;
int usb_kbd_trace_enabled = 0;
static uint32_t xhci_bar0 = 0;
static uint32_t xhci_bar_size = 0;
static uint32_t op_base = 0;
static uint32_t db_base = 0;
static uint32_t runtime_base = 0;
static uint32_t interrupter0 = 0;
static uint32_t context_size = 32;
static uint32_t hcsparams2_raw = 0;
static uint16_t scratchpad_count = 0;
static uint8_t max_ports = 0;
static uint8_t max_slots = 0;

static uint8_t cmd_cycle = 1;
static uint8_t event_cycle = 1;
static uint8_t ep0_cycle = 1;
static uint8_t intr_cycle = 1;
static uint32_t cmd_enqueue = 0;
static uint32_t ep0_enqueue = 0;
static uint32_t intr_enqueue = 0;
static uint32_t event_dequeue = 0;
static uint8_t enabled_slot = 0;
static uint8_t selected_port = 0;
static uint8_t selected_speed = 0;
static uint8_t enum_state = XHCI_ENUM_IDLE;
static uint8_t pending_command = XHCI_PENDING_NONE;
static uint8_t retry_after_disable_slot = 0;
static uint8_t address_retry_count = 0;
static uint8_t transfer_retry_count = 0;
static uint32_t ep0_mps_override = 0;
static uint8_t kbd_interface = 0;
static uint8_t kbd_config_value = 0;
static uint8_t kbd_ep_addr = 0;
static uint8_t kbd_ep_dci = 0;
static uint8_t kbd_ep_interval = 10;
static uint16_t kbd_ep_max_packet = 8;
static xhci_trb_t *ep0_wait_trb = 0;
static xhci_trb_t *intr_wait_trb = 0;
static uint8_t last_report[8] = {0};
static uint8_t last_modifiers = 0;

static uint64_t dcbaa[256] __attribute__((aligned(4096)));
static uint64_t scratchpad_ptrs[XHCI_MAX_SCRATCHPADS] __attribute__((aligned(4096)));
static uint8_t scratchpad_buffers[XHCI_MAX_SCRATCHPADS][XHCI_PAGE_BYTES] __attribute__((aligned(4096)));
static xhci_trb_t command_ring[XHCI_CMD_RING_SIZE] __attribute__((aligned(4096)));
static xhci_trb_t event_ring[XHCI_EVENT_RING_SIZE] __attribute__((aligned(4096)));
static xhci_trb_t ep0_ring[XHCI_EP_RING_SIZE] __attribute__((aligned(4096)));
static xhci_trb_t intr_ring[XHCI_EP_RING_SIZE] __attribute__((aligned(4096)));
static xhci_erst_entry_t erst[1] __attribute__((aligned(4096)));
static uint8_t input_context[34 * XHCI_CONTEXT_BYTES] __attribute__((aligned(4096)));
static uint8_t device_context[33 * XHCI_CONTEXT_BYTES] __attribute__((aligned(4096)));
static uint8_t device_desc[18] __attribute__((aligned(64)));
static uint8_t config_desc[256] __attribute__((aligned(64)));
static uint8_t keyboard_report[8] __attribute__((aligned(64)));

void usb_kbd_set_debug(int enabled) {
    usb_kbd_debug_enabled = enabled ? 1 : 0;
}

static void usb_error_print(char *msg) {
    if (usb_kbd_debug_enabled) print(msg);
}

static void usb_error_hex(uint32_t value) {
    if (usb_kbd_debug_enabled) print_hex(value);
}

static uint32_t mmio_read32(uint32_t addr) {
    return *(volatile uint32_t*)(uintptr_t)addr;
}

static void mmio_write32(uint32_t addr, uint32_t value) {
    *(volatile uint32_t*)(uintptr_t)addr = value;
}

static void mmio_write64(uint32_t addr, uint64_t value) {
    mmio_write32(addr, (uint32_t)(value & 0xFFFFFFFF));
    mmio_write32(addr + 4, (uint32_t)(value >> 32));
}

static void zero_bytes(void *ptr, uint32_t bytes) {
    uint8_t *dst8 = (uint8_t*)ptr;
    uint32_t dwords = bytes / 4;
    uint32_t tail = bytes & 3;

    __asm__ volatile(
        "cld\n"
        "rep stosl"
        : "+D"(dst8), "+c"(dwords)
        : "a"(0)
        : "memory"
    );

    for (uint32_t i = 0; i < tail; i++) {
        dst8[i] = 0;
    }
}

static uint32_t pci_addr(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (uint32_t)(1U << 31) |
           ((uint32_t)bus << 16) |
           ((uint32_t)device << 11) |
           ((uint32_t)function << 8) |
           (offset & 0xFC);
}

static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_addr(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

static void pci_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_addr(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

static uint32_t align_down(uint32_t value, uint32_t alignment) {
    if (alignment == 0) return value;
    return value & ~(alignment - 1);
}

static uint32_t detect_bar_size(uint8_t bus, uint8_t dev, uint8_t func, uint16_t command) {
    uint32_t old_bar0 = pci_read32(bus, dev, func, PCI_BAR0);
    uint32_t old_bar1 = pci_read32(bus, dev, func, PCI_BAR0 + 4);

    pci_write32(bus, dev, func, PCI_COMMAND, command & ~PCI_COMMAND_MEMORY);
    pci_write32(bus, dev, func, PCI_BAR0, 0xFFFFFFFF);
    pci_write32(bus, dev, func, PCI_BAR0 + 4, 0xFFFFFFFF);

    uint32_t mask0 = pci_read32(bus, dev, func, PCI_BAR0);
    uint32_t mask1 = pci_read32(bus, dev, func, PCI_BAR0 + 4);

    pci_write32(bus, dev, func, PCI_BAR0, old_bar0);
    pci_write32(bus, dev, func, PCI_BAR0 + 4, old_bar1);
    pci_write32(bus, dev, func, PCI_COMMAND, command);

    uint64_t mask = ((uint64_t)mask1 << 32) | (uint64_t)(mask0 & 0xFFFFFFF0);
    uint64_t size = (~mask) + 1;
    if (size == 0 || size > 0x10000000ULL) return 0;
    return (uint32_t)size;
}

static int relocate_bar_below_4g(uint8_t bus, uint8_t dev, uint8_t func, uint32_t *bar0, uint32_t *bar1) {
    uint16_t command = (uint16_t)(pci_read32(bus, dev, func, PCI_COMMAND) & 0xFFFF);
    uint32_t size = detect_bar_size(bus, dev, func, command);
    if (size == 0) {
        USBDBG_ERROR("USBDBG: could not detect BAR size\n");
        return 0;
    }

    uint32_t base = align_down(XHCI_LOW_MMIO_BASE, size);
    if (base < XHCI_LOW_MMIO_MIN || base + size > XHCI_LOW_MMIO_LIMIT || base + size < base) {
        USBDBG_ERROR("USBDBG: relocation range invalid base=");
        USBDBG_HEX(base);
        USBDBG_PRINT(" size=");
        USBDBG_HEX(size);
        USBDBG_PRINT("\n");
        return 0;
    }

    pci_write32(bus, dev, func, PCI_COMMAND, command & ~PCI_COMMAND_MEMORY);
    pci_write32(bus, dev, func, PCI_BAR0, base | (*bar0 & 0x0F));
    pci_write32(bus, dev, func, PCI_BAR0 + 4, 0);
    pci_write32(bus, dev, func, PCI_COMMAND, command | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

    *bar0 = pci_read32(bus, dev, func, PCI_BAR0);
    *bar1 = pci_read32(bus, dev, func, PCI_BAR0 + 4);
    xhci_bar_size = size;

    USBDBG_INFO("USBDBG: relocated BAR0=");
    USBDBG_HEX(*bar0);
    USBDBG_PRINT(" BAR1=");
    USBDBG_HEX(*bar1);
    USBDBG_PRINT(" size=");
    USBDBG_HEX(size);
    USBDBG_PRINT("\n");

    return 1;
}

static int wait_reg_clear(uint32_t addr, uint32_t mask) {
    for (uint32_t i = 0; i < 1000000; i++) {
        if ((mmio_read32(addr) & mask) == 0) return 1;
    }
    return 0;
}

static int wait_reg_set(uint32_t addr, uint32_t mask) {
    for (uint32_t i = 0; i < 1000000; i++) {
        if (mmio_read32(addr) & mask) return 1;
    }
    return 0;
}

static void xhci_wait_ticks(uint32_t ticks) {
    uint32_t start = timer_ticks;
    if (ticks == 0) return;

    while ((timer_ticks - start) < ticks) {
        __asm__ volatile("pause");
    }
}

static uint8_t hid_boot_usage_to_set1(uint8_t usage, int *extended) {
    *extended = 0;
    if (usage >= 0x04 && usage <= 0x1D) {
        static const uint8_t letters[] = {
            0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23,
            0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19,
            0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D,
            0x15, 0x2C
        };
        return letters[usage - 0x04];
    }
    if (usage >= 0x1E && usage <= 0x27) {
        static const uint8_t numbers[] = {
            0x02, 0x03, 0x04, 0x05, 0x06,
            0x07, 0x08, 0x09, 0x0A, 0x0B
        };
        return numbers[usage - 0x1E];
    }

    switch (usage) {
        case 0x28: return 0x1C;
        case 0x29: return 0x01;
        case 0x2A: return 0x0E;
        case 0x2B: return 0x0F;
        case 0x2C: return 0x39;
        case 0x2D: return 0x0C;
        case 0x2E: return 0x0D;
        case 0x2F: return 0x1A;
        case 0x30: return 0x1B;
        case 0x31: return 0x2B;
        case 0x33: return 0x27;
        case 0x34: return 0x28;
        case 0x35: return 0x29;
        case 0x36: return 0x33;
        case 0x37: return 0x34;
        case 0x38: return 0x35;
        case 0x4F: *extended = 1; return 0x4D;
        case 0x50: *extended = 1; return 0x4B;
        case 0x51: *extended = 1; return 0x50;
        case 0x52: *extended = 1; return 0x48;
    }

    return 0;
}

static void kbd_usb_emit_set1(uint8_t scancode, int extended, int release) {
    if (scancode == 0) return;
    if (extended) kbd_scancode_input(0xE0);
    kbd_scancode_input(release ? (scancode | 0x80) : scancode);
}

static void kbd_usb_update_modifier(uint8_t report_mods, uint8_t bit,
                                    uint8_t scancode, int extended) {
    int was_down = (last_modifiers & bit) != 0;
    int is_down = (report_mods & bit) != 0;
    if (was_down == is_down) return;
    kbd_usb_emit_set1(scancode, extended, !is_down);
}

static int report_has_usage(const uint8_t report[8], uint8_t usage) {
    for (int i = 2; i < 8; i++) {
        if (report[i] == usage) return 1;
    }
    return 0;
}

static void ring_command_doorbell(void) {
    mmio_write32(db_base, 0);
}

static void ring_endpoint_doorbell(uint8_t slot, uint8_t dci) {
    mmio_write32(db_base + ((uint32_t)slot * 4), dci);
}

static int xhci_push_command(uint64_t parameter, uint32_t status,
                             uint32_t control, uint8_t pending) {
    if (cmd_enqueue >= XHCI_CMD_RING_SIZE - 1) return 0;

    command_ring[cmd_enqueue].parameter = parameter;
    command_ring[cmd_enqueue].status = status;
    command_ring[cmd_enqueue].control = control | (cmd_cycle & 1);
    cmd_enqueue++;
    pending_command = pending;
    ring_command_doorbell();
    return 1;
}

static xhci_trb_t* ep_push(xhci_trb_t *ring, uint32_t *enqueue, uint8_t *cycle,
                           uint64_t parameter, uint32_t status, uint32_t control) {
    if (*enqueue >= XHCI_EP_RING_SIZE - 1) {
        ring[XHCI_EP_RING_SIZE - 1].control =
            (XHCI_TRB_LINK << 10) | XHCI_TRB_LINK_TC | (*cycle & 1);
        *enqueue = 0;
        *cycle ^= 1;
    }

    xhci_trb_t *trb = &ring[*enqueue];
    trb->parameter = parameter;
    trb->status = status;
    trb->control = control | (*cycle & 1);
    (*enqueue)++;
    return trb;
}

static void init_rings(void) {
    zero_bytes(dcbaa, sizeof(dcbaa));
    zero_bytes(command_ring, sizeof(command_ring));
    zero_bytes(event_ring, sizeof(event_ring));
    zero_bytes(ep0_ring, sizeof(ep0_ring));
    zero_bytes(intr_ring, sizeof(intr_ring));
    zero_bytes(input_context, sizeof(input_context));
    zero_bytes(device_context, sizeof(device_context));
    zero_bytes(device_desc, sizeof(device_desc));
    zero_bytes(config_desc, sizeof(config_desc));
    zero_bytes(keyboard_report, sizeof(keyboard_report));

    command_ring[XHCI_CMD_RING_SIZE - 1].parameter = (uint32_t)(uintptr_t)&command_ring[0];
    command_ring[XHCI_CMD_RING_SIZE - 1].status = 0;
    command_ring[XHCI_CMD_RING_SIZE - 1].control =
        (XHCI_TRB_LINK << 10) | XHCI_TRB_LINK_TC | (cmd_cycle & 1);
    ep0_ring[XHCI_EP_RING_SIZE - 1].parameter = (uint32_t)(uintptr_t)&ep0_ring[0];
    ep0_ring[XHCI_EP_RING_SIZE - 1].status = 0;
    ep0_ring[XHCI_EP_RING_SIZE - 1].control =
        (XHCI_TRB_LINK << 10) | XHCI_TRB_LINK_TC | (ep0_cycle & 1);
    intr_ring[XHCI_EP_RING_SIZE - 1].parameter = (uint32_t)(uintptr_t)&intr_ring[0];
    intr_ring[XHCI_EP_RING_SIZE - 1].status = 0;
    intr_ring[XHCI_EP_RING_SIZE - 1].control =
        (XHCI_TRB_LINK << 10) | XHCI_TRB_LINK_TC | (intr_cycle & 1);

    erst[0].base = (uint32_t)(uintptr_t)&event_ring[0];
    erst[0].size = XHCI_EVENT_RING_SIZE;
    erst[0].reserved = 0;

    cmd_enqueue = 0;
    ep0_enqueue = 0;
    intr_enqueue = 0;
    event_dequeue = 0;
    cmd_cycle = 1;
    ep0_cycle = 1;
    intr_cycle = 1;
    event_cycle = 1;
    ep0_wait_trb = 0;
    intr_wait_trb = 0;
}

static uint32_t* input_control_context(void) {
    return (uint32_t*)input_context;
}

static uint32_t* input_slot_context(void) {
    return (uint32_t*)(input_context + context_size);
}

static uint32_t* input_ep_context(uint8_t dci) {
    return (uint32_t*)(input_context + ((uint32_t)(dci + 1) * context_size));
}

static uint32_t ep0_max_packet(uint8_t speed) {
    if (ep0_mps_override != 0) return ep0_mps_override;
    if (speed == 2) return 8;
    if (speed == 3) return 64;
    if (speed >= 4) return 512;
    return 8;
}

static uint32_t next_ep0_mps_retry(uint8_t retry) {
    if (selected_speed == 2) return 0;
    if (retry == 1) return 64;
    if (retry == 2) return 32;
    if (retry == 3) return 16;
    return 0;
}

static uint8_t xhci_interrupt_interval(uint8_t speed, uint8_t endpoint_interval) {
    if (endpoint_interval == 0) endpoint_interval = 1;

    if (speed <= 3) {
        uint32_t microframes = (uint32_t)endpoint_interval * 8;
        uint8_t interval = 0;
        uint32_t value = 1;

        while (value < microframes && interval < 15) {
            value <<= 1;
            interval++;
        }

        if (interval < 3) interval = 3;
        return interval;
    }

    if (endpoint_interval > 16) endpoint_interval = 16;
    return endpoint_interval - 1;
}

static uint16_t scratchpads_from_hcsparams2(uint32_t hcsparams2) {
    uint16_t low = (uint16_t)((hcsparams2 >> 21) & 0x1F);
    uint16_t high = (uint16_t)((hcsparams2 >> 27) & 0x1F);
    return (uint16_t)((high << 5) | low);
}

static void take_ownership(uint32_t hccparams1) {
    uint32_t ext_offset = (hccparams1 >> 16) & 0xFFFF;
    if (ext_offset == 0) {
        USBDBG_INFO("USBDBG: no xHCI extended caps\n");
        return;
    }

    uint32_t cap = xhci_bar0 + (ext_offset * 4);
    for (int i = 0; i < 32 && cap != xhci_bar0; i++) {
        uint32_t header = mmio_read32(cap);
        uint8_t cap_id = (uint8_t)(header & 0xFF);
        uint8_t next = (uint8_t)((header >> 8) & 0xFF);

        USBDBG_INFO("USBDBG: extcap id=");
        USBDBG_HEX(cap_id);
        USBDBG_PRINT(" off=");
        USBDBG_HEX(cap - xhci_bar0);
        USBDBG_PRINT(" val=");
        USBDBG_HEX(header);
        USBDBG_PRINT("\n");

        if (cap_id == XHCI_EXT_CAP_LEGACY) {
            if (header & XHCI_LEGACY_BIOS_OWNED) {
                USBDBG_INFO("USBDBG: claiming xHCI from BIOS\n");
                mmio_write32(cap, header | XHCI_LEGACY_OS_OWNED);
                for (uint32_t wait = 0; wait < 1000000; wait++) {
                    header = mmio_read32(cap);
                    if ((header & XHCI_LEGACY_BIOS_OWNED) == 0 &&
                        (header & XHCI_LEGACY_OS_OWNED)) {
                        break;
                    }
                }
            } else {
                mmio_write32(cap, header | XHCI_LEGACY_OS_OWNED);
            }

            mmio_write32(cap + 4, 0);
            header = mmio_read32(cap);
            USBDBG_INFO("USBDBG: legacy ownership=");
            USBDBG_HEX(header);
            USBDBG_PRINT("\n");
        }

        if (next == 0) break;
        cap += (uint32_t)next * 4;
    }
}

static int setup_scratchpads(void) {
    if (scratchpad_count == 0) {
        dcbaa[0] = 0;
        USBDBG_INFO("USBDBG: scratchpads=0\n");
        return 1;
    }

    if (scratchpad_count > XHCI_MAX_SCRATCHPADS) {
        USBDBG_ERROR("USBDBG: too many scratchpads required count=");
        USBDBG_HEX(scratchpad_count);
        USBDBG_PRINT("\n");
        return 0;
    }

    for (uint16_t i = 0; i < scratchpad_count; i++) {
        scratchpad_ptrs[i] = (uint32_t)(uintptr_t)&scratchpad_buffers[i][0];
        zero_bytes(&scratchpad_buffers[i][0], XHCI_PAGE_BYTES);
    }

    dcbaa[0] = (uint32_t)(uintptr_t)&scratchpad_ptrs[0];
    USBDBG_INFO("USBDBG: scratchpads=");
    USBDBG_HEX(scratchpad_count);
    USBDBG_PRINT(" table=");
    USBDBG_HEX((uint32_t)(uintptr_t)&scratchpad_ptrs[0]);
    USBDBG_PRINT("\n");
    return 1;
}

static uint64_t usb_setup_packet(uint8_t request_type, uint8_t request,
                                 uint16_t value, uint16_t index, uint16_t length) {
    return (uint64_t)request_type |
           ((uint64_t)request << 8) |
           ((uint64_t)value << 16) |
           ((uint64_t)index << 32) |
           ((uint64_t)length << 48);
}

static void usb_control_in(uint8_t request_type, uint8_t request, uint16_t value,
                           uint16_t index, uint16_t length, uint8_t *buffer) {
    USBDBG_INFO("USBDBG: control in req=");
    USBDBG_HEX(request);
    USBDBG_PRINT(" value=");
    USBDBG_HEX(value);
    USBDBG_PRINT(" index=");
    USBDBG_HEX(index);
    USBDBG_PRINT(" len=");
    USBDBG_HEX(length);
    USBDBG_PRINT(" ep0_enq=");
    USBDBG_HEX(ep0_enqueue);
    USBDBG_PRINT("\n");
    ep_push(ep0_ring, &ep0_enqueue, &ep0_cycle,
            usb_setup_packet(request_type, request, value, index, length),
            8, (XHCI_TRB_SETUP_STAGE << 10) | XHCI_TRB_IDT | XHCI_TRB_TRT_IN);
    ep_push(ep0_ring, &ep0_enqueue, &ep0_cycle,
            (uint32_t)(uintptr_t)buffer,
            length, (XHCI_TRB_DATA_STAGE << 10) | XHCI_TRB_DIR_IN);
    ep0_wait_trb = ep_push(ep0_ring, &ep0_enqueue, &ep0_cycle,
                           0, 0, (XHCI_TRB_STATUS_STAGE << 10) | XHCI_TRB_IOC);
    ring_endpoint_doorbell(enabled_slot, 1);
}

static void usb_control_out(uint8_t request_type, uint8_t request, uint16_t value,
                            uint16_t index, uint16_t length, uint8_t *buffer) {
    USBDBG_INFO("USBDBG: control out req=");
    USBDBG_HEX(request);
    USBDBG_PRINT(" value=");
    USBDBG_HEX(value);
    USBDBG_PRINT(" index=");
    USBDBG_HEX(index);
    USBDBG_PRINT(" len=");
    USBDBG_HEX(length);
    USBDBG_PRINT(" ep0_enq=");
    USBDBG_HEX(ep0_enqueue);
    USBDBG_PRINT("\n");
    ep_push(ep0_ring, &ep0_enqueue, &ep0_cycle,
            usb_setup_packet(request_type, request, value, index, length),
            8, (XHCI_TRB_SETUP_STAGE << 10) | XHCI_TRB_IDT |
               (length ? XHCI_TRB_TRT_OUT : 0));
    if (length != 0 && buffer != 0) {
        ep_push(ep0_ring, &ep0_enqueue, &ep0_cycle,
                (uint32_t)(uintptr_t)buffer,
                length, XHCI_TRB_DATA_STAGE << 10);
    }
    ep0_wait_trb = ep_push(ep0_ring, &ep0_enqueue, &ep0_cycle,
                           0, 0, (XHCI_TRB_STATUS_STAGE << 10) |
                                 XHCI_TRB_DIR_IN | XHCI_TRB_IOC);
    ring_endpoint_doorbell(enabled_slot, 1);
}

static uint32_t port_addr(uint8_t port) {
    return op_base + XHCI_PORTSC_BASE + ((uint32_t)(port - 1) * 0x10);
}

static void clear_port_changes(uint8_t port, uint32_t portsc) {
    uint32_t changes = portsc & XHCI_PORTSC_CHANGE_BITS;
    if (changes != 0) {
        mmio_write32(port_addr(port), (portsc & XHCI_PORTSC_RW_PRESERVE) | changes);
    }
}

static void start_port_reset(uint8_t port) {
    uint32_t addr = port_addr(port);
    uint32_t portsc = mmio_read32(addr);
    clear_port_changes(port, portsc);
    portsc = mmio_read32(addr);
    selected_port = port;
    selected_speed = (uint8_t)((portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT);
    enum_state = XHCI_ENUM_RESET_SENT;
    USBDBG_INFO("USBDBG: reset port=");
    USBDBG_HEX(port);
    USBDBG_PRINT(" portsc=");
    USBDBG_HEX(portsc);
    USBDBG_PRINT(" speed=");
    USBDBG_HEX(selected_speed);
    USBDBG_PRINT("\n");
    mmio_write32(addr, (portsc & XHCI_PORTSC_RW_PRESERVE) | XHCI_PORTSC_PP | XHCI_PORTSC_PR);

    for (uint32_t wait = 0; wait < 10000000; wait++) {
        portsc = mmio_read32(addr);
        if ((portsc & XHCI_PORTSC_PR) == 0) {
            selected_speed = (uint8_t)((portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT);
            clear_port_changes(port, portsc);

            if (portsc & XHCI_PORTSC_PED) {
                xhci_wait_ticks(10);
                enum_state = XHCI_ENUM_ENABLE_SLOT_SENT;
                USBDBG_INFO("USBDBG: reset complete by poll portsc=");
                USBDBG_HEX(portsc);
                USBDBG_PRINT(" speed=");
                USBDBG_HEX(selected_speed);
                USBDBG_PRINT("\n");
                USBDBG_INFO("USBDBG: enable slot\n");
                xhci_push_command(0, 0, XHCI_TRB_ENABLE_SLOT << 10,
                                  XHCI_PENDING_ENABLE_SLOT);
            }
            return;
        }
    }
}

static void power_xhci_ports(void) {
    for (uint8_t port = 1; port <= max_ports; port++) {
        uint32_t addr = port_addr(port);
        uint32_t portsc = mmio_read32(addr);
        clear_port_changes(port, portsc);
        portsc = mmio_read32(addr);
        mmio_write32(addr, (portsc & XHCI_PORTSC_RW_PRESERVE) | XHCI_PORTSC_PP);
    }

    xhci_wait_ticks(10);
}

static void reset_first_connected_port(void) {
    power_xhci_ports();
    USBDBG_INFO("USBDBG: scanning ports max=");
    USBDBG_HEX(max_ports);
    USBDBG_PRINT("\n");
    for (uint8_t port = 1; port <= max_ports; port++) {
        uint32_t portsc = mmio_read32(port_addr(port));
        USBDBG_INFO("USBDBG: port=");
        USBDBG_HEX(port);
        USBDBG_PRINT(" portsc=");
        USBDBG_HEX(portsc);
        USBDBG_PRINT("\n");
        if (portsc & XHCI_PORTSC_CCS) {
            start_port_reset(port);
            return;
        }
    }
    USBDBG_ERROR("USBDBG: no connected xHCI port found\n");
    USBDBG_ERROR("USBDBG: port sample:");
    for (uint8_t port = 1; port <= max_ports && port <= 8; port++) {
        usb_error_print(" p");
        usb_error_hex(port);
        usb_error_print("=");
        usb_error_hex(mmio_read32(port_addr(port)));
    }
    usb_error_print("\n");
    transport_ready = 0;
    transport_failed = 1;
}

static void fail_transport(char *msg) {
    USBDBG_ERROR(msg);
    transport_ready = 0;
    transport_failed = 1;
    pending_command = XHCI_PENDING_NONE;
    enum_state = XHCI_ENUM_IDLE;
    enabled_slot = 0;
    retry_after_disable_slot = 0;
}

static void disable_slot_for_retry(void) {
    retry_after_disable_slot = 1;
    if (enabled_slot != 0) {
        xhci_push_command(0, 0,
                          (XHCI_TRB_DISABLE_SLOT << 10) |
                          ((uint32_t)enabled_slot << 24),
                          XHCI_PENDING_DISABLE_SLOT);
    } else {
        start_port_reset(selected_port);
    }
}

static int selected_port_enabled(void) {
    if (selected_port == 0) return 0;
    uint32_t portsc = mmio_read32(port_addr(selected_port));
    return (portsc & XHCI_PORTSC_PED) != 0;
}

static int wait_selected_port_enabled(void) {
    if (selected_port == 0) return 0;

    for (uint32_t i = 0; i < 100000; i++) {
        if (selected_port_enabled()) return 1;
        __asm__ volatile("pause");
    }

    return selected_port_enabled();
}

static void retry_selected_port(char *reason) {
    uint32_t portsc = selected_port ? mmio_read32(port_addr(selected_port)) : 0;

    if (selected_port == 0 || transfer_retry_count >= 3) {
        USBDBG_ERROR("USBDBG: transfer retry exhausted; USB native disabled\n");
        fail_transport(reason);
        return;
    }

    transfer_retry_count++;
    ep0_wait_trb = 0;
    intr_wait_trb = 0;

    USBDBG_ERROR(reason);
    USBDBG_ERROR("USBDBG: retrying selected port reset retry=");
    usb_error_hex(transfer_retry_count);
    usb_error_print(" port=");
    usb_error_hex(selected_port);
    usb_error_print(" portsc=");
    usb_error_hex(portsc);
    usb_error_print(" pls=");
    usb_error_hex((portsc & XHCI_PORTSC_PLS_MASK) >> XHCI_PORTSC_PLS_SHIFT);
    usb_error_print("\n");
    clear_port_changes(selected_port, portsc);
    disable_slot_for_retry();
}

static int transfer_code_ok(uint8_t code) {
    return code == XHCI_CC_SUCCESS || code == XHCI_CC_SHORT_PACKET;
}

static void print_transfer_failure_debug(uint8_t code, uint32_t trb_addr) {
    uint32_t portsc = selected_port ? mmio_read32(port_addr(selected_port)) : 0;
    USBDBG_ERROR("USBDBG: transfer failed code=");
    usb_error_hex(code);
    usb_error_print(" state=");
    usb_error_hex(enum_state);
    usb_error_print(" trb=");
    usb_error_hex(trb_addr);
    usb_error_print(" ep0_wait=");
    usb_error_hex((uint32_t)(uintptr_t)ep0_wait_trb);
    usb_error_print(" intr_wait=");
    usb_error_hex((uint32_t)(uintptr_t)intr_wait_trb);
    usb_error_print(" port=");
    usb_error_hex(selected_port);
    usb_error_print(" portsc=");
    usb_error_hex(portsc);
    usb_error_print("\n");
}

static void prepare_address_device(uint8_t slot, int bsr, int clear_output_context) {
    zero_bytes(input_context, sizeof(input_context));
    if (clear_output_context) {
        zero_bytes(device_context, sizeof(device_context));
    }
    zero_bytes(ep0_ring, sizeof(ep0_ring));
    ep0_enqueue = 0;
    ep0_cycle = 1;
    ep0_wait_trb = 0;
    ep0_ring[XHCI_EP_RING_SIZE - 1].parameter = (uint32_t)(uintptr_t)&ep0_ring[0];
    ep0_ring[XHCI_EP_RING_SIZE - 1].control =
        (XHCI_TRB_LINK << 10) | XHCI_TRB_LINK_TC | (ep0_cycle & 1);

    dcbaa[slot] = (uint32_t)(uintptr_t)&device_context[0];

    uint32_t *control = input_control_context();
    uint32_t *slot_ctx = input_slot_context();
    uint32_t *ep0_ctx = input_ep_context(1);
    uint32_t mps = ep0_max_packet(selected_speed);

    control[1] = 0x00000003;
    slot_ctx[0] = ((uint32_t)selected_speed << 20) | (1U << 27);
    slot_ctx[1] = (uint32_t)selected_port << 16;

    ep0_ctx[1] = (3U << 1) | (XHCI_EP_TYPE_CONTROL << 3) | (mps << 16);
    ep0_ctx[2] = ((uint32_t)(uintptr_t)&ep0_ring[0]) | 1;
    ep0_ctx[4] = 8;

    enum_state = bsr ? XHCI_ENUM_ADDRESS_BSR_SENT : XHCI_ENUM_ADDRESS_SENT;
    USBDBG_INFO("USBDBG: address device slot=");
    USBDBG_HEX(slot);
    USBDBG_PRINT(" port=");
    USBDBG_HEX(selected_port);
    USBDBG_PRINT(" speed=");
    USBDBG_HEX(selected_speed);
    USBDBG_PRINT(" ep0_mps=");
    USBDBG_HEX(mps);
    USBDBG_PRINT(" override=");
    USBDBG_HEX(ep0_mps_override);
    USBDBG_PRINT(" bsr=");
    USBDBG_HEX((uint32_t)bsr);
    USBDBG_PRINT(" clear_out=");
    USBDBG_HEX((uint32_t)clear_output_context);
    USBDBG_PRINT(" input=");
    USBDBG_HEX((uint32_t)(uintptr_t)&input_context[0]);
    USBDBG_PRINT(" output=");
    USBDBG_HEX((uint32_t)(uintptr_t)&device_context[0]);
    USBDBG_PRINT(" ep0_ring=");
    USBDBG_HEX((uint32_t)(uintptr_t)&ep0_ring[0]);
    USBDBG_PRINT("\n");
    USBDBG_INFO("USBDBG: slotctx0=");
    USBDBG_HEX(slot_ctx[0]);
    USBDBG_PRINT(" slotctx1=");
    USBDBG_HEX(slot_ctx[1]);
    USBDBG_PRINT(" ep0ctx1=");
    USBDBG_HEX(ep0_ctx[1]);
    USBDBG_PRINT(" ep0ctx2=");
    USBDBG_HEX(ep0_ctx[2]);
    USBDBG_PRINT("\n");
    xhci_push_command((uint32_t)(uintptr_t)&input_context[0], 0,
                      (11U << 10) | ((uint32_t)slot << 24) |
                      (bsr ? (1U << 9) : 0),
                      bsr ? XHCI_PENDING_ADDRESS_DEVICE_BSR :
                            XHCI_PENDING_ADDRESS_DEVICE);
}

static void print_address_failure_debug(uint8_t code) {
    uint32_t portsc = selected_port ? mmio_read32(port_addr(selected_port)) : 0;
    USBDBG_ERROR("USBDBG: address failed code=");
    usb_error_hex(code);
    if (code == 4) {
        usb_error_print("(usb transaction)");
    }
    usb_error_print(" port=");
    usb_error_hex(selected_port);
    usb_error_print(" portsc=");
    usb_error_hex(portsc);
    usb_error_print(" speed=");
    usb_error_hex(selected_speed);
    usb_error_print(" scratch=");
    usb_error_hex(scratchpad_count);
    usb_error_print(" hcs2=");
    usb_error_hex(hcsparams2_raw);
    usb_error_print("\n");
    USBDBG_ERROR("USBDBG: dcbaa0=");
    usb_error_hex((uint32_t)dcbaa[0]);
    usb_error_print(" dcbaa_slot=");
    usb_error_hex((uint32_t)dcbaa[enabled_slot]);
    usb_error_print(" retry=");
    usb_error_hex(address_retry_count);
    usb_error_print("\n");
    USBDBG_ERROR("USBDBG: input=");
    usb_error_hex((uint32_t)(uintptr_t)&input_context[0]);
    usb_error_print(" output=");
    usb_error_hex((uint32_t)(uintptr_t)&device_context[0]);
    usb_error_print(" ep0=");
    usb_error_hex((uint32_t)(uintptr_t)&ep0_ring[0]);
    usb_error_print(" cmd_enq=");
    usb_error_hex(cmd_enqueue);
    usb_error_print("\n");
}

static int parse_keyboard_endpoint(void) {
    uint16_t total = (uint16_t)config_desc[2] | ((uint16_t)config_desc[3] << 8);
    uint8_t current_interface = 0;
    int boot_keyboard_interface = 0;
    if (total > sizeof(config_desc)) total = sizeof(config_desc);

    kbd_config_value = config_desc[5];
    USBDBG_INFO("USBDBG: config total=");
    USBDBG_HEX(total);
    USBDBG_PRINT(" cfg=");
    USBDBG_HEX(kbd_config_value);
    USBDBG_PRINT("\n");

    kbd_ep_addr = 0;
    kbd_ep_dci = 0;
    kbd_ep_max_packet = 8;
    kbd_ep_interval = 10;

    for (uint16_t offset = 0; offset + 2 <= total;) {
        uint8_t len = config_desc[offset];
        uint8_t type = config_desc[offset + 1];
        USBDBG_INFO("USBDBG: desc off=");
        USBDBG_HEX(offset);
        USBDBG_PRINT(" type=");
        USBDBG_HEX(type);
        USBDBG_PRINT(" len=");
        USBDBG_HEX(len);
        USBDBG_PRINT("\n");
        if (len == 0 || offset + len > total) {
            USBDBG_ERROR("USBDBG: invalid descriptor walk\n");
            break;
        }

        if (type == 4 && len >= 9) {
            current_interface = config_desc[offset + 2];
            USBDBG_INFO("USBDBG: interface=");
            USBDBG_HEX(current_interface);
            USBDBG_PRINT(" class=");
            USBDBG_HEX(config_desc[offset + 5]);
            USBDBG_PRINT(" subclass=");
            USBDBG_HEX(config_desc[offset + 6]);
            USBDBG_PRINT(" proto=");
            USBDBG_HEX(config_desc[offset + 7]);
            USBDBG_PRINT("\n");
            boot_keyboard_interface =
                config_desc[offset + 5] == 0x03 &&
                config_desc[offset + 6] == 0x01 &&
                config_desc[offset + 7] == 0x01;
            if (boot_keyboard_interface) kbd_interface = current_interface;
        } else if (type == 5 && len >= 7 && boot_keyboard_interface) {
            uint8_t ep_addr = config_desc[offset + 2];
            uint8_t attr = config_desc[offset + 3] & 0x03;
            uint16_t ep_mps = (uint16_t)config_desc[offset + 4] |
                              ((uint16_t)config_desc[offset + 5] << 8);
            USBDBG_INFO("USBDBG: endpoint addr=");
            USBDBG_HEX(ep_addr);
            USBDBG_PRINT(" attr=");
            USBDBG_HEX(attr);
            USBDBG_PRINT(" mps=");
            USBDBG_HEX(ep_mps);
            USBDBG_PRINT(" interval=");
            USBDBG_HEX(config_desc[offset + 6]);
            USBDBG_PRINT("\n");
            if ((ep_addr & 0x80) && attr == 0x03) {
                uint8_t ep_num = ep_addr & 0x0F;
                kbd_ep_addr = ep_addr;
                kbd_ep_dci = (uint8_t)((ep_num * 2) + 1);
                kbd_ep_max_packet = ep_mps;
                kbd_ep_interval = config_desc[offset + 6];
                if (kbd_ep_max_packet == 0 || kbd_ep_max_packet > sizeof(keyboard_report)) {
                    kbd_ep_max_packet = sizeof(keyboard_report);
                }
                USBDBG_INFO("USBDBG: boot keyboard ep selected dci=");
                USBDBG_HEX(kbd_ep_dci);
                USBDBG_PRINT("\n");
                return 1;
            }
        }

        offset += len;
    }

    USBDBG_ERROR("USBDBG: boot keyboard endpoint not found\n");
    return 0;
}

static void prepare_configure_keyboard_endpoint(void) {
    zero_bytes(input_context, sizeof(input_context));
    zero_bytes(intr_ring, sizeof(intr_ring));
    intr_enqueue = 0;
    intr_cycle = 1;
    intr_ring[XHCI_EP_RING_SIZE - 1].parameter = (uint32_t)(uintptr_t)&intr_ring[0];
    intr_ring[XHCI_EP_RING_SIZE - 1].control =
        (XHCI_TRB_LINK << 10) | XHCI_TRB_LINK_TC | (intr_cycle & 1);

    uint32_t *control = input_control_context();
    uint32_t *slot_ctx = input_slot_context();
    uint32_t *ep_ctx = input_ep_context(kbd_ep_dci);
    uint8_t xhci_interval = xhci_interrupt_interval(selected_speed, kbd_ep_interval);

    control[0] = 0;
    control[1] = (1U << 0) | (1U << kbd_ep_dci);
    slot_ctx[0] = ((uint32_t)selected_speed << 20) | ((uint32_t)kbd_ep_dci << 27);
    slot_ctx[1] = (uint32_t)selected_port << 16;

    ep_ctx[0] = (uint32_t)xhci_interval << 16;
    ep_ctx[1] = (3U << 1) | (XHCI_EP_TYPE_INTERRUPT_IN << 3) |
                ((uint32_t)kbd_ep_max_packet << 16);
    ep_ctx[2] = ((uint32_t)(uintptr_t)&intr_ring[0]) | 1;
    ep_ctx[4] = kbd_ep_max_packet;

    enum_state = XHCI_ENUM_CONFIGURE_ENDPOINT_SENT;
    USBDBG_INFO("USBDBG: configure interrupt in ep=");
    USBDBG_HEX(kbd_ep_addr);
    USBDBG_PRINT(" dci=");
    USBDBG_HEX(kbd_ep_dci);
    USBDBG_PRINT(" interval=");
    USBDBG_HEX(kbd_ep_interval);
    USBDBG_PRINT(" xhci_interval=");
    USBDBG_HEX(xhci_interval);
    USBDBG_PRINT(" mps=");
    USBDBG_HEX(kbd_ep_max_packet);
    USBDBG_PRINT("\n");
    xhci_push_command((uint32_t)(uintptr_t)&input_context[0], 0,
                      (XHCI_TRB_CONFIGURE_ENDPOINT << 10) |
                      ((uint32_t)enabled_slot << 24),
                      XHCI_PENDING_CONFIGURE_ENDPOINT);
}

static void queue_keyboard_report(void) {
    USBDBG_INFO("USBDBG: queue keyboard report\n");
    zero_bytes(keyboard_report, sizeof(keyboard_report));
    intr_wait_trb = ep_push(intr_ring, &intr_enqueue, &intr_cycle,
                            (uint32_t)(uintptr_t)&keyboard_report[0],
                            kbd_ep_max_packet,
                            (XHCI_TRB_NORMAL << 10) | XHCI_TRB_IOC);
    ring_endpoint_doorbell(enabled_slot, kbd_ep_dci);
}

static int find_controller(uint8_t *out_bus, uint8_t *out_dev, uint8_t *out_func) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_read32((uint8_t)bus, dev, func, 0x00);
                uint32_t class_reg = pci_read32((uint8_t)bus, dev, func, 0x08);
                if ((vendor_device & 0xFFFF) == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                if (((class_reg >> 24) == 0x0C) &&
                    (((class_reg >> 16) & 0xFF) == 0x03) &&
                    (((class_reg >> 8) & 0xFF) == 0x30)) {
                    *out_bus = (uint8_t)bus;
                    *out_dev = dev;
                    *out_func = func;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void usb_kbd_feed_boot_report(const uint8_t report[8]) {
    kbd_usb_update_modifier(report[0], 0x01, 0x1D, 0);
    kbd_usb_update_modifier(report[0], 0x02, 0x2A, 0);
    kbd_usb_update_modifier(report[0], 0x04, 0x38, 0);
    kbd_usb_update_modifier(report[0], 0x10, 0x1D, 0);
    kbd_usb_update_modifier(report[0], 0x20, 0x36, 0);
    kbd_usb_update_modifier(report[0], 0x40, 0x38, 0);

    for (int i = 2; i < 8; i++) {
        uint8_t usage = last_report[i];
        if (usage == 0) continue;

        if (!report_has_usage(report, usage)) {
            int extended = 0;
            uint8_t scancode = hid_boot_usage_to_set1(usage, &extended);
            kbd_usb_emit_set1(scancode, extended, 1);
        }
    }

    for (int i = 2; i < 8; i++) {
        uint8_t usage = report[i];
        if (usage == 0) continue;

        if (!report_has_usage(last_report, usage)) {
            int extended = 0;
            uint8_t scancode = hid_boot_usage_to_set1(usage, &extended);
            kbd_usb_emit_set1(scancode, extended, 0);
        }
    }
    last_modifiers = report[0];
    for (int i = 0; i < 8; i++) last_report[i] = report[i];
}

void usb_kbd_init(void) {
    uint8_t bus = 0, dev = 0, func = 0;
    controller_present = 0;
    transport_ready = 0;
    transport_failed = 0;
    enabled_slot = 0;
    selected_port = 0;
    selected_speed = 0;
    enum_state = XHCI_ENUM_IDLE;
    pending_command = XHCI_PENDING_NONE;
    retry_after_disable_slot = 0;
    address_retry_count = 0;
    transfer_retry_count = 0;
    ep0_mps_override = 0;
    last_modifiers = 0;
    for (int i = 0; i < 8; i++) last_report[i] = 0;

    USBDBG_INFO("USBDBG: init xHCI\n");
    if (!find_controller(&bus, &dev, &func)) {
        USBDBG_ERROR("USBDBG: xHCI PCI controller not found\n");
        return;
    }
    USBDBG_INFO("USBDBG: xHCI PCI bus=");
    USBDBG_HEX(bus);
    USBDBG_PRINT(" dev=");
    USBDBG_HEX(dev);
    USBDBG_PRINT(" func=");
    USBDBG_HEX(func);
    USBDBG_PRINT("\n");

    uint32_t command = pci_read32(bus, dev, func, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    pci_write32(bus, dev, func, PCI_COMMAND, command);

    uint32_t bar0 = pci_read32(bus, dev, func, PCI_BAR0);
    uint32_t bar1 = pci_read32(bus, dev, func, PCI_BAR0 + 4);
    USBDBG_INFO("USBDBG: BAR0=");
    USBDBG_HEX(bar0);
    USBDBG_PRINT(" BAR1=");
    USBDBG_HEX(bar1);
    USBDBG_PRINT("\n");
    if ((bar0 & 0x04) && bar1 != 0) {
        USBDBG_INFO("USBDBG: BAR above 4G, relocating\n");
        if (!relocate_bar_below_4g(bus, dev, func, &bar0, &bar1)) {
            USBDBG_ERROR("USBDBG: BAR relocation failed\n");
            return;
        }
    }

    if ((bar0 & 0x04) && bar1 != 0) {
        USBDBG_ERROR("USBDBG: BAR still above 4G after relocation\n");
        return;
    }

    xhci_bar0 = bar0 & 0xFFFFFFF0;
    if (xhci_bar0 == 0) {
        USBDBG_ERROR("USBDBG: invalid xHCI BAR\n");
        return;
    }

    if (!vga_map_mmio_region((uint64_t)xhci_bar0,
                             xhci_bar_size ? xhci_bar_size : 0x10000,
                             xhci_bar0)) {
        USBDBG_ERROR("USBDBG: could not map xHCI MMIO\n");
        return;
    }

    uint8_t cap_length = *(volatile uint8_t*)(uintptr_t)xhci_bar0;
    uint32_t hcsparams1 = mmio_read32(xhci_bar0 + 0x04);
    uint32_t hcsparams2 = mmio_read32(xhci_bar0 + XHCI_HCSPARAMS2);
    uint32_t hccparams1 = mmio_read32(xhci_bar0 + XHCI_HCCPARAMS1);
    uint32_t dboff = mmio_read32(xhci_bar0 + 0x14);
    uint32_t rtsoff = mmio_read32(xhci_bar0 + 0x18);

    max_slots = (uint8_t)(hcsparams1 & 0xFF);
    max_ports = (uint8_t)((hcsparams1 >> 24) & 0xFF);
    hcsparams2_raw = hcsparams2;
    scratchpad_count = scratchpads_from_hcsparams2(hcsparams2);
    context_size = (hccparams1 & 0x04) ? 64 : 32;
    op_base = xhci_bar0 + cap_length;
    db_base = xhci_bar0 + (dboff & ~0x3);
    runtime_base = xhci_bar0 + (rtsoff & ~0x1F);
    interrupter0 = runtime_base + 0x20;
    USBDBG_INFO("USBDBG: mmio=");
    USBDBG_HEX(xhci_bar0);
    USBDBG_PRINT(" slots=");
    USBDBG_HEX(max_slots);
    USBDBG_PRINT(" ports=");
    USBDBG_HEX(max_ports);
    USBDBG_PRINT(" ctx=");
    USBDBG_HEX(context_size);
    USBDBG_PRINT(" scratch=");
    USBDBG_HEX(scratchpad_count);
    USBDBG_PRINT("\n");
    take_ownership(hccparams1);

    mmio_write32(op_base + XHCI_USBCMD, mmio_read32(op_base + XHCI_USBCMD) & ~XHCI_USBCMD_RS);
    if (!wait_reg_set(op_base + XHCI_USBSTS, XHCI_USBSTS_HCH)) {
        USBDBG_ERROR("USBDBG: xHCI did not halt\n");
        return;
    }

    mmio_write32(op_base + XHCI_USBCMD, mmio_read32(op_base + XHCI_USBCMD) | XHCI_USBCMD_HCRST);
    if (!wait_reg_clear(op_base + XHCI_USBCMD, XHCI_USBCMD_HCRST) ||
        !wait_reg_clear(op_base + XHCI_USBSTS, XHCI_USBSTS_CNR)) {
        USBDBG_ERROR("USBDBG: xHCI reset failed\n");
        return;
    }

    init_rings();
    if (!setup_scratchpads()) return;

    mmio_write64(op_base + XHCI_DCBAAP, (uint32_t)(uintptr_t)&dcbaa[0]);
    mmio_write64(op_base + XHCI_CRCR, ((uint32_t)(uintptr_t)&command_ring[0]) | 1);
    mmio_write32(op_base + XHCI_CONFIG, max_slots);

    mmio_write32(interrupter0 + XHCI_INTR_ERSTSZ, 1);
    mmio_write64(interrupter0 + XHCI_INTR_ERSTBA, (uint32_t)(uintptr_t)&erst[0]);
    mmio_write64(interrupter0 + XHCI_INTR_ERDP, ((uint32_t)(uintptr_t)&event_ring[0]) | 0x8);
    mmio_write32(interrupter0 + XHCI_INTR_IMAN, 0x2);

    mmio_write32(op_base + XHCI_USBCMD, XHCI_USBCMD_RS | XHCI_USBCMD_INTE);
    if (!wait_reg_clear(op_base + XHCI_USBSTS, XHCI_USBSTS_HCH)) {
        USBDBG_ERROR("USBDBG: xHCI did not start\n");
        return;
    }

    controller_present = 1;
    transport_ready = 1;
    USBDBG_INFO("USBDBG: xHCI transport ready\n");

    reset_first_connected_port();
}

void usb_kbd_poll(void) {
    if (!transport_ready) return;

    while ((event_ring[event_dequeue].control & 1) == event_cycle) {
        xhci_trb_t *event = &event_ring[event_dequeue];
        uint32_t type = (event->control >> 10) & 0x3F;

        if (type == XHCI_TRB_COMMAND_COMPLETION) {
            uint8_t code = (uint8_t)(event->status >> 24);
            uint8_t slot = (uint8_t)(event->control >> 24);
            uint8_t completed = pending_command;
            pending_command = XHCI_PENDING_NONE;
            USBDBG_INFO("USBDBG: command event code=");
            USBDBG_HEX(code);
            USBDBG_PRINT(" slot=");
            USBDBG_HEX(slot);
            USBDBG_PRINT(" state=");
            USBDBG_HEX(enum_state);
            USBDBG_PRINT(" pending=");
            USBDBG_HEX(completed);
            USBDBG_PRINT(" cmdtrb=");
            USBDBG_HEX((uint32_t)event->parameter);
            USBDBG_PRINT("\n");
            if (completed == XHCI_PENDING_NONE) {
                USBDBG_INFO("USBDBG: ignoring stale command completion\n");
            } else if (code != 1) {
                if (completed == XHCI_PENDING_ADDRESS_DEVICE ||
                    completed == XHCI_PENDING_ADDRESS_DEVICE_BSR) {
                    if (address_retry_count < 3 && selected_port != 0) {
                        uint32_t next_mps = next_ep0_mps_retry(address_retry_count + 1);
                        if (next_mps == 0 && selected_speed != 2) {
                            USBDBG_ERROR("USBDBG: command failed pending=");
                            usb_error_hex(completed);
                            usb_error_print(" code=");
                            usb_error_hex(code);
                            usb_error_print("\n");
                            print_address_failure_debug(code);
                            fail_transport("USBDBG: no valid ep0 mps retry for this speed\n");
                        } else {
                            address_retry_count++;
                            ep0_mps_override = next_mps;
                            pending_command = XHCI_PENDING_NONE;
                            USBDBG_INFO("USBDBG: retrying port reset after address failure mps=");
                            USBDBG_HEX(ep0_mps_override);
                            USBDBG_PRINT("\n");
                            disable_slot_for_retry();
                        }
                    } else {
                        USBDBG_ERROR("USBDBG: command failed pending=");
                        usb_error_hex(completed);
                        usb_error_print(" code=");
                        usb_error_hex(code);
                        usb_error_print("\n");
                        print_address_failure_debug(code);
                        fail_transport("USBDBG: address retries exhausted; USB native disabled\n");
                    }
                } else if (completed == XHCI_PENDING_DISABLE_SLOT && retry_after_disable_slot) {
                    enabled_slot = 0;
                    retry_after_disable_slot = 0;
                    start_port_reset(selected_port);
                } else {
                    USBDBG_ERROR("USBDBG: command failed pending=");
                    usb_error_hex(completed);
                    usb_error_print(" code=");
                    usb_error_hex(code);
                    usb_error_print("\n");
                }
            } else if (completed == XHCI_PENDING_ENABLE_SLOT) {
                if (slot != 0) {
                    enabled_slot = slot;
                    xhci_wait_ticks(2);
                    prepare_address_device(enabled_slot, selected_speed >= 4, 1);
                } else {
                    USBDBG_ERROR("USBDBG: enable slot returned slot 0\n");
                }
            } else if (completed == XHCI_PENDING_ADDRESS_DEVICE_BSR) {
                enum_state = XHCI_ENUM_GET_DEVICE8_SENT;
                USBDBG_INFO("USBDBG: get first 8 device descriptor bytes\n");
                xhci_wait_ticks(10);
                if (!wait_selected_port_enabled()) {
                    retry_selected_port("USBDBG: port disabled before first descriptor\n");
                } else {
                    usb_control_in(0x80, 0x06, 0x0100, 0, 8, device_desc);
                }
            } else if (completed == XHCI_PENDING_ADDRESS_DEVICE) {
                enum_state = XHCI_ENUM_GET_DEVICE_SENT;
                USBDBG_INFO("USBDBG: address ok; get full device descriptor\n");
                xhci_wait_ticks(5);
                if (!wait_selected_port_enabled()) {
                    retry_selected_port("USBDBG: port disabled before full descriptor\n");
                } else {
                    usb_control_in(0x80, 0x06, 0x0100, 0, sizeof(device_desc), device_desc);
                }
            } else if (completed == XHCI_PENDING_DISABLE_SLOT) {
                enabled_slot = 0;
                if (retry_after_disable_slot) {
                    retry_after_disable_slot = 0;
                    start_port_reset(selected_port);
                }
            } else if (completed == XHCI_PENDING_CONFIGURE_ENDPOINT) {
                enum_state = XHCI_ENUM_READY;
                USBDBG_INFO("USBDBG: keyboard ready\n");
                queue_keyboard_report();
            } else {
                USBDBG_ERROR("USBDBG: command completion with no pending command\n");
            }
        } else if (type == XHCI_TRB_PORT_STATUS_CHANGE) {
            USBDBG_INFO("USBDBG: port status change event\n");
            if (selected_port != 0) {
                uint32_t portsc = mmio_read32(port_addr(selected_port));
                selected_speed = (uint8_t)((portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT);
                USBDBG_INFO("USBDBG: selected portsc=");
                USBDBG_HEX(portsc);
                USBDBG_PRINT(" speed=");
                USBDBG_HEX(selected_speed);
                USBDBG_PRINT(" state=");
                USBDBG_HEX(enum_state);
                USBDBG_PRINT("\n");
                clear_port_changes(selected_port, portsc);
                if (enum_state == XHCI_ENUM_RESET_SENT && (portsc & XHCI_PORTSC_PED)) {
                    xhci_wait_ticks(10);
                    enum_state = XHCI_ENUM_ENABLE_SLOT_SENT;
                    USBDBG_INFO("USBDBG: enable slot\n");
                    xhci_push_command(0, 0, XHCI_TRB_ENABLE_SLOT << 10,
                                      XHCI_PENDING_ENABLE_SLOT);
                } else if (enum_state == XHCI_ENUM_RESET_SENT) {
                    USBDBG_ERROR("USBDBG: port reset done but PED not set\n");
                }
            }
        } else if (type == XHCI_TRB_TRANSFER_EVENT) {
            uint8_t code = (uint8_t)(event->status >> 24);
            uint32_t trb_addr = (uint32_t)event->parameter;
            USBDBG_INFO("USBDBG: transfer event code=");
            USBDBG_HEX(code);
            USBDBG_PRINT(" trb=");
            USBDBG_HEX(trb_addr);
            USBDBG_PRINT(" state=");
            USBDBG_HEX(enum_state);
            USBDBG_PRINT("\n");
            if (!transfer_code_ok(code)) {
                print_transfer_failure_debug(code, trb_addr);
                if (code == 4 &&
                    (enum_state == XHCI_ENUM_GET_DEVICE8_SENT ||
                     enum_state == XHCI_ENUM_GET_DEVICE_SENT ||
                     enum_state == XHCI_ENUM_GET_CONFIG8_SENT ||
                     enum_state == XHCI_ENUM_GET_CONFIG_FULL_SENT ||
                     enum_state == XHCI_ENUM_SET_CONFIG_SENT ||
                     enum_state == XHCI_ENUM_SET_PROTOCOL_SENT)) {
                    retry_selected_port("USBDBG: control transfer failed; resetting port\n");
                }
            } else if (ep0_wait_trb && trb_addr == (uint32_t)(uintptr_t)ep0_wait_trb) {
                ep0_wait_trb = 0;
                if (enum_state == XHCI_ENUM_GET_DEVICE8_SENT) {
                    transfer_retry_count = 0;
                    USBDBG_INFO("USBDBG: device desc8 ok maxpkt=");
                    USBDBG_HEX(device_desc[7]);
                    USBDBG_PRINT("\n");
                    if (device_desc[7] == 8 || device_desc[7] == 16 ||
                        device_desc[7] == 32 || device_desc[7] == 64) {
                        ep0_mps_override = device_desc[7];
                    }
                    xhci_wait_ticks(2);
                    USBDBG_INFO("USBDBG: sending final address command\n");
                    prepare_address_device(enabled_slot, 0, 0);
                } else if (enum_state == XHCI_ENUM_GET_DEVICE_SENT) {
                    transfer_retry_count = 0;
                    USBDBG_INFO("USBDBG: device desc ok class=");
                    USBDBG_HEX(device_desc[4]);
                    USBDBG_PRINT(" subclass=");
                    USBDBG_HEX(device_desc[5]);
                    USBDBG_PRINT(" proto=");
                    USBDBG_HEX(device_desc[6]);
                    USBDBG_PRINT(" maxpkt=");
                    USBDBG_HEX(device_desc[7]);
                    USBDBG_PRINT("\n");
                    enum_state = XHCI_ENUM_GET_CONFIG8_SENT;
                    USBDBG_INFO("USBDBG: get config header\n");
                    usb_control_in(0x80, 0x06, 0x0200, 0, 8, config_desc);
                } else if (enum_state == XHCI_ENUM_GET_CONFIG8_SENT) {
                    uint16_t total = (uint16_t)config_desc[2] | ((uint16_t)config_desc[3] << 8);
                    if (total > sizeof(config_desc)) total = sizeof(config_desc);
                    enum_state = XHCI_ENUM_GET_CONFIG_FULL_SENT;
                    USBDBG_INFO("USBDBG: get full config total=");
                    USBDBG_HEX(total);
                    USBDBG_PRINT("\n");
                    usb_control_in(0x80, 0x06, 0x0200, 0, total, config_desc);
                } else if (enum_state == XHCI_ENUM_GET_CONFIG_FULL_SENT) {
                    if (!parse_keyboard_endpoint()) {
                        fail_transport("USBDBG: no HID boot keyboard in config; USB native disabled\n");
                    } else {
                        enum_state = XHCI_ENUM_SET_CONFIG_SENT;
                        USBDBG_INFO("USBDBG: set configuration cfg=");
                        USBDBG_HEX(kbd_config_value);
                        USBDBG_PRINT("\n");
                        usb_control_out(0x00, 0x09, kbd_config_value, 0, 0, 0);
                    }
                } else if (enum_state == XHCI_ENUM_SET_CONFIG_SENT) {
                    enum_state = XHCI_ENUM_SET_PROTOCOL_SENT;
                    USBDBG_INFO("USBDBG: set boot protocol iface=");
                    USBDBG_HEX(kbd_interface);
                    USBDBG_PRINT("\n");
                    usb_control_out(0x21, 0x0B, 0, kbd_interface, 0, 0);
                } else if (enum_state == XHCI_ENUM_SET_PROTOCOL_SENT) {
                    prepare_configure_keyboard_endpoint();
                }
            } else if (intr_wait_trb && trb_addr == (uint32_t)(uintptr_t)intr_wait_trb) {
                intr_wait_trb = 0;
                USBDBG_INFO("USBDBG: report ");
                for (int i = 0; i < 8; i++) {
                    USBDBG_HEX_BYTE(keyboard_report[i]);
                    USBDBG_PRINT(" ");
                }
                USBDBG_PRINT("\n");
                usb_kbd_feed_boot_report(keyboard_report);
                queue_keyboard_report();
            } else {
                USBDBG_ERROR("USBDBG: transfer event did not match pending TRB\n");
            }
        }

        event_dequeue++;
        if (event_dequeue >= XHCI_EVENT_RING_SIZE) {
            event_dequeue = 0;
            event_cycle ^= 1;
        }
        mmio_write64(interrupter0 + XHCI_INTR_ERDP,
                     ((uint32_t)(uintptr_t)&event_ring[event_dequeue]) | 0x8);
    }
}

int usb_kbd_has_controller(void) {
    return controller_present;
}

int usb_kbd_is_ready(void) {
    return transport_ready && enum_state == XHCI_ENUM_READY &&
           kbd_ep_addr != 0 && intr_wait_trb != 0;
}

int usb_kbd_has_failed(void) {
    return transport_failed;
}
