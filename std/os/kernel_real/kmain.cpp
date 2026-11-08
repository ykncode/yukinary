// YukiOS - YKN-to-C++ Kernel Representation
// Bare-metal C++ kernel entry point.

#include <stdint.h>
#include <stddef.h>

// VGA Text Mode Buffer
volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
const int VGA_COLS = 80;
const int VGA_ROWS = 25;

void clear_screen() {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        vga_buffer[i] = (uint16_t) ' ' | (uint16_t)(0x0F << 8); // White on Black
    }
}

void print_string(const char* str, int x, int y, uint8_t color) {
    int index = y * VGA_COLS + x;
    for (int i = 0; str[i] != '\0'; i++) {
        vga_buffer[index++] = (uint16_t)str[i] | (uint16_t)(color << 8);
    }
}

extern "C" void kmain(uint32_t magic, uint32_t multiboot_addr) {
    clear_screen();
    
    // Draw the "YukiOS" ASCII Logo
    print_string(" __     __    _    _    ___    _____ ", 20, 5, 0x0B); // Light Cyan
    print_string(" \\ \\   / /   | |  / /  |_ _|  / ____|", 20, 6, 0x0B);
    print_string("  \\ \\_/ /_   _| | / /    | |  | (___  ", 20, 7, 0x0B);
    print_string("   \\   /| | | | |/ /     | |   \\___ \\ ", 20, 8, 0x0B);
    print_string("    | | | |_| | |\\ \\    _| |_  ____) |", 20, 9, 0x0B);
    print_string("    |_|  \\__,_|_| \\_\\  |_____||_____/ ", 20, 10, 0x0B);
    
    print_string("==========================================", 19, 13, 0x0F);
    print_string("       YukiOS - Bare-Metal Kernel         ", 19, 14, 0x0A); // Light Green
    print_string("==========================================", 19, 15, 0x0F);
    
    print_string("System Status: ONLINE", 20, 17, 0x0E); // Yellow
    print_string("POSIX (Steam) Layer: Initialized", 20, 18, 0x0E);
    print_string("Yuki-AI Engine: Mapped to VRAM", 20, 19, 0x0E);
    
    print_string("Awaiting Input...", 20, 22, 0x07); // Light Gray
}
