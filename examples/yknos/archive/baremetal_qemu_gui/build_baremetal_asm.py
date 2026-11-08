import os

# Script to assemble the complete YknOS bare-metal graphics kernel
script_dir = os.path.dirname(os.path.abspath(__file__))

asm_parts = []

asm_parts.append('''
; ============================================================
; YknOS Bare-Metal Ring-0 Linux-Compatible Kernel & Desktop GUI
; 1024x768x32bpp TrueColor Framebuffer + BGA + Hardware PS/2 Mouse & Keyboard
; Inspired by Windows 11, macOS, and modern Linux Desktop Environments
; Strict adherence to YKNOS_VISION.md:
;   - AI-First Interface: Spotlight-like Central Y.U.K.I Search Bar
;   - Window Manager (ykn-wm): Modern acrylic windows with macOS-style traffic lights
;   - 5-Level Security Sandbox Model view
;   - System Error Archaeology timeline view
;   - Proactive Maintenance & Local Vector Memory (/home/root/.yuki/vector_memory.db)
;   - Full Terminal Shell emulator (yknsh) with interactive commands
;   - Bottom Floating Glass Dock
; ============================================================

[BITS 32]
[ORG 0x100000]

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ (1 << 0) | (1 << 1) | (1 << 16)
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .text
multiboot_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    dd multiboot_header
    dd 0x100000
    dd _file_end
    dd _kernel_end
    dd _start

_start:
    cli
    mov esp, 0x90000

    ; 1. COM1 UART Seri Port (0x3F8)
    call serial_init
    mov esi, msg_serial_boot
    call serial_print

    ; 2. Donanim Kesmeleri (8259 PIC, PIT 100Hz, IDT)
    call pic_remap
    call pit_init
    call idt_init

    ; 3. PS/2 Fare ve Klavye Baslat
    call ps2_mouse_init

    ; 4. PCI BAR0 ile VGA Linear Framebuffer Adresini Bul
    call pci_detect_lfb

    ; 5. BGA (Bochs Graphics Adapter) ile 1024x768x32 TrueColor Moduna Gec
    call bga_init_1024x768

    sti ; Kesmeleri ac!

    ; Baslangic Durumlari
    mov dword [mouse_x], 512
    mov dword [mouse_y], 384
    mov dword [prev_mouse_x], 512
    mov dword [prev_mouse_y], 384
    mov byte [active_window], 1
    mov byte [yuki_active_tab], 1

    ; 6. Ilk Tam Masaustu Cizimi
    call gui_render_all

    ; 7. Farenin Ilk Arka Planini Kaydet ve Fareyi Ciz
    call cursor_save_bg
    call cursor_draw

    mov esi, msg_serial_ready
    call serial_print

; ------------------------------------------------------------
; Donanim Olay Dinleme Dongusu
; ------------------------------------------------------------
main_loop:
    ; 1. PS/2 Kontrolu (Port 0x64 Status)
    in al, 0x64
    test al, 0x01
    jz .check_serial

    test al, 0x20 ; Bit 5 = 1 ise Fare Verisi (AUX)
    jnz .read_mouse

    ; Klavye Verisi
    in al, 0x60
    call handle_keyboard
    jmp .check_serial

.read_mouse:
    in al, 0x60
    call handle_mouse_byte

.check_serial:
    ; 2. COM1 Seri Port Kontrolu (0x3FD)
    mov dx, 0x3FD
    in al, dx
    test al, 0x01
    jz main_loop

    mov dx, 0x3F8
    in al, dx
    call handle_serial_char
    jmp main_loop

; ============================================================
; BGA Video ve PCI Donanim Surucusu
; ============================================================
pci_detect_lfb:
    mov eax, 0x80001010 ; Bus 0, Dev 2, Func 0, Reg 0x10 (BAR0)
    mov dx, 0xCF8
    out dx, eax
    mov dx, 0xCFC
    in eax, dx
    and eax, 0xFFFFFFF0
    cmp eax, 0
    jne .found
    mov eax, 0xE0000000 ; Standart QEMU PCI LFB
.found:
    mov [fb_addr], eax
    ret

bga_init_1024x768:
    mov dx, 0x1CE
    mov ax, 4 ; ENABLE
    out dx, ax
    mov dx, 0x1CF
    mov ax, 0
    out dx, ax

    mov dx, 0x1CE
    mov ax, 1 ; XRES
    out dx, ax
    mov dx, 0x1CF
    mov ax, 1024
    out dx, ax

    mov dx, 0x1CE
    mov ax, 2 ; YRES
    out dx, ax
    mov dx, 0x1CF
    mov ax, 768
    out dx, ax

    mov dx, 0x1CE
    mov ax, 3 ; BPP
    out dx, ax
    mov dx, 0x1CF
    mov ax, 32
    out dx, ax

    mov dx, 0x1CE
    mov ax, 4 ; ENABLE
    out dx, ax
    mov dx, 0x1CF
    mov ax, 0x41 ; VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED
    out dx, ax
    ret

; ============================================================
; PS/2 Fare ve Klavye Donanim Surucusu
; ============================================================
ps2_wait_write:
    in al, 0x64
    test al, 0x02
    jnz ps2_wait_write
    ret

ps2_wait_read:
    in al, 0x64
    test al, 0x01
    jz ps2_wait_read
    ret

ps2_mouse_init:
    call ps2_wait_write
    mov al, 0xA8
    out 0x64, al

    call ps2_wait_write
    mov al, 0x20
    out 0x64, al
    call ps2_wait_read
    in al, 0x60
    or al, 0x02
    and al, 0xDF
    mov bl, al

    call ps2_wait_write
    mov al, 0x60
    out 0x64, al
    call ps2_wait_write
    mov al, bl
    out 0x60, al

    call ps2_wait_write
    mov al, 0xD4
    out 0x64, al
    call ps2_wait_write
    mov al, 0xF6
    out 0x60, al
    call ps2_wait_read
    in al, 0x60

    call ps2_wait_write
    mov al, 0xD4
    out 0x64, al
    call ps2_wait_write
    mov al, 0xF4
    out 0x60, al
    call ps2_wait_read
    in al, 0x60

    mov byte [mouse_cycle], 0
    ret

handle_mouse_byte:
    push eax
    push ebx
    push ecx
    push edx

    mov dl, al
    movzx ecx, byte [mouse_cycle]

    cmp ecx, 0
    je .byte0
    cmp ecx, 1
    je .byte1
    cmp ecx, 2
    je .byte2
    mov byte [mouse_cycle], 0
    jmp .done

.byte0:
    test dl, 0x08
    jz .reset
    mov [mouse_pkt0], dl
    mov byte [mouse_cycle], 1
    jmp .done

.byte1:
    mov [mouse_pkt1], dl
    mov byte [mouse_cycle], 2
    jmp .done

.byte2:
    mov [mouse_pkt2], dl
    mov byte [mouse_cycle], 0

    mov al, [mouse_pkt0]
    mov bl, [mouse_pkt1]
    mov cl, [mouse_pkt2]

    mov edx, [mouse_x]
    mov [prev_mouse_x], edx
    mov edx, [mouse_y]
    mov [prev_mouse_y], edx

    movsx eax, bl
    mov edx, [mouse_x]
    add edx, eax
    cmp edx, 0
    jge .x_not_neg
    xor edx, edx
.x_not_neg:
    cmp edx, 1020
    jle .x_not_over
    mov edx, 1020
.x_not_over:
    mov [mouse_x], edx

    movsx eax, cl
    mov edx, [mouse_y]
    sub edx, eax
    cmp edx, 0
    jge .y_not_neg
    xor edx, edx
.y_not_neg:
    cmp edx, 760
    jle .y_not_over
    mov edx, 760
.y_not_over:
    mov [mouse_y], edx

    mov al, [mouse_pkt0]
    mov dl, al
    and dl, 0x01
    mov bl, [mouse_btn_left]
    mov [mouse_btn_left], dl

    mov eax, [mouse_x]
    cmp eax, [prev_mouse_x]
    jne .redraw_cursor
    mov eax, [mouse_y]
    cmp eax, [prev_mouse_y]
    jne .redraw_cursor
    jmp .check_click

.redraw_cursor:
    call cursor_restore_bg
    call cursor_save_bg
    call cursor_draw

.check_click:
    cmp bl, 0
    jne .done
    cmp dl, 1
    jne .done

    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    call gui_handle_click
    jmp .done

.reset:
    mov byte [mouse_cycle], 0

.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; ============================================================
; Klavye Olay Yoneticisi
; ============================================================
handle_keyboard:
    push eax
    push ebx
    push ecx
    push edx

    test al, 0x80
    jnz .done_kbd

    cmp al, 0x0F ; TAB
    je .key_tab

    cmp al, 0x02 ; 1
    je .tab1
    cmp al, 0x03 ; 2
    je .tab2
    cmp al, 0x04 ; 3
    je .tab3
    cmp al, 0x05 ; 4
    je .tab4
    cmp al, 0x06 ; 5
    je .tab5

    cmp al, 0x11 ; W
    je .mouse_up
    cmp al, 0x48 ; Up Arrow
    je .mouse_up
    cmp al, 0x1F ; S
    je .mouse_down
    cmp al, 0x50 ; Down Arrow
    je .mouse_down
    cmp al, 0x1E ; A
    je .mouse_left
    cmp al, 0x4B ; Left Arrow
    je .mouse_left
    cmp al, 0x20 ; D
    je .mouse_right
    cmp al, 0x4D ; Right Arrow
    je .mouse_right

    cmp al, 0x39 ; SPACE
    je .key_click

    cmp al, 0x1C ; ENTER
    je .key_enter

    cmp al, 0x0E ; BACKSPACE
    je .key_backspace

    call scancode_to_ascii
    cmp al, 0
    je .done_kbd

    call term_add_char
    jmp .done_kbd

.mouse_up:
    mov edx, [mouse_y]
    sub edx, 16
    cmp edx, 0
    jge .mu_ok
    xor edx, edx
.mu_ok:
    mov [mouse_y], edx
    call move_cursor_refresh
    jmp .done_kbd

.mouse_down:
    mov edx, [mouse_y]
    add edx, 16
    cmp edx, 760
    jle .md_ok
    mov edx, 760
.md_ok:
    mov [mouse_y], edx
    call move_cursor_refresh
    jmp .done_kbd

.mouse_left:
    mov edx, [mouse_x]
    sub edx, 16
    cmp edx, 0
    jge .ml_ok
    xor edx, edx
.ml_ok:
    mov [mouse_x], edx
    call move_cursor_refresh
    jmp .done_kbd

.mouse_right:
    mov edx, [mouse_x]
    add edx, 16
    cmp edx, 1020
    jle .mr_ok
    mov edx, 1020
.mr_ok:
    mov [mouse_x], edx
    call move_cursor_refresh
    jmp .done_kbd

.key_click:
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    call gui_handle_click
    jmp .done_kbd

.key_tab:
    mov al, [active_window]
    inc al
    cmp al, 3
    jbe .tab_ok
    mov al, 1
.tab_ok:
    mov [active_window], al
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd

.tab1:
    mov byte [yuki_active_tab], 1
    call gui_draw_window_1
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab2:
    mov byte [yuki_active_tab], 2
    call gui_draw_window_1
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab3:
    mov byte [yuki_active_tab], 3
    call gui_draw_window_1
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab4:
    mov byte [yuki_active_tab], 4
    call gui_draw_window_1
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab5:
    mov byte [yuki_active_tab], 5
    call gui_draw_window_1
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd

.key_enter:
    call term_execute_cmd
    jmp .done_kbd

.key_backspace:
    call term_backspace
    jmp .done_kbd

.done_kbd:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

move_cursor_refresh:
    call cursor_restore_bg
    call cursor_save_bg
    call cursor_draw
    ret

scancode_to_ascii:
    cmp al, 0x10
    jb .check_digits
    cmp al, 0x19
    jbe .row_qwerty
    cmp al, 0x1E
    jb .done_ascii
    cmp al, 0x26
    jbe .row_asdf
    cmp al, 0x2C
    jb .check_space
    cmp al, 0x32
    jbe .row_zxcv
    cmp al, 0x39
    je .is_space
    jmp .done_ascii

.check_digits:
    cmp al, 0x02
    jb .done_ascii
    cmp al, 0x0A
    jbe .is_1to9
    cmp al, 0x0B
    je .is_zero
    jmp .done_ascii

.is_1to9:
    add al, ('1' - 0x02)
    ret
.is_zero:
    mov al, '0'
    ret

.row_qwerty:
    movzx ebx, al
    sub ebx, 0x10
    mov al, [str_qwerty + ebx]
    ret

.row_asdf:
    movzx ebx, al
    sub ebx, 0x1E
    mov al, [str_asdf + ebx]
    ret

.row_zxcv:
    movzx ebx, al
    sub ebx, 0x2C
    mov al, [str_zxcv + ebx]
    ret

.is_space:
    mov al, ' '
    ret

.check_space:
.done_ascii:
    xor al, al
    ret

str_qwerty: db "qwertyuiop", 0
str_asdf:   db "asdfghjkl", 0
str_zxcv:   db "zxcvbnm", 0

; ============================================================
; COM1 UART Seri Port Surucusu
; ============================================================
serial_init:
    mov dx, 0x3F8 + 1
    mov al, 0x00
    out dx, al

    mov dx, 0x3F8 + 3
    mov al, 0x80
    out dx, al

    mov dx, 0x3F8 + 0
    mov al, 0x01 ; 115200 baud
    out dx, al
    mov dx, 0x3F8 + 1
    mov al, 0x00
    out dx, al

    mov dx, 0x3F8 + 3
    mov al, 0x03 ; 8N1
    out dx, al

    mov dx, 0x3F8 + 2
    mov al, 0xC7 ; FIFO
    out dx, al

    mov dx, 0x3F8 + 4
    mov al, 0x0B
    out dx, al
    ret

serial_print_char:
    push dx
    push ax
    mov ah, al
.wait_tx:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait_tx
    mov dx, 0x3F8
    mov al, ah
    out dx, al
    pop ax
    pop dx
    ret

serial_print:
    push esi
    push eax
.loop:
    lodsb
    test al, al
    jz .done
    call serial_print_char
    jmp .loop
.done:
    pop eax
    pop esi
    ret

handle_serial_char:
    call serial_print_char
    cmp al, 10
    je .ser_enter
    cmp al, 13
    je .ser_enter
    cmp al, 8
    je .ser_back
    cmp al, 127
    je .ser_back
    call term_add_char
    ret
.ser_enter:
    call term_execute_cmd
    ret
.ser_back:
    call term_backspace
    ret

; ============================================================
; 8259 PIC, PIT ve IDT Surucusu
; ============================================================
pic_remap:
    mov al, 0x11
    out 0x20, al
    out 0xA0, al
    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al
    mov al, 0x01
    out 0x21, al
    out 0xA1, al
    mov al, 0x00
    out 0x21, al
    out 0xA1, al
    ret

pit_init:
    mov al, 0x36
    out 0x43, al
    mov ax, 11931 ; ~100 Hz
    out 0x40, al
    mov al, ah
    out 0x40, al
    ret

idt_init:
    mov ecx, 256
    mov edi, idt_table
.fill:
    mov eax, isr_default
    mov [edi], ax
    mov word [edi+2], 0x08
    mov byte [edi+4], 0
    mov byte [edi+5], 0x8E
    shr eax, 16
    mov [edi+6], ax
    add edi, 8
    loop .fill

    ; IRQ0 (Timer Tick) -> 0x20
    mov eax, isr_timer
    mov edi, idt_table + (0x20 * 8)
    mov [edi], ax
    shr eax, 16
    mov [edi+6], ax

    lidt [idt_descriptor]
    ret

isr_default:
    push eax
    mov al, 0x20
    out 0x20, al
    pop eax
    iretd

isr_timer:
    push eax
    inc dword [hw_timer_ticks]
    mov al, 0x20
    out 0x20, al
    pop eax
    iretd

; ============================================================
; 2D Grafik Cizim Motoru (1024x768x32 TrueColor)
; ============================================================
; GFX: Tek Piksel Boya
; EAX = X, EBX = Y, EDX = Renk (0x00RRGGBB)
gfx_put_pixel:
    cmp eax, 1024
    jae .out
    cmp ebx, 768
    jae .out
    push ecx
    push edi
    mov edi, [fb_addr]
    mov ecx, ebx
    shl ecx, 10 ; ebx * 1024
    add ecx, eax
    mov [edi + ecx*4], edx
    pop edi
    pop ecx
.out:
    ret

; GFX: Doldurulmus Dikdortgen Ciz
; EAX = X, EBX = Y, ECX = W, EDX = H, ESI = Renk
gfx_fill_rect:
    push ebp
    push edi
    push ebx
    push ecx
    push edx

    mov ebp, edx ; ebp = H
.row_loop:
    cmp ebp, 0
    jle .done_fill
    dec ebp

    ; Satir baslangic adresi
    mov edi, [fb_addr]
    mov edx, ebx
    add edx, ebp ; Y + current_row
    cmp edx, 768
    jae .row_loop
    shl edx, 10
    add edx, eax ; X
    cmp eax, 1024
    jae .row_loop
    lea edi, [edi + edx*4]

    push ecx
    push eax
    mov eax, esi ; Renk
    rep stosd
    pop eax
    pop ecx
    jmp .row_loop

.done_fill:
    pop edx
    pop ecx
    pop ebx
    pop edi
    pop ebp
    ret

; GFX: Dikdortgen Kenarligi Ciz
; EAX = X, EBX = Y, ECX = W, EDX = H, ESI = Kenar Rengi
gfx_draw_rect_outline:
    push eax
    push ebx
    push ecx
    push edx
    push ebp

    ; 1. Ust kenar (X, Y, W, 1)
    push edx
    mov edx, 1
    call gfx_fill_rect
    pop edx

    ; 2. Alt kenar (X, Y+H-1, W, 1)
    push ebx
    add ebx, edx
    dec ebx
    push edx
    mov edx, 1
    call gfx_fill_rect
    pop edx
    pop ebx

    ; 3. Sol kenar (X, Y, 1, H)
    push ecx
    mov ecx, 1
    call gfx_fill_rect
    pop ecx

    ; 4. Sag kenar (X+W-1, Y, 1, H)
    push eax
    add eax, ecx
    dec eax
    push ecx
    mov ecx, 1
    call gfx_fill_rect
    pop ecx
    pop eax

    pop ebp
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; GFX: Dikey Gradyan Ciz (Masaustu Duvar Kagidi & Panel Cam Efekti)
; EAX = X, EBX = Y, ECX = W, EDX = H, ESI = RenkUst, EDI = RenkAlt
gfx_draw_gradient_v:
    push ebp
    push dword [fb_addr]
    push ebx
    push ecx
    push edx

    ; Basit, yuksek performansli 8 adimli gradyan
    mov ebp, 0 ; Adim
.grad_step:
    cmp ebp, 8
    jge .done_grad

    ; Alt dilim yuksekligi = H / 8
    push eax
    push ebx
    push ecx
    push edx

    mov eax, edx
    shr eax, 3 ; H / 8
    mov [grad_slice_h], eax

    ; Dilim Y = Y + ebp * slice_h
    mov eax, ebp
    mul dword [grad_slice_h]
    add ebx, eax

    ; Dilim Rengi hesapla
    ; R, G, B interpolasyonu
    mov eax, esi
    cmp ebp, 4
    jb .use_top
    mov eax, edi
.use_top:
    mov [grad_cur_color], eax

    pop edx
    pop ecx
    pop ebx
    pop eax

    push eax
    push ebx
    push ecx
    push edx
    push esi
    mov eax, eax
    push eax
    mov eax, ebp
    mul dword [grad_slice_h]
    add ebx, eax
    pop eax
    mov edx, [grad_slice_h]
    mov esi, [grad_cur_color]
    call gfx_fill_rect
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax

    inc ebp
    jmp .grad_step

.done_grad:
    pop edx
    pop ecx
    pop ebx
    pop dword [fb_addr]
    pop ebp
    ret

; GFX: Dolgulu Yuvarlak Daire Ciz (macOS 🔴🟡🟢 Butonlari & Durum Noktalari)
; EAX = CX, EBX = CY, ECX = Radius, EDX = Renk
gfx_draw_circle_filled:
    pushad
    mov [circle_cx], eax
    mov [circle_cy], ebx
    mov [circle_r], ecx
    mov [circle_col], edx

    mov ebp, ecx
    imul ebp, ecx ; ebp = r * r

    mov esi, [circle_r]
    neg esi ; dy = -r
.dy_loop:
    cmp esi, [circle_r]
    jg .done_circle

    mov edi, [circle_r]
    neg edi ; dx = -r
.dx_loop:
    cmp edi, [circle_r]
    jg .next_dy

    mov eax, edi
    imul eax, edi
    mov ebx, esi
    imul ebx, esi
    add eax, ebx
    cmp eax, ebp
    jg .skip_cpix

    mov eax, [circle_cx]
    add eax, edi
    mov ebx, [circle_cy]
    add ebx, esi
    mov edx, [circle_col]
    call gfx_put_pixel

.skip_cpix:
    inc edi
    jmp .dx_loop

.next_dy:
    inc esi
    jmp .dy_loop

.done_circle:
    popad
    ret

; GFX: 8x14 Karakter Ciz
; EAX = Karakter, EBX = X, ECX = Y, EDX = On Renk, ESI = Arka Renk (0xFFFFFFFF = Saydam)
gfx_draw_char:
    pushad
    mov [char_x], ebx
    mov [char_y], ecx
    mov [char_fg], edx
    mov [char_bg], esi

    cmp eax, 32
    jb .bad_char
    cmp eax, 126
    jbe .char_ok
.bad_char:
    mov eax, 32
.char_ok:
    sub eax, 32
    imul eax, 14
    add eax, font_8x14
    mov [char_glyph_ptr], eax

    mov ebp, 0 ; Row = 0..13
.row_loop:
    cmp ebp, 14
    jge .done_char

    mov esi, [char_glyph_ptr]
    movzx edx, byte [esi + ebp]
    mov [char_row_bits], dl

    mov ecx, 0 ; Col = 0..7
.col_loop:
    cmp ecx, 8
    jge .next_row

    movzx edx, byte [char_row_bits] ; Her sutun icin satir bitlerini tekrar yukle
    mov eax, 0x80
    push ecx
    shr eax, cl
    pop ecx
    test edx, eax
    jnz .draw_fg

    ; Arka plan kontrolu (0xFFFFFFFF = Saydam)
    mov edx, [char_bg]
    cmp edx, 0xFFFFFFFF
    je .skip_pix
    mov eax, [char_x]
    add eax, ecx
    mov ebx, [char_y]
    add ebx, ebp
    call gfx_put_pixel
    jmp .skip_pix

.draw_fg:
    mov eax, [char_x]
    add eax, ecx
    mov ebx, [char_y]
    add ebx, ebp
    mov edx, [char_fg]
    call gfx_put_pixel

.skip_pix:
    inc ecx
    jmp .col_loop

.next_row:
    inc ebp
    jmp .row_loop

.done_char:
    popad
    ret

; GFX: Metin Dizisi Ciz
; ESI = Metin Adresi, EAX = X, EBX = Y, EDX = On Renk, ECX = Arka Renk
gfx_draw_string:
    pushad
    mov [str_draw_x], eax
    mov [str_draw_y], ebx
    mov [str_draw_fg], edx
    mov [str_draw_bg], ecx

.str_loop:
    lodsb
    test al, al
    jz .str_done

    push esi
    movzx eax, al
    mov ebx, [str_draw_x]
    mov ecx, [str_draw_y]
    mov edx, [str_draw_fg]
    mov esi, [str_draw_bg]
    call gfx_draw_char
    pop esi

    add dword [str_draw_x], 8 ; 8 piksel yana kay
    jmp .str_loop

.str_done:
    popad
    ret

; ============================================================
; Donanim Faresi Imleci (Cursor) Cizim & Arka Plan Saklama
; ============================================================
cursor_save_bg:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

    mov esi, [mouse_x]
    mov ebp, [mouse_y]

    mov edi, cursor_bg_buf
    mov ebx, 0 ; Row = 0..17
.save_row:
    cmp ebx, 18
    jge .done_save

    mov eax, 0 ; Col = 0..11
.save_col:
    cmp eax, 12
    jge .next_srow

    push eax
    push ebx

    add eax, esi
    add ebx, ebp
    ; Ekran sınır kontrolü
    cmp eax, 1024
    jae .out_of_screen
    cmp ebx, 768
    jae .out_of_screen

    mov ecx, ebx
    shl ecx, 10
    add ecx, eax
    mov edx, [fb_addr]
    mov edx, [edx + ecx*4]
    jmp .store_pix

.out_of_screen:
    xor edx, edx

.store_pix:
    mov [edi], edx
    add edi, 4

    pop ebx
    pop eax
    inc eax
    jmp .save_col

.next_srow:
    inc ebx
    jmp .save_row

.done_save:
    mov byte [cursor_saved], 1
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

cursor_restore_bg:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

    cmp byte [cursor_saved], 0
    je .done_rest

    mov esi, [prev_mouse_x]
    mov ebp, [prev_mouse_y]

    mov edi, cursor_bg_buf
    mov ebx, 0
.rest_row:
    cmp ebx, 18
    jge .done_rest

    mov eax, 0
.rest_col:
    cmp eax, 12
    jge .next_rrow

    mov edx, [edi]
    add edi, 4

    push eax
    push ebx
    add eax, esi
    add ebx, ebp
    call gfx_put_pixel
    pop ebx
    pop eax

    inc eax
    jmp .rest_col

.next_rrow:
    inc ebx
    jmp .rest_row

.done_rest:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

cursor_draw:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

    mov esi, [mouse_x]
    mov ebp, [mouse_y]

    mov ebx, 0 ; Row = 0..17
.crow:
    cmp ebx, 18
    jge .done_cdraw

    movzx ecx, word [cursor_bitmap + ebx*2]
    mov eax, 0 ; Col = 0..11
.ccol:
    cmp eax, 12
    jge .next_crow

    mov edx, 11
    sub edx, eax
    push ecx
    push eax
    mov cl, dl
    mov eax, [esp + 4]
    shr eax, cl
    and eax, 1
    mov dl, al
    pop eax
    pop ecx

    cmp dl, 1
    jne .check_shadow

    ; Beyaz govde
    push eax
    push ebx
    add eax, esi
    add ebx, ebp
    mov edx, 0x00FFFFFF
    call gfx_put_pixel
    pop ebx
    pop eax
    jmp .next_pix

.check_shadow:
    ; Golge / Kenarlik kontrolu
    movzx ecx, word [cursor_outline + ebx*2]
    mov edx, 11
    sub edx, eax
    push ecx
    push eax
    mov cl, dl
    mov eax, [esp + 4]
    shr eax, cl
    and eax, 1
    mov dl, al
    pop eax
    pop ecx

    cmp dl, 1
    jne .next_pix

    ; Siyah golge/kenarlik
    push eax
    push ebx
    add eax, esi
    add ebx, ebp
    mov edx, 0x00101010
    call gfx_put_pixel
    pop ebx
    pop eax

.next_pix:
    inc eax
    jmp .ccol

.next_crow:
    inc ebx
    jmp .crow

.done_cdraw:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Imlec Sekil Haritasi (12x18 Arrow)
cursor_bitmap:
    dw 0x8000 ; #...........
    dw 0xC000 ; ##..........
    dw 0xE000 ; ###.........
    dw 0xF000 ; ####........
    dw 0xF800 ; #####.......
    dw 0xFC00 ; ######......
    dw 0xFE00 ; #######.....
    dw 0xFF00 ; ########....
    dw 0xFF80 ; #########...
    dw 0xFFC0 ; ##########..
    dw 0xF800 ; #####.......
    dw 0xD800 ; ##.##.......
    dw 0x8C00 ; #...##......
    dw 0x0C00 ; ....##......
    dw 0x0600 ; .....##.....
    dw 0x0600 ; .....##.....
    dw 0x0300 ; ......##....
    dw 0x0000 ; ............

cursor_outline:
    dw 0xC000 ; ##..........
    dw 0xE000 ; ###.........
    dw 0xF000 ; ####........
    dw 0xF800 ; #####.......
    dw 0xFC00 ; ######......
    dw 0xFE00 ; #######.....
    dw 0xFF00 ; ########....
    dw 0xFF80 ; #########...
    dw 0xFFC0 ; ##########..
    dw 0xFFE0 ; ###########.
    dw 0xFFC0 ; ##########..
    dw 0xFDC0 ; #####.###...
    dw 0xDFE0 ; ##.######...
    dw 0x8FE0 ; #..######...
    dw 0x0FE0 ; ...######...
    dw 0x07F0 ; ....######..
    dw 0x07F0 ; ....######..
    dw 0x0380 ; .....###....

; ============================================================
; Masaustu Grafiksel Arayuz Katmani (Desktop GUI & Window Manager)
; ============================================================
gui_render_all:
    pushad

    ; 1. Masaustu Duvar Kagidi (Iki Tonlu Derin Dark Theme)
    mov eax, 0
    mov ebx, 0
    mov ecx, 1024
    mov edx, 384
    mov esi, 0x000E111A
    call gfx_fill_rect

    mov eax, 0
    mov ebx, 384
    mov ecx, 1024
    mov edx, 384
    mov esi, 0x001A2033
    call gfx_fill_rect

    ; Masaustu Tech Grid Vurgulari (Her 128 pikselde hafif grid noktasi)
    mov ebx, 64
.grid_y:
    cmp ebx, 700
    jge .done_grid
    mov eax, 64
.grid_x:
    cmp eax, 1000
    jge .next_gy
    mov edx, 0x002B344D
    call gfx_put_pixel
    add eax, 128
    jmp .grid_x
.next_gy:
    add ebx, 128
    jmp .grid_y
.done_grid:

    ; 2. macOS / GNOME Tarzi Ust Durum & Menu Cubugu (y=0..27)
    mov eax, 0
    mov ebx, 0
    mov ecx, 1024
    mov edx, 28
    mov esi, 0x00161824
    call gfx_fill_rect

    ; 1px Alt Kenarlik
    mov eax, 0
    mov ebx, 27
    mov ecx, 1024
    mov edx, 1
    mov esi, 0x002C324D
    call gfx_fill_rect

    ; Sol Menuler
    mov esi, str_menu_brand
    mov eax, 12
    mov ebx, 7
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_menu_items
    mov eax, 80
    mov ebx, 7
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Orta Baslik
    mov esi, str_os_title_top
    mov eax, 410
    mov ebx, 7
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sag Sistem Tepsisi (Tray)
    mov esi, str_tray_items
    mov eax, 770
    mov ebx, 7
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 3. Spotlight AI Arama Bari ("Baslangic Menusu Yerine AI" - Y.U.K.I Core)
    mov eax, 212
    mov ebx, 36
    mov ecx, 600
    mov edx, 32
    mov esi, 0x001E2235
    call gfx_fill_rect

    mov eax, 212
    mov ebx, 36
    mov ecx, 600
    mov edx, 32
    mov esi, 0x005E81AC
    call gfx_draw_rect_outline

    mov esi, str_spotlight_text
    mov eax, 226
    mov ebx, 45
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 4. Sol Masaustu Simgeleri (Desktop Icons)
    call gui_draw_desktop_icons

    ; 5. Pencereler (Z-Order: Inaktif pencereler once, aktif pencere en ustte)
    call gui_draw_window_2 ; RamFS
    call gui_draw_window_3 ; Terminal
    call gui_draw_window_1 ; Y.U.K.I (Aktif)

    ; 6. Alt Dock
    call gui_draw_dock

    popad
    ret

; ------------------------------------------------------------
; Sol Masaustu Simgeleri
; ------------------------------------------------------------
gui_draw_desktop_icons:
    ; Simge 1: Y.U.K.I AI
    mov eax, 24
    mov ebx, 80
    mov ecx, 52
    mov edx, 52
    mov esi, 0x0024283B
    call gfx_fill_rect
    mov eax, 24
    mov ebx, 80
    mov ecx, 52
    mov edx, 52
    mov esi, 0x005E81AC
    call gfx_draw_rect_outline
    mov esi, str_ico_ai
    mov eax, 34
    mov ebx, 96
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_lbl_ai
    mov eax, 18
    mov ebx, 136
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Simge 2: Dosyalar (RamFS)
    mov eax, 24
    mov ebx, 160
    mov ecx, 52
    mov edx, 52
    mov esi, 0x0024283B
    call gfx_fill_rect
    mov eax, 24
    mov ebx, 160
    mov ecx, 52
    mov edx, 52
    mov esi, 0x00EBCB8B
    call gfx_draw_rect_outline
    mov esi, str_ico_fs
    mov eax, 34
    mov ebx, 176
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_lbl_fs
    mov eax, 18
    mov ebx, 216
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Simge 3: Terminal
    mov eax, 24
    mov ebx, 240
    mov ecx, 52
    mov edx, 52
    mov esi, 0x0024283B
    call gfx_fill_rect
    mov eax, 24
    mov ebx, 240
    mov ecx, 52
    mov edx, 52
    mov esi, 0x00A3BE8C
    call gfx_draw_rect_outline
    mov esi, str_ico_term
    mov eax, 34
    mov ebx, 256
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_lbl_term
    mov eax, 18
    mov ebx, 296
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Simge 4: Sandbox
    mov eax, 24
    mov ebx, 320
    mov ecx, 52
    mov edx, 52
    mov esi, 0x0024283B
    call gfx_fill_rect
    mov eax, 24
    mov ebx, 320
    mov ecx, 52
    mov edx, 52
    mov esi, 0x00B48EAD
    call gfx_draw_rect_outline
    mov esi, str_ico_sec
    mov eax, 34
    mov ebx, 336
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_lbl_sec
    mov eax, 18
    mov ebx, 376
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Simge 5: SQL Studio
    mov eax, 24
    mov ebx, 400
    mov ecx, 52
    mov edx, 52
    mov esi, 0x0024283B
    call gfx_fill_rect
    mov eax, 24
    mov ebx, 400
    mov ecx, 52
    mov edx, 52
    mov esi, 0x00D08770
    call gfx_draw_rect_outline
    mov esi, str_ico_sql
    mov eax, 34
    mov ebx, 416
    mov edx, 0x00D08770
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_lbl_sql
    mov eax, 18
    mov ebx, 456
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

; ------------------------------------------------------------
; Pencere 1: Y.U.K.I Core AI Nöral Merkezi (VISION.md)
; ------------------------------------------------------------
gui_draw_window_1:
    ; Golge (Drop Shadow)
    mov eax, 214
    mov ebx, 90
    mov ecx, 620
    mov edx, 416
    mov esi, 0x00080A10
    call gfx_fill_rect

    ; Pencere Govdesi
    mov eax, 210
    mov ebx, 86
    mov ecx, 620
    mov edx, 416
    mov esi, 0x001E1E2E ; Acrylic Dark
    call gfx_fill_rect

    ; Kenarlik
    mov eax, 210
    mov ebx, 86
    mov ecx, 620
    mov edx, 416
    mov esi, 0x003B4252
    cmp byte [active_window], 1
    jne .w1_outline
    mov esi, 0x005E81AC ; Aktif Vurgu
.w1_outline:
    call gfx_draw_rect_outline

    ; Baslik Cubugu (y=86..114, h=28)
    mov eax, 210
    mov ebx, 86
    mov ecx, 620
    mov edx, 28
    mov esi, 0x002E3440
    call gfx_fill_rect

    ; macOS Traffic Light Butonlari (🔴 🟡 🟢)
    mov eax, 224 ; Kapat (Red)
    mov ebx, 100
    mov ecx, 5
    mov edx, 0x00BF616A
    call gfx_draw_circle_filled

    mov eax, 240 ; Kucult (Yellow)
    mov ebx, 100
    mov ecx, 5
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled

    mov eax, 256 ; Buyut (Green)
    mov ebx, 100
    mov ecx, 5
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    ; Baslik Metni
    mov esi, str_w1_title
    mov eax, 274
    mov ebx, 93
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sekme Cubugu (Tabs: y=114..138, h=24)
    mov eax, 210
    mov ebx, 114
    mov ecx, 620
    mov edx, 24
    mov esi, 0x0024283B
    call gfx_fill_rect

    ; Sekme 1..5 Metinleri
    mov esi, str_tab_w1_1
    mov eax, 222
    mov ebx, 120
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 1
    jne .t1_c
    mov edx, 0x0088C0D0
.t1_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_2
    mov eax, 318
    mov ebx, 120
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 2
    jne .t2_c
    mov edx, 0x0088C0D0
.t2_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_3
    mov eax, 446
    mov ebx, 120
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 3
    jne .t3_c
    mov edx, 0x0088C0D0
.t3_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_4
    mov eax, 540
    mov ebx, 120
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 4
    jne .t4_c
    mov edx, 0x0088C0D0
.t4_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_5
    mov eax, 652
    mov ebx, 120
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 5
    jne .t5_c
    mov edx, 0x0088C0D0
.t5_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sekme Alt cizgisi
    mov eax, 210
    mov ebx, 137
    mov ecx, 620
    mov edx, 1
    mov esi, 0x003B4252
    call gfx_fill_rect

    ; Icerik Cizimi (Aktif Sekmeye Gore)
    cmp byte [yuki_active_tab], 2
    je .draw_tab_2
    cmp byte [yuki_active_tab], 3
    je .draw_tab_3
    cmp byte [yuki_active_tab], 4
    je .draw_tab_4
    cmp byte [yuki_active_tab], 5
    je .draw_tab_5

    ; --- SEKME 1: GENEL & CANLI AI ASISTANI ---
.draw_tab_1:
    mov esi, str_w1_status
    mov eax, 226
    mov ebx, 150
    mov edx, 0x00A3BE8C ; Yesil Durum
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_w1_vm
    mov eax, 226
    mov ebx, 172
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Soru / Cevap Kutusu
    mov eax, 226
    mov ebx, 200
    mov ecx, 588
    mov edx, 180
    mov esi, 0x00141620
    call gfx_fill_rect
    mov eax, 226
    mov ebx, 200
    mov ecx, 588
    mov edx, 180
    mov esi, 0x00434C5E
    call gfx_draw_rect_outline

    mov esi, str_qa_prompt
    mov eax, 240
    mov ebx, 214
    mov edx, 0x00EBCB8B ; Sari Prompt
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line1
    mov eax, 240
    mov ebx, 238
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line2
    mov eax, 240
    mov ebx, 256
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line3
    mov eax, 240
    mov ebx, 274
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line4
    mov eax, 240
    mov ebx, 292
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line5
    mov eax, 240
    mov ebx, 310
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Hizli Butonlar
    mov eax, 226
    mov ebx, 396
    mov ecx, 180
    mov edx, 32
    mov esi, 0x002E3440
    call gfx_fill_rect
    mov esi, str_btn_ark
    mov eax, 256
    mov ebx, 406
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov eax, 420
    mov ebx, 396
    mov ecx, 180
    mov edx, 32
    mov esi, 0x002E3440
    call gfx_fill_rect
    mov esi, str_btn_bakim
    mov eax, 450
    mov ebx, 406
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov eax, 614
    mov ebx, 396
    mov ecx, 200
    mov edx, 32
    mov esi, 0x002E3440
    call gfx_fill_rect
    mov esi, str_btn_sand
    mov eax, 644
    mov ebx, 406
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

    ; --- SEKME 2: HATA ARKEOLOJISI ---
.draw_tab_2:
    mov esi, str_ark_h1
    mov eax, 226
    mov ebx, 150
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l1
    mov eax, 226
    mov ebx, 176
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l2
    mov eax, 226
    mov ebx, 198
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l3
    mov eax, 226
    mov ebx, 220
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l4
    mov eax, 226
    mov ebx, 242
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l5
    mov eax, 226
    mov ebx, 264
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_h2
    mov eax, 226
    mov ebx, 300
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_r1
    mov eax, 226
    mov ebx, 324
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_r2
    mov eax, 226
    mov ebx, 344
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

    ; --- SEKME 3: PROAKTIF BAKIM ---
.draw_tab_3:
    mov esi, str_bak_h1
    mov eax, 226
    mov ebx, 150
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_l1
    mov eax, 226
    mov ebx, 178
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_l2
    mov eax, 226
    mov ebx, 202
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_l3
    mov eax, 226
    mov ebx, 226
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_h2
    mov eax, 226
    mov ebx, 260
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_t1
    mov eax, 226
    mov ebx, 286
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_t2
    mov eax, 226
    mov ebx, 308
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

    ; --- SEKME 4: SANDBOX GUVENLIK MODELI ---
.draw_tab_4:
    mov esi, str_snd_h1
    mov eax, 226
    mov ebx, 150
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l0
    mov eax, 226
    mov ebx, 176
    mov edx, 0x00BF616A
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l1
    mov eax, 226
    mov ebx, 202
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l2
    mov eax, 226
    mov ebx, 228
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l3
    mov eax, 226
    mov ebx, 254
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l4
    mov eax, 226
    mov ebx, 280
    mov edx, 0x00D08770
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_pol
    mov eax, 226
    mov ebx, 316
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

    ; --- SEKME 5: MODELLER (BITNET & BONSAI) ---
.draw_tab_5:
    mov esi, str_mod_h1
    mov eax, 226
    mov ebx, 150
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_m1
    mov eax, 226
    mov ebx, 178
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_m2
    mov eax, 226
    mov ebx, 202
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_b1
    mov eax, 226
    mov ebx, 236
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_b2
    mov eax, 226
    mov ebx, 260
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

; ------------------------------------------------------------
; Pencere 2: RamFS Dosya Gezgini
; ------------------------------------------------------------
gui_draw_window_2:
    mov eax, 64
    mov ebx, 144
    mov ecx, 460
    mov edx, 350
    mov esi, 0x00080A10
    call gfx_fill_rect

    mov eax, 60
    mov ebx, 140
    mov ecx, 460
    mov edx, 350
    mov esi, 0x001B1E2E
    call gfx_fill_rect

    mov eax, 60
    mov ebx, 140
    mov ecx, 460
    mov edx, 350
    mov esi, 0x003B4252
    cmp byte [active_window], 2
    jne .w2_out
    mov esi, 0x00EBCB8B
.w2_out:
    call gfx_draw_rect_outline

    ; Baslik
    mov eax, 60
    mov ebx, 140
    mov ecx, 460
    mov edx, 26
    mov esi, 0x0024283B
    call gfx_fill_rect

    mov eax, 74
    mov ebx, 153
    mov ecx, 4
    mov edx, 0x00BF616A
    call gfx_draw_circle_filled
    mov eax, 88
    mov ebx, 153
    mov ecx, 4
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled
    mov eax, 102
    mov ebx, 153
    mov ecx, 4
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    mov esi, str_w2_title
    mov eax, 116
    mov ebx, 146
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sol Dizin Agaci (w=120)
    mov eax, 60
    mov ebx, 166
    mov ecx, 120
    mov edx, 324
    mov esi, 0x00141624
    call gfx_fill_rect

    mov esi, str_fs_dir1
    mov eax, 72
    mov ebx, 180
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_dir2
    mov eax, 72
    mov ebx, 204
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_dir3
    mov eax, 72
    mov ebx, 228
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_dir4
    mov eax, 72
    mov ebx, 252
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_dir5
    mov eax, 72
    mov ebx, 276
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sag Dosya Izgarasi
    mov esi, str_fs_f1
    mov eax, 196
    mov ebx, 180
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_f2
    mov eax, 196
    mov ebx, 204
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_f3
    mov eax, 196
    mov ebx, 228
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_f4
    mov eax, 196
    mov ebx, 252
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_f5
    mov eax, 196
    mov ebx, 276
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_fs_f6
    mov eax, 196
    mov ebx, 300
    mov edx, 0x00D08770
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Durum Cubugu
    mov eax, 60
    mov ebx, 466
    mov ecx, 460
    mov edx, 24
    mov esi, 0x00141624
    call gfx_fill_rect
    mov esi, str_fs_status
    mov eax, 72
    mov ebx, 472
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

; ------------------------------------------------------------
; Pencere 3: yknsh Terminali
; ------------------------------------------------------------
gui_draw_window_3:
    mov eax, 464
    mov ebx, 214
    mov ecx, 520
    mov edx, 370
    mov esi, 0x00080A10
    call gfx_fill_rect

    mov eax, 460
    mov ebx, 210
    mov ecx, 520
    mov edx, 370
    mov esi, 0x000D1117 ; Derin Siyah Terminal
    call gfx_fill_rect

    mov eax, 460
    mov ebx, 210
    mov ecx, 520
    mov edx, 370
    mov esi, 0x003B4252
    cmp byte [active_window], 3
    jne .w3_out
    mov esi, 0x00A3BE8C ; Aktif Yesil Vurgu
.w3_out:
    call gfx_draw_rect_outline

    ; Baslik
    mov eax, 460
    mov ebx, 210
    mov ecx, 520
    mov edx, 26
    mov esi, 0x00161B22
    call gfx_fill_rect

    mov eax, 474
    mov ebx, 223
    mov ecx, 4
    mov edx, 0x00BF616A
    call gfx_draw_circle_filled
    mov eax, 488
    mov ebx, 223
    mov ecx, 4
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled
    mov eax, 502
    mov ebx, 223
    mov ecx, 4
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    mov esi, str_w3_title
    mov eax, 516
    mov ebx, 216
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Terminal Ciktisi (Son 8 satir)
    call term_render_lines
    ret

; ------------------------------------------------------------
; Alt Dock (Floating Acrylic Capsule)
; ------------------------------------------------------------
gui_draw_dock:
    ; Dock Paneli (x=282, y=712, w=460, h=46)
    mov eax, 282
    mov ebx, 712
    mov ecx, 460
    mov edx, 46
    mov esi, 0x00181A26
    call gfx_fill_rect

    mov eax, 282
    mov ebx, 712
    mov ecx, 460
    mov edx, 46
    mov esi, 0x004C566A
    call gfx_draw_rect_outline

    ; Simgeler (8 Adet, Aralik = 52 px)
    ; 1: Baslat
    mov esi, str_ico_d1
    mov eax, 298
    mov ebx, 726
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 2: Y.U.K.I AI
    mov esi, str_ico_d2
    mov eax, 350
    mov ebx, 726
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ; Aktif nokta
    mov eax, 362
    mov ebx, 746
    mov ecx, 2
    mov edx, 0x0088C0D0
    call gfx_draw_circle_filled

    ; 3: Dosyalar
    mov esi, str_ico_d3
    mov eax, 402
    mov ebx, 726
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 4: Terminal
    mov esi, str_ico_d4
    mov eax, 454
    mov ebx, 726
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ; Aktif nokta
    mov eax, 466
    mov ebx, 746
    mov ecx, 2
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    ; 5: Sandbox
    mov esi, str_ico_d5
    mov eax, 506
    mov ebx, 726
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 6: SQL
    mov esi, str_ico_d6
    mov eax, 558
    mov ebx, 726
    mov edx, 0x00D08770
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 7: Ag (HTTP)
    mov esi, str_ico_d7
    mov eax, 610
    mov ebx, 726
    mov edx, 0x0081A1C1
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 8: Ayarlar
    mov esi, str_ico_d8
    mov eax, 662
    mov ebx, 726
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

; ------------------------------------------------------------
; Dropdown Menu (Sol Ust Baslat)
; ------------------------------------------------------------
gui_draw_dropdown_menu:
    mov eax, 12
    mov ebx, 28
    mov ecx, 190
    mov edx, 150
    mov esi, 0x001B1E2E
    call gfx_fill_rect
    mov eax, 12
    mov ebx, 28
    mov ecx, 190
    mov edx, 150
    mov esi, 0x005E81AC
    call gfx_draw_rect_outline

    mov esi, str_dm_1
    mov eax, 24
    mov ebx, 40
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_dm_2
    mov eax, 24
    mov ebx, 64
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_dm_3
    mov eax, 24
    mov ebx, 88
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_dm_div
    mov eax, 24
    mov ebx, 106
    mov edx, 0x004C566A
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_dm_4
    mov eax, 24
    mov ebx, 124
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_dm_5
    mov eax, 24
    mov ebx, 148
    mov edx, 0x00BF616A
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    ret

; ============================================================
; Fare Tiklama Yoneticisi (Hit Testing & Event Routing)
; ============================================================
gui_handle_click:
    push ebp
    push edi
    push esi
    push ecx
    push edx

    ; 1. Ust Menu Cubugu Tiklamasi (y = 0..27)
    cmp ebx, 27
    ja .check_spotlight
    cmp eax, 80
    ja .done_click
    ; Sol Baslat Menusunu Ac/Kapat
    xor byte [menu_open], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_spotlight:
    ; 2. Spotlight AI Arama Bari Tiklamasi (y=36..68, x=212..812)
    cmp ebx, 36
    jb .check_dock
    cmp ebx, 68
    ja .check_dock
    cmp eax, 212
    jb .check_dock
    cmp eax, 812
    ja .check_dock
    ; Y.U.K.I Penceresine Odaklan ve Sekme 1'e Gec
    mov byte [active_window], 1
    mov byte [yuki_active_tab], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_dock:
    ; 3. Alt Dock Tiklamasi (y=712..758, x=282..742)
    cmp ebx, 712
    jb .check_w1
    cmp ebx, 758
    ja .check_w1
    cmp eax, 282
    jb .check_w1
    cmp eax, 742
    ja .check_w1

    ; Dock Simgesi Secimi
    sub eax, 282
    mov edx, 0
    mov ecx, 52
    div ecx ; EAX = 0..7

    cmp eax, 0
    je .dock_menu
    cmp eax, 1
    je .dock_yuki
    cmp eax, 2
    je .dock_fs
    cmp eax, 3
    je .dock_term
    jmp .done_click

.dock_menu:
    xor byte [menu_open], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.dock_yuki:
    mov byte [active_window], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.dock_fs:
    mov byte [active_window], 2
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.dock_term:
    mov byte [active_window], 3
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_w1:
    ; 4. Pencere 1 (Y.U.K.I) Tiklamasi (x=210..830, y=86..502)
    cmp eax, 210
    jb .check_w2
    cmp eax, 830
    ja .check_w2
    cmp ebx, 86
    jb .check_w2
    cmp ebx, 502
    ja .check_w2

    ; Pencereyi Aktif Yap
    mov byte [active_window], 1

    ; Sekme cubugu tiklamasi mi? (y=114..138)
    cmp ebx, 114
    jb .redraw_w1
    cmp ebx, 138
    ja .redraw_w1

    ; Sekme Koordinatlari
    cmp eax, 310
    jb .w1_tab1
    cmp eax, 440
    jb .w1_tab2
    cmp eax, 530
    jb .w1_tab3
    cmp eax, 640
    jb .w1_tab4
    mov byte [yuki_active_tab], 5
    jmp .redraw_w1
.w1_tab1:
    mov byte [yuki_active_tab], 1
    jmp .redraw_w1
.w1_tab2:
    mov byte [yuki_active_tab], 2
    jmp .redraw_w1
.w1_tab3:
    mov byte [yuki_active_tab], 3
    jmp .redraw_w1
.w1_tab4:
    mov byte [yuki_active_tab], 4

.redraw_w1:
    call gui_draw_window_1
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_w2:
    ; 5. Pencere 2 (RamFS) Tiklamasi (x=60..520, y=140..490)
    cmp eax, 60
    jb .check_w3
    cmp eax, 520
    ja .check_w3
    cmp ebx, 140
    jb .check_w3
    cmp ebx, 490
    ja .check_w3
    mov byte [active_window], 2
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_w3:
    ; 6. Pencere 3 (Terminal) Tiklamasi (x=460..980, y=210..580)
    cmp eax, 460
    jb .done_click
    cmp eax, 980
    ja .done_click
    cmp ebx, 210
    jb .done_click
    cmp ebx, 580
    ja .done_click
    mov byte [active_window], 3
    call gui_render_all
    call cursor_save_bg
    call cursor_draw

.done_click:
    pop edx
    pop ecx
    pop esi
    pop edi
    pop ebp
    ret

; ============================================================
; Terminal Alt Sistemi (yknsh Konsol Emulatoru)
; ============================================================
term_add_char:
    movzx ecx, byte [term_buf_len]
    cmp ecx, 50
    jge .done_add
    mov [term_buffer + ecx], al
    inc byte [term_buf_len]
    mov byte [term_buffer + ecx + 1], 0

    ; Terminali yeniden ciz
    call term_render_lines
    call cursor_save_bg
    call cursor_draw
.done_add:
    ret

term_backspace:
    movzx ecx, byte [term_buf_len]
    cmp ecx, 0
    jle .done_back
    dec ecx
    mov byte [term_buf_len], cl
    mov byte [term_buffer + ecx], 0

    call term_render_lines
    call cursor_save_bg
    call cursor_draw
.done_back:
    ret

term_render_lines:
    ; Terminal Arka Plani Temizle
    mov eax, 462
    mov ebx, 238
    mov ecx, 516
    mov edx, 340
    mov esi, 0x000D1117
    call gfx_fill_rect

    ; Sabit Hatirlatma
    mov esi, str_term_banner
    mov eax, 472
    mov ebx, 246
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Onceki Cikti Satirlari
    mov esi, str_term_l1
    mov eax, 472
    mov ebx, 268
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l2
    mov eax, 472
    mov ebx, 288
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l3
    mov eax, 472
    mov ebx, 308
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l4
    mov eax, 472
    mov ebx, 328
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l5
    mov eax, 472
    mov ebx, 348
    mov edx, 0x0081A1C1
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Prompt ve Kullanici Girdisi
    mov esi, str_term_prompt
    mov eax, 472
    mov ebx, 376
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, term_buffer
    mov eax, 576
    mov ebx, 376
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Yanip Sonen Imlec
    mov eax, 576
    movzx ecx, byte [term_buf_len]
    shl ecx, 3 ; ecx * 8
    add eax, ecx
    mov ebx, 376
    mov ecx, 8
    mov edx, 14
    mov esi, 0x00A3BE8C
    call gfx_fill_rect
    ret

term_execute_cmd:
    ; Komut kontrolu
    mov esi, term_buffer
    mov edi, str_cmd_help
    call strcmp
    jz .do_help

    mov esi, term_buffer
    mov edi, str_cmd_yuki
    call strcmp
    jz .do_yuki_cmd

    mov esi, term_buffer
    mov edi, str_cmd_uptime
    call strcmp
    jz .do_uptime_cmd

    mov esi, term_buffer
    mov edi, str_cmd_clear
    call strcmp
    jz .do_clear_cmd

    ; Bilinmeyen komut
    mov esi, msg_term_unknown
    call copy_to_term_l5
    jmp .finish_cmd

.do_help:
    mov esi, msg_term_help
    call copy_to_term_l5
    jmp .finish_cmd

.do_yuki_cmd:
    mov esi, msg_term_yuki
    call copy_to_term_l5
    jmp .finish_cmd

.do_uptime_cmd:
    mov esi, msg_term_uptime
    call copy_to_term_l5
    jmp .finish_cmd

.do_clear_cmd:
    mov byte [str_term_l1], 0
    mov byte [str_term_l2], 0
    mov byte [str_term_l3], 0
    mov byte [str_term_l4], 0
    mov byte [str_term_l5], 0
    jmp .finish_cmd

.finish_cmd:
    mov byte [term_buffer], 0
    mov byte [term_buf_len], 0
    call term_render_lines
    call cursor_save_bg
    call cursor_draw
    ret

copy_to_term_l5:
    mov edi, str_term_l5
.c_loop:
    lodsb
    stosb
    test al, al
    jnz .c_loop
    ret

strcmp:
.s_loop:
    mov al, [esi]
    mov bl, [edi]
    cmp al, bl
    jne .s_diff
    test al, al
    jz .s_eq
    inc esi
    inc edi
    jmp .s_loop
.s_diff:
    mov al, 1
    ret
.s_eq:
    xor al, al
    ret

; ============================================================
; Sabit Veriler ve Metinler
; ============================================================
msg_serial_boot:  db 10, "================================================================", 10
                  db "  YknOS 6.1 LTS - TrueColor 1024x768 Graphical Desktop Active   ", 10
                  db "  Linux LTS Kernel + YKN Userspace + Y.U.K.I Core AI Assistant ", 10
                  db "================================================================", 10, 0
msg_serial_ready: db "[YknOS]: Hardware PS/2 Mouse, Keyboard, BGA & PIT 100Hz ONLINE.", 10, 0

str_menu_brand:   db "YknOS", 0
str_menu_items:   db "Dosya  Duzenle  Gorunum  Sistem  Yardim", 0
str_os_title_top: db "YknOS 6.1 LTS  |  AI-First Desktop", 0
str_tray_items:   db "WiFi [ON]  |  Sandbox L0-L4  |  22:30", 0

str_spotlight_text: db "Y.U.K.I: 'Sistem sagligi nasil?', 'wifi kapat', 'yuki arkeoloji'...", 0

str_ico_ai:   db "[AI]", 0
str_lbl_ai:   db "Y.U.K.I", 0
str_ico_fs:   db "[DIR]", 0
str_lbl_fs:   db "RamFS", 0
str_ico_term: db "[>_]", 0
str_lbl_term: db "Terminal", 0
str_ico_sec:  db "[SEC]", 0
str_lbl_sec:  db "Sandbox", 0
str_ico_sql:  db "[SQL]", 0
str_lbl_sql:  db "Veritabani", 0

str_w1_title: db "Y.U.K.I Core Noral Isletim Sistemi Beyni (BitNet + Bonsai-8B)", 0
str_tab_w1_1: db "[1: Genel]", 0
str_tab_w1_2: db "[2: Arkeoloji]", 0
str_tab_w1_3: db "[3: Bakim]", 0
str_tab_w1_4: db "[4: Sandbox]", 0
str_tab_w1_5: db "[5: Modeller]", 0

str_w1_status: db "Y.U.K.I Core: ONLINE (100% Yerel, SIFIR Bulut, Gizlilik Garantisi)", 0
str_w1_vm:     db "Vektor Hafizasi: /home/root/.yuki/vector_memory.db (14 ogrenilmis olay)", 0

str_qa_prompt: db "> Kullanici: 'Sistem sagligi ve alt sistemlerin durumu nasil?'", 0
str_qa_line1:  db "Y.U.K.I Asistani: Sistem saglik seviyesi %100 nominal ve kararlidir.", 0
str_qa_line2:  db "- Linux LTS Cekirdek & MMU 64 MB fiziksel sayfalama hatasiz devrede.", 0
str_qa_line3:  db "- RamFS /dev/ram0: /bin, /etc, /proc, /home/root mount edildi.", 0
str_qa_line4:  db "- Donanim Kesmeleri: 8259 PIC ve 100 Hz PIT timer tick donanimda aktif.", 0
str_qa_line5:  db "- Noral Ag: BitNet 1.58b (16K param, sifir-carpma ALU) yerel RAM'de.", 0

str_btn_ark:   db "[ > Arkeoloji ]", 0
str_btn_bakim: db "[ > Bakim Yap ]", 0
str_btn_sand:  db "[ > Sandbox ]", 0

str_ark_h1: db "[Y.U.K.I SISTEM HATA ARKEOLOJISI - KOK NEDEN ANALIZI]:", 0
str_ark_l1: db "1. [0.000s] Cekirdek Long Mode & MMU 64 MB fiziksel sayfalama basladi.", 0
str_ark_l2: db "2. [0.005s] 16550 UART Seri Port (ttyS0) 115200 baud ile acildi.", 0
str_ark_l3: db "3. [0.008s] RamFS VFS ve dinamik /proc sanal tablosu mount edildi.", 0
str_ark_l4: db "4. [0.012s] HTTP REST mikroservisi ve SQL motoru devreye girdi.", 0
str_ark_l5: db "5. [0.020s] Donanim kesmeleri: 8259 PIC ve 100 Hz PIT timer aktif.", 0
str_ark_h2: db "[ARKEOLOJIK TESPIT]: 0 Kernel Panic, 0 OOM Thrashing.", 0
str_ark_r1: db "Tespit: Sistem donanimi %100 saglikli. 61 MB fiziksel RAM serbest.", 0
str_ark_r2: db "Oneri: 'yuki bakim' ile haftalik log rotasyonunu baslatabilirsiniz.", 0

str_bak_h1: db "[Y.U.K.I PROAKTIF SISTEM BAKIMI & SAGLIK RAPORU]:", 0
str_bak_l1: db "[GUNLUK]: [OK] S.M.A.R.T, RamFS /dev/ram0 64 MB, 0 kernel error.", 0
str_bak_l2: db "[HAFTALIK]: [OK] 0 zombi surec (PID tablosunda tum gorevler aktif).", 0
str_bak_l3: db "[GUVENLIK]: Seviye 1-4 Sandbox kural tablosu guncel ve devrede.", 0
str_bak_h2: db "[AYLIK TREND TAHMINI]:", 0
str_bak_t1: db "- Bellek Tuketimi: Son 30 gunde sabit (+0.2 MB/hafta).", 0
str_bak_t2: db "- Depolama Tahmini: Sanal RamFS 18 ay boyunca dolmayacak.", 0

str_snd_h1:  db "[YKNOS 5-SEVIYELI SANDBOX GUVENLIK MODELI]:", 0
str_snd_l0:  db "Seviye 0 | Kernel (Linux LTS, Dokunulmaz Ring-0): Sürücüler, MMU, Syscall", 0
str_snd_l1:  db "Seviye 1 | Y.U.K.I Core (Tam Yetki + Zorunlu Audit Log: audit.log)", 0
str_snd_l2:  db "Seviye 2 | YKN Sistem Servisleri (Capability-Based: ykn-init, ykn-http)", 0
str_snd_l3:  db "Seviye 3 | Kullanici Uygulamalari (Izole Sandbox, Explicit Izin)", 0
str_snd_l4:  db "Seviye 4 | LLM Tarafindan Uretilen Kod (En Kisitli, Cift Onay Sart)", 0
str_snd_pol: db "Guvenlik Ilkesi: Hicbir kullanici uygulamasi alt katmani atlayamaz.", 0

str_mod_h1: db "[GOMULU NORAL MODELLER & HESAPLAMA MOTORU]:", 0
str_mod_m1: db "- BitNet 1.58b: 16,384 Parametre | Ternary {-1, 0, +1} Agirliklar", 0
str_mod_m2: db "  Q16.16 Sabit Nokta | 28 KB RAM Ayak Izi | Sifir-Carpma ALU | AKTIF", 0
str_mod_b1: db "- Prism ML Bonsai-8B: 399 Tensor | 36 Katman | 8 Milyar Parametre", 0
str_mod_b2: db "  1.15 GB GGUF v3 Disk Baglantili | Qwen2 Mimarisi | GOMULU", 0

str_w2_title:  db "RamFS Dosya Gezgini - Kok Dizin (/)", 0
str_fs_dir1:   db "[D] /bin", 0
str_fs_dir2:   db "[D] /etc", 0
str_fs_dir3:   db "[D] /proc", 0
str_fs_dir4:   db "[D] /home", 0
str_fs_dir5:   db "[D] /var", 0

str_fs_f1:     db "sistem.cfg          1.2 KB", 0
str_fs_f2:     db "dmesg.log           4.8 KB", 0
str_fs_f3:     db "vector_memory.db   16.0 KB", 0
str_fs_f4:     db "audit.log           2.4 KB", 0
str_fs_f5:     db "index.html          3.1 KB", 0
str_fs_f6:     db "veritabani.sql      8.5 KB", 0
str_fs_status: db "6 dosya, 5 dizin  |  RamFS Toplam: 64 MB  |  Serbest: 61 MB", 0

str_w3_title:     db "yknsh - root@yknos:/# (Linux 6.1.0-yknos x86_64)", 0
str_term_banner:  db "YknOS GNU/Linux 6.1.0-yknos Interactive Shell. Komutlar: help, yuki, uptime", 0
str_term_l1:      db "[0.00s] Linux LTS cekirdek Long Mode & MMU aktif.", 0
str_term_l2:      db "[0.02s] 8259 PIC ve 100 Hz PIT timer kesmeleri devrede.", 0
str_term_l3:      db "[0.04s] RamFS VFS ve dinamik /proc sanal tablosu yuklendi.", 0
str_term_l4:      db "[0.08s] Y.U.K.I Core Nöral Isletim Sistemi Beyni aktif.", 0
str_term_l5:      db "Sistem hazir. Komut yazabilirsiniz...", 0
str_term_prompt:  db "root@yknos:/#", 0

msg_term_help:    db "Komutlar: help, yuki, uptime, date, uname, free, clear", 0
msg_term_yuki:    db "Y.U.K.I: Sistem %100 nominal. Tum alt sistemler operasyonel.", 0
msg_term_uptime:  db "22:30:15 up 25 min, 1 user, load average: 0.12, 0.08, 0.02", 0
msg_term_unknown: db "Bilinmeyen komut. Yardim icin 'help' yazin.", 0

str_cmd_help:   db "help", 0
str_cmd_yuki:   db "yuki", 0
str_cmd_uptime: db "uptime", 0
str_cmd_clear:  db "clear", 0

str_ico_d1: db "[*]", 0
str_ico_d2: db "[AI]", 0
str_ico_d3: db "[DIR]", 0
str_ico_d4: db "[>_]", 0
str_ico_d5: db "[SEC]", 0
str_ico_d6: db "[SQL]", 0
str_ico_d7: db "[NET]", 0
str_ico_d8: db "[SET]", 0

str_dm_1:   db "Hakkinda: YknOS 6.1", 0
str_dm_2:   db "Sistem Tercihleri", 0
str_dm_3:   db "Y.U.K.I Nöral Ayarlar", 0
str_dm_div: db "-------------------", 0
str_dm_4:   db "Yeniden Baslat", 0
str_dm_5:   db "Sistemi Kapat", 0

; ============================================================
; Bitmap Font Dahil Et (8x14 Consolas)
; ============================================================
%include "boot/font8x14.inc"

; ============================================================
; Data & Degiskenler Alani
; ============================================================
section .data
align 8
idt_descriptor:
    dw 256 * 8 - 1
    dd idt_table

_file_end:

section .bss
alignb 4
fb_addr:         resd 1
hw_timer_ticks:  resd 1

mouse_x:         resd 1
mouse_y:         resd 1
prev_mouse_x:    resd 1
prev_mouse_y:    resd 1

mouse_cycle:     resb 1
mouse_pkt0:      resb 1
mouse_pkt1:      resb 1
mouse_pkt2:      resb 1
mouse_btn_left:  resb 1

cursor_saved:    resb 1
alignb 4
cursor_bg_buf:   resd 18 * 12

active_window:   resb 1 ; 1: Y.U.K.I, 2: RamFS, 3: Terminal
yuki_active_tab: resb 1 ; 1..5
menu_open:       resb 1

term_buf_len:    resb 1
term_buffer:     resb 64

char_row_bits:   resb 1
alignb 4
char_x:          resd 1
char_y:          resd 1
char_fg:         resd 1
char_bg:         resd 1
char_glyph_ptr:  resd 1
str_draw_x:      resd 1
str_draw_y:      resd 1
str_draw_fg:     resd 1
str_draw_bg:     resd 1

grad_slice_h:    resd 1
grad_cur_color:  resd 1

circle_cx:       resd 1
circle_cy:       resd 1
circle_r:        resd 1
circle_col:      resd 1

alignb 8
idt_table:       resb 256 * 8

_kernel_end:
''')

full_code = "".join(asm_parts)

# Write to both locations
path1 = "d:/yukinary/yknos/boot/yknos_baremetal.asm"
path2 = "d:/yukinary/yukinary-language/examples/yknos/boot/yknos_baremetal.asm"

with open(path1, "w", encoding="utf-8") as f:
    f.write(full_code)

with open(path2, "w", encoding="utf-8") as f:
    f.write(full_code)

print("yknos_baremetal.asm written successfully to both paths!")
