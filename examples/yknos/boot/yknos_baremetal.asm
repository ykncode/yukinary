
; ============================================================
; YknOS Bare-Metal Ring-0 Linux-Compatible Kernel & Desktop GUI
; 1024x768x32bpp TrueColor Framebuffer + BGA + Hardware PS/2 Mouse & Keyboard
; Inspired by Windows 11, macOS, and modern Linux Desktop Environments
; Strict adherence to YKNOS_VISION.md:
;   - AI-First Interface: Spotlight-like Central Y.U.K.I Search Bar
;   - Window Manager (ykn-wm): Interactive Window Dragging, Z-Order, Acrylic Style
;   - Window 1: Y.U.K.I Core Neural OS Center (5 Tabs: Genel, Arkeoloji, Bakim, Sandbox, Modeller)
;   - Window 2: RamFS File Explorer (Dizin Agaci & Dosya Izgarasi)
;   - Window 3: yknsh Interactive Terminal (Bash/Linux emulator)
;   - Window 4: YknBrowser (Yukinary Modern Web Engine, HTTP 1.1 Live Client)
;   - macOS-style Traffic Lights (🔴 Close, 🟡 Minimize, 🟢 Maximize)
;   - Bottom Floating Glass Dock with Active App Indicators
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

    ; Pencere Baslangic Pozisyonlari
    mov dword [w1_x], 190
    mov dword [w1_y], 85
    mov dword [w1_w], 620
    mov dword [w1_h], 410
    mov byte [w1_state], 1

    mov dword [w2_x], 50
    mov dword [w2_y], 140
    mov dword [w2_w], 460
    mov dword [w2_h], 350
    mov byte [w2_state], 1

    mov dword [w3_x], 480
    mov dword [w3_y], 190
    mov dword [w3_w], 500
    mov dword [w3_h], 360
    mov byte [w3_state], 1

    mov dword [w4_x], 130
    mov dword [w4_y], 95
    mov dword [w4_w], 740
    mov dword [w4_h], 490
    mov byte [w4_state], 1

    ; Z-Order: Window 2 -> Window 3 -> Window 1 -> Window 4 (Browser on top!)
    mov byte [win_z_order + 0], 2
    mov byte [win_z_order + 1], 3
    mov byte [win_z_order + 2], 1
    mov byte [win_z_order + 3], 4
    mov byte [active_window], 4

    mov byte [yuki_active_tab], 1
    mov byte [browser_active_tab], 1
    mov byte [is_dragging], 0

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

; ------------------------------------------------------------
; Fare Paketi Isleme & Surukleme & Tiklama Yonetimi
; ------------------------------------------------------------
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

    ; X Delta
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

    ; Y Delta (PS/2 Y ters yondedir)
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

    ; Buton Durumlari: bl = onceki, dl = simdiki
    mov al, [mouse_pkt0]
    mov dl, al
    and dl, 0x01
    mov bl, [mouse_btn_left]
    mov [mouse_btn_left], dl

    ; 1. DURUM: FARE SURUKLEME DEVAM EDIYOR MU?
    cmp byte [is_dragging], 0
    je .check_button_transitions

    ; Fare basili tutulmaya devam ediyorsa surukle
    cmp dl, 1
    jne .drag_released

    ; Fare hareket etti mi?
    mov eax, [mouse_x]
    cmp eax, [prev_mouse_x]
    jne .do_drag_update
    mov eax, [mouse_y]
    cmp eax, [prev_mouse_y]
    jne .do_drag_update
    jmp .done

.do_drag_update:
    call update_window_drag
    jmp .done

.drag_released:
    mov byte [is_dragging], 0

.check_button_transitions:
    ; 2. DURUM: TIKLAMA OLAYI (Button Pressed 0 -> 1)
    cmp bl, 0
    jne .check_mouse_move
    cmp dl, 1
    jne .check_mouse_move

    ; Sol tiklama algilandi!
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    call gui_handle_click
    jmp .done

.check_mouse_move:
    ; Normal fare hareketi (Surukleme yokken hafif cursor yenileme)
    mov eax, [mouse_x]
    cmp eax, [prev_mouse_x]
    jne .redraw_cursor
    mov eax, [mouse_y]
    cmp eax, [prev_mouse_y]
    jne .redraw_cursor
    jmp .done

.redraw_cursor:
    call cursor_restore_bg
    call cursor_save_bg
    call cursor_draw
    jmp .done

.reset:
    mov byte [mouse_cycle], 0

.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; ------------------------------------------------------------
; Pencere Surukleme Konum Guncelleme
; ------------------------------------------------------------
update_window_drag:
    pushad
    movzx edx, byte [is_dragging]
    cmp edx, 1
    je .drag_w1
    cmp edx, 2
    je .drag_w2
    cmp edx, 3
    je .drag_w3
    cmp edx, 4
    je .drag_w4
    jmp .done_drag

.drag_w1:
    mov eax, [mouse_x]
    sub eax, [drag_off_x]
    ; Sinirlar: 0 <= x <= 1024 - w1_w
    cmp eax, 0
    jge .w1_x_ok
    xor eax, eax
.w1_x_ok:
    mov ecx, 1024
    sub ecx, [w1_w]
    cmp eax, ecx
    jle .w1_x_max
    mov eax, ecx
.w1_x_max:
    mov [w1_x], eax

    mov eax, [mouse_y]
    sub eax, [drag_off_y]
    ; Sinirlar: 28 <= y <= 712 - 28
    cmp eax, 28
    jge .w1_y_ok
    mov eax, 28
.w1_y_ok:
    cmp eax, 680
    jle .w1_y_max
    mov eax, 680
.w1_y_max:
    mov [w1_y], eax
    jmp .redraw_all_drag

.drag_w2:
    mov eax, [mouse_x]
    sub eax, [drag_off_x]
    cmp eax, 0
    jge .w2_x_ok
    xor eax, eax
.w2_x_ok:
    mov ecx, 1024
    sub ecx, [w2_w]
    cmp eax, ecx
    jle .w2_x_max
    mov eax, ecx
.w2_x_max:
    mov [w2_x], eax

    mov eax, [mouse_y]
    sub eax, [drag_off_y]
    cmp eax, 28
    jge .w2_y_ok
    mov eax, 28
.w2_y_ok:
    cmp eax, 680
    jle .w2_y_max
    mov eax, 680
.w2_y_max:
    mov [w2_y], eax
    jmp .redraw_all_drag

.drag_w3:
    mov eax, [mouse_x]
    sub eax, [drag_off_x]
    cmp eax, 0
    jge .w3_x_ok
    xor eax, eax
.w3_x_ok:
    mov ecx, 1024
    sub ecx, [w3_w]
    cmp eax, ecx
    jle .w3_x_max
    mov eax, ecx
.w3_x_max:
    mov [w3_x], eax

    mov eax, [mouse_y]
    sub eax, [drag_off_y]
    cmp eax, 28
    jge .w3_y_ok
    mov eax, 28
.w3_y_ok:
    cmp eax, 680
    jle .w3_y_max
    mov eax, 680
.w3_y_max:
    mov [w3_y], eax
    jmp .redraw_all_drag

.drag_w4:
    mov eax, [mouse_x]
    sub eax, [drag_off_x]
    cmp eax, 0
    jge .w4_x_ok
    xor eax, eax
.w4_x_ok:
    mov ecx, 1024
    sub ecx, [w4_w]
    cmp eax, ecx
    jle .w4_x_max
    mov eax, ecx
.w4_x_max:
    mov [w4_x], eax

    mov eax, [mouse_y]
    sub eax, [drag_off_y]
    cmp eax, 28
    jge .w4_y_ok
    mov eax, 28
.w4_y_ok:
    cmp eax, 680
    jle .w4_y_max
    mov eax, 680
.w4_y_max:
    mov [w4_y], eax

.redraw_all_drag:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw

.done_drag:
    popad
    ret

; ============================================================
; Z-Order (Pencere Siralama & En One Getirme)
; ============================================================
; AL = win_id (1..4)
bring_to_front:
    push eax
    push ecx
    push edx
    mov [active_window], al

    ; win_z_order dizisinde AL'nin indeksini bul
    mov ecx, 0
.find_idx:
    cmp ecx, 4
    jge .done_btf
    cmp byte [win_z_order + ecx], al
    je .found_idx
    inc ecx
    jmp .find_idx

.found_idx:
    ; ecx indeksindeki elemani en sona tasi
.shift_loop:
    cmp ecx, 3
    jge .set_last
    mov dl, [win_z_order + ecx + 1]
    mov [win_z_order + ecx], dl
    inc ecx
    jmp .shift_loop

.set_last:
    mov [win_z_order + 3], al

.done_btf:
    pop edx
    pop ecx
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

    ; Sayi tuslari ile sekme / pencere degisimi
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
    mov [prev_mouse_y], edx
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
    mov [prev_mouse_y], edx
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
    mov [prev_mouse_x], edx
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
    mov [prev_mouse_x], edx
    mov [mouse_x], edx
    call move_cursor_refresh
    jmp .done_kbd

.key_click:
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    call gui_handle_click
    jmp .done_kbd

.key_enter:
    call term_execute_cmd
    jmp .done_kbd

.key_backspace:
    call term_backspace
    jmp .done_kbd

.key_tab:
    ; Pencereler arasi gecis (1 -> 2 -> 3 -> 4 -> 1)
    mov al, [active_window]
    inc al
    cmp al, 4
    jle .set_win
    mov al, 1
.set_win:
    call bring_to_front
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd

.tab1:
    mov byte [yuki_active_tab], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab2:
    mov byte [yuki_active_tab], 2
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab3:
    mov byte [yuki_active_tab], 3
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab4:
    mov byte [yuki_active_tab], 4
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_kbd
.tab5:
    mov byte [yuki_active_tab], 5
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
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

    mov edi, [fb_addr]
    mov edx, ebx
    add edx, ebp
    cmp edx, 768
    jae .row_loop
    shl edx, 10
    add edx, eax
    cmp eax, 1024
    jae .row_loop
    lea edi, [edi + edx*4]

    push ecx
    push eax
    mov eax, esi
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

gfx_draw_rect_outline:
    push eax
    push ebx
    push ecx
    push edx
    push ebp

    push edx
    mov edx, 1
    call gfx_fill_rect
    pop edx

    push ebx
    add ebx, edx
    dec ebx
    push edx
    mov edx, 1
    call gfx_fill_rect
    pop edx
    pop ebx

    push ecx
    mov ecx, 1
    call gfx_fill_rect
    pop ecx

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

gfx_draw_circle_filled:
    pushad
    mov [circle_cx], eax
    mov [circle_cy], ebx
    mov [circle_r], ecx
    mov [circle_col], edx

    mov ebp, ecx
    neg ebp ; dy = -r
.loop_y:
    cmp ebp, [circle_r]
    jg .done_circle

    mov edi, [circle_r]
    neg edi ; dx = -r
.loop_x:
    cmp edi, [circle_r]
    jg .next_y

    mov eax, edi
    imul eax, edi
    mov ebx, ebp
    imul ebx, ebp
    add eax, ebx

    mov ebx, [circle_r]
    imul ebx, ebx
    cmp eax, ebx
    jg .skip_pixel

    mov eax, [circle_cx]
    add eax, edi
    mov ebx, [circle_cy]
    add ebx, ebp
    mov edx, [circle_col]
    call gfx_put_pixel

.skip_pixel:
    inc edi
    jmp .loop_x

.next_y:
    inc ebp
    jmp .loop_y

.done_circle:
    popad
    ret

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

    movzx edx, byte [char_row_bits]
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

    add dword [str_draw_x], 8
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
    mov ebx, 0
.save_row:
    cmp ebx, 18
    jge .done_save

    mov eax, 0
.save_col:
    cmp eax, 12
    jge .next_srow

    push eax
    push ebx

    add eax, esi
    add ebx, ebp
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

    mov ebx, 0
.crow:
    cmp ebx, 18
    jge .done_cdraw

    movzx ecx, word [cursor_bitmap + ebx*2]
    mov eax, 0
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

    ; Siyah golge
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

cursor_bitmap:
    dw 0x8000
    dw 0xC000
    dw 0xE000
    dw 0xF000
    dw 0xF800
    dw 0xFC00
    dw 0xFE00
    dw 0xFF00
    dw 0xFF80
    dw 0xFFC0
    dw 0xF800
    dw 0xD800
    dw 0x8C00
    dw 0x0C00
    dw 0x0600
    dw 0x0600
    dw 0x0300
    dw 0x0000

cursor_outline:
    dw 0xC000
    dw 0xE000
    dw 0xF000
    dw 0xF800
    dw 0xFC00
    dw 0xFE00
    dw 0xFF00
    dw 0xFF80
    dw 0xFFC0
    dw 0xFFE0
    dw 0xFFC0
    dw 0xFDC0
    dw 0xDFE0
    dw 0x8FE0
    dw 0x0FE0
    dw 0x07F0
    dw 0x07F0
    dw 0x0380

; ============================================================
; Masaustu Grafiksel Arayuz Katmani (Desktop GUI & Window Manager)
; ============================================================
gui_render_all:
    pushad

    ; 1. Masaustu Duvar Kagidi (Derin Dark Tema)
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

    ; Tech Grid Vurgulari
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
    mov eax, 74
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

    ; 3. Spotlight AI Arama Bari ("Baslangic Menusu Yerine AI")
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

    ; 4. Sol Masaustu Simgeleri
    call gui_draw_desktop_icons

    ; 5. Pencereler: Z-Order Sirasina Gore Ciz (win_z_order[0] -> [3])
    mov ecx, 0
.draw_win_loop:
    cmp ecx, 4
    jge .done_win_draw
    push ecx

    movzx eax, byte [win_z_order + ecx]
    cmp eax, 1
    je .call_w1
    cmp eax, 2
    je .call_w2
    cmp eax, 3
    je .call_w3
    cmp eax, 4
    je .call_w4
    jmp .next_w_draw

.call_w1:
    cmp byte [w1_state], 0
    je .next_w_draw
    call gui_draw_window_1
    jmp .next_w_draw

.call_w2:
    cmp byte [w2_state], 0
    je .next_w_draw
    call gui_draw_window_2
    jmp .next_w_draw

.call_w3:
    cmp byte [w3_state], 0
    je .next_w_draw
    call gui_draw_window_3
    jmp .next_w_draw

.call_w4:
    cmp byte [w4_state], 0
    je .next_w_draw
    call gui_draw_window_4

.next_w_draw:
    pop ecx
    inc ecx
    jmp .draw_win_loop

.done_win_draw:
    ; 6. Alt Dock (Her zaman pencerelerin ustunde!)
    call gui_draw_dock

    ; 7. Acik ise Baslat Menusu
    cmp byte [menu_open], 1
    jne .skip_menu
    call gui_draw_dropdown_menu
.skip_menu:

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

    ; Simge 4: Web Tarayici (YknBrowser)
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
    mov esi, 0x0088C0D0
    call gfx_draw_rect_outline
    mov esi, str_ico_web
    mov eax, 34
    mov ebx, 336
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_lbl_web
    mov eax, 18
    mov ebx, 376
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Simge 5: Sandbox
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
    mov esi, 0x00B48EAD
    call gfx_draw_rect_outline
    mov esi, str_ico_sec
    mov eax, 34
    mov ebx, 416
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    mov esi, str_lbl_sec
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
    pushad

    ; Golge
    mov eax, [w1_x]
    add eax, 4
    mov ebx, [w1_y]
    add ebx, 4
    mov ecx, [w1_w]
    mov edx, [w1_h]
    mov esi, 0x00080A10
    call gfx_fill_rect

    ; Govde
    mov eax, [w1_x]
    mov ebx, [w1_y]
    mov ecx, [w1_w]
    mov edx, [w1_h]
    mov esi, 0x001E1E2E
    call gfx_fill_rect

    ; Kenarlik
    mov eax, [w1_x]
    mov ebx, [w1_y]
    mov ecx, [w1_w]
    mov edx, [w1_h]
    mov esi, 0x003B4252
    cmp byte [active_window], 1
    jne .w1_outline
    mov esi, 0x005E81AC
.w1_outline:
    call gfx_draw_rect_outline

    ; Baslik Cubugu
    mov eax, [w1_x]
    mov ebx, [w1_y]
    mov ecx, [w1_w]
    mov edx, 28
    mov esi, 0x002E3440
    call gfx_fill_rect

    ; macOS Traffic Lights (🔴 🟡 🟢)
    mov eax, [w1_x]
    add eax, 14
    mov ebx, [w1_y]
    add ebx, 14
    mov ecx, 5
    mov edx, 0x00BF616A
    call gfx_draw_circle_filled

    mov eax, [w1_x]
    add eax, 30
    mov ebx, [w1_y]
    add ebx, 14
    mov ecx, 5
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled

    mov eax, [w1_x]
    add eax, 46
    mov ebx, [w1_y]
    add ebx, 14
    mov ecx, 5
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    ; Baslik Metni
    mov esi, str_w1_title
    mov eax, [w1_x]
    add eax, 64
    mov ebx, [w1_y]
    add ebx, 7
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sekme Cubugu (Tabs)
    mov eax, [w1_x]
    mov ebx, [w1_y]
    add ebx, 28
    mov ecx, [w1_w]
    mov edx, 24
    mov esi, 0x0024283B
    call gfx_fill_rect

    ; Sekme 1..5 Metinleri
    mov esi, str_tab_w1_1
    mov eax, [w1_x]
    add eax, 12
    mov ebx, [w1_y]
    add ebx, 34
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 1
    jne .t1_c
    mov edx, 0x0088C0D0
.t1_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_2
    mov eax, [w1_x]
    add eax, 108
    mov ebx, [w1_y]
    add ebx, 34
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 2
    jne .t2_c
    mov edx, 0x0088C0D0
.t2_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_3
    mov eax, [w1_x]
    add eax, 236
    mov ebx, [w1_y]
    add ebx, 34
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 3
    jne .t3_c
    mov edx, 0x0088C0D0
.t3_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_4
    mov eax, [w1_x]
    add eax, 330
    mov ebx, [w1_y]
    add ebx, 34
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 4
    jne .t4_c
    mov edx, 0x0088C0D0
.t4_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_tab_w1_5
    mov eax, [w1_x]
    add eax, 442
    mov ebx, [w1_y]
    add ebx, 34
    mov edx, 0x00D8DEE9
    cmp byte [yuki_active_tab], 5
    jne .t5_c
    mov edx, 0x0088C0D0
.t5_c:
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sekme Alt Cizgisi
    mov eax, [w1_x]
    mov ebx, [w1_y]
    add ebx, 51
    mov ecx, [w1_w]
    mov edx, 1
    mov esi, 0x003B4252
    call gfx_fill_rect

    ; Icerik Cizimi
    cmp byte [yuki_active_tab], 2
    je .draw_tab_2
    cmp byte [yuki_active_tab], 3
    je .draw_tab_3
    cmp byte [yuki_active_tab], 4
    je .draw_tab_4
    cmp byte [yuki_active_tab], 5
    je .draw_tab_5

    ; --- SEKME 1: GENEL ---
.draw_tab_1:
    mov esi, str_w1_status
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 64
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_w1_vm
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 86
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; QA Box
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 114
    mov ecx, [w1_w]
    sub ecx, 32
    mov edx, 180
    mov esi, 0x00141620
    call gfx_fill_rect

    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 114
    mov ecx, [w1_w]
    sub ecx, 32
    mov edx, 180
    mov esi, 0x00434C5E
    call gfx_draw_rect_outline

    mov esi, str_qa_prompt
    mov eax, [w1_x]
    add eax, 30
    mov ebx, [w1_y]
    add ebx, 128
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line1
    mov eax, [w1_x]
    add eax, 30
    mov ebx, [w1_y]
    add ebx, 152
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line2
    mov eax, [w1_x]
    add eax, 30
    mov ebx, [w1_y]
    add ebx, 170
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line3
    mov eax, [w1_x]
    add eax, 30
    mov ebx, [w1_y]
    add ebx, 188
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line4
    mov eax, [w1_x]
    add eax, 30
    mov ebx, [w1_y]
    add ebx, 206
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_qa_line5
    mov eax, [w1_x]
    add eax, 30
    mov ebx, [w1_y]
    add ebx, 224
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Hizli Butonlar
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 310
    mov ecx, 180
    mov edx, 32
    mov esi, 0x002E3440
    call gfx_fill_rect
    mov esi, str_btn_ark
    mov eax, [w1_x]
    add eax, 46
    mov ebx, [w1_y]
    add ebx, 320
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov eax, [w1_x]
    add eax, 210
    mov ebx, [w1_y]
    add ebx, 310
    mov ecx, 180
    mov edx, 32
    mov esi, 0x002E3440
    call gfx_fill_rect
    mov esi, str_btn_bakim
    mov eax, [w1_x]
    add eax, 240
    mov ebx, [w1_y]
    add ebx, 320
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov eax, [w1_x]
    add eax, 404
    mov ebx, [w1_y]
    add ebx, 310
    mov ecx, 200
    mov edx, 32
    mov esi, 0x002E3440
    call gfx_fill_rect
    mov esi, str_btn_sand
    mov eax, [w1_x]
    add eax, 434
    mov ebx, [w1_y]
    add ebx, 320
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    jmp .done_w1

.draw_tab_2:
    mov esi, str_ark_h1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 64
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 88
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l2
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 110
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l3
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 132
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l4
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 154
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_l5
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 176
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Arkeoloji Sonuc Kutusu
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 204
    mov ecx, [w1_w]
    sub ecx, 32
    mov edx, 90
    mov esi, 0x00161E28
    call gfx_fill_rect
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 204
    mov ecx, [w1_w]
    sub ecx, 32
    mov edx, 90
    mov esi, 0x00A3BE8C
    call gfx_draw_rect_outline

    mov esi, str_ark_h2
    mov eax, [w1_x]
    add eax, 28
    mov ebx, [w1_y]
    add ebx, 218
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_r1
    mov eax, [w1_x]
    add eax, 28
    mov ebx, [w1_y]
    add ebx, 240
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_ark_r2
    mov eax, [w1_x]
    add eax, 28
    mov ebx, [w1_y]
    add ebx, 262
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    jmp .done_w1

.draw_tab_3:
    mov esi, str_bak_h1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 64
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_l1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 92
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_l2
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 116
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_l3
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 140
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 172
    mov ecx, [w1_w]
    sub ecx, 32
    mov edx, 100
    mov esi, 0x0019202E
    call gfx_fill_rect
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 172
    mov ecx, [w1_w]
    sub ecx, 32
    mov edx, 100
    mov esi, 0x0088C0D0
    call gfx_draw_rect_outline

    mov esi, str_bak_h2
    mov eax, [w1_x]
    add eax, 28
    mov ebx, [w1_y]
    add ebx, 186
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_t1
    mov eax, [w1_x]
    add eax, 28
    mov ebx, [w1_y]
    add ebx, 210
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_bak_t2
    mov eax, [w1_x]
    add eax, 28
    mov ebx, [w1_y]
    add ebx, 234
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    jmp .done_w1

.draw_tab_4:
    mov esi, str_snd_h1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 64
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l0
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 90
    mov edx, 0x00BF616A
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 114
    mov edx, 0x00D08770
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l2
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 138
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l3
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 162
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_l4
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 186
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_snd_pol
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 220
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    jmp .done_w1

.draw_tab_5:
    mov esi, str_mod_h1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 64
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_m1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 92
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_m2
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 114
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_b1
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 148
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_mod_b2
    mov eax, [w1_x]
    add eax, 16
    mov ebx, [w1_y]
    add ebx, 170
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

.done_w1:
    popad
    ret

; ------------------------------------------------------------
; Pencere 2: RamFS Dosya Gezgini
; ------------------------------------------------------------
gui_draw_window_2:
    pushad

    mov eax, [w2_x]
    add eax, 4
    mov ebx, [w2_y]
    add ebx, 4
    mov ecx, [w2_w]
    mov edx, [w2_h]
    mov esi, 0x00080A10
    call gfx_fill_rect

    mov eax, [w2_x]
    mov ebx, [w2_y]
    mov ecx, [w2_w]
    mov edx, [w2_h]
    mov esi, 0x001B1E2E
    call gfx_fill_rect

    mov eax, [w2_x]
    mov ebx, [w2_y]
    mov ecx, [w2_w]
    mov edx, [w2_h]
    mov esi, 0x003B4252
    cmp byte [active_window], 2
    jne .w2_out
    mov esi, 0x00EBCB8B
.w2_out:
    call gfx_draw_rect_outline

    ; Baslik
    mov eax, [w2_x]
    mov ebx, [w2_y]
    mov ecx, [w2_w]
    mov edx, 26
    mov esi, 0x0024283B
    call gfx_fill_rect

    mov eax, [w2_x]
    add eax, 14
    mov ebx, [w2_y]
    add ebx, 13
    mov ecx, 4
    mov edx, 0x00BF616A
    call gfx_draw_circle_filled

    mov eax, [w2_x]
    add eax, 28
    mov ebx, [w2_y]
    add ebx, 13
    mov ecx, 4
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled

    mov eax, [w2_x]
    add eax, 42
    mov ebx, [w2_y]
    add ebx, 13
    mov ecx, 4
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    mov esi, str_w2_title
    mov eax, [w2_x]
    add eax, 56
    mov ebx, [w2_y]
    add ebx, 6
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sol Dizin Agaci
    mov eax, [w2_x]
    mov ebx, [w2_y]
    add ebx, 26
    mov ecx, 120
    mov edx, [w2_h]
    sub edx, 50
    mov esi, 0x00141624
    call gfx_fill_rect

    mov esi, str_fs_dir1
    mov eax, [w2_x]
    add eax, 12
    mov ebx, [w2_y]
    add ebx, 40
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_dir2
    mov eax, [w2_x]
    add eax, 12
    mov ebx, [w2_y]
    add ebx, 64
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_dir3
    mov eax, [w2_x]
    add eax, 12
    mov ebx, [w2_y]
    add ebx, 88
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_dir4
    mov eax, [w2_x]
    add eax, 12
    mov ebx, [w2_y]
    add ebx, 112
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_dir5
    mov eax, [w2_x]
    add eax, 12
    mov ebx, [w2_y]
    add ebx, 136
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sag Dosya Izgarasi
    mov esi, str_fs_f1
    mov eax, [w2_x]
    add eax, 136
    mov ebx, [w2_y]
    add ebx, 40
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_f2
    mov eax, [w2_x]
    add eax, 136
    mov ebx, [w2_y]
    add ebx, 64
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_f3
    mov eax, [w2_x]
    add eax, 136
    mov ebx, [w2_y]
    add ebx, 88
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_f4
    mov eax, [w2_x]
    add eax, 136
    mov ebx, [w2_y]
    add ebx, 112
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_f5
    mov eax, [w2_x]
    add eax, 136
    mov ebx, [w2_y]
    add ebx, 136
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_fs_f6
    mov eax, [w2_x]
    add eax, 136
    mov ebx, [w2_y]
    add ebx, 160
    mov edx, 0x00D08770
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Durum Cubugu
    mov eax, [w2_x]
    mov ebx, [w2_y]
    add ebx, [w2_h]
    sub ebx, 24
    mov ecx, [w2_w]
    mov edx, 24
    mov esi, 0x00141624
    call gfx_fill_rect

    mov esi, str_fs_status
    mov eax, [w2_x]
    add eax, 12
    mov ebx, [w2_y]
    add ebx, [w2_h]
    sub ebx, 18
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    popad
    ret

; ------------------------------------------------------------
; Pencere 3: yknsh Terminali
; ------------------------------------------------------------
gui_draw_window_3:
    pushad

    mov eax, [w3_x]
    add eax, 4
    mov ebx, [w3_y]
    add ebx, 4
    mov ecx, [w3_w]
    mov edx, [w3_h]
    mov esi, 0x00080A10
    call gfx_fill_rect

    mov eax, [w3_x]
    mov ebx, [w3_y]
    mov ecx, [w3_w]
    mov edx, [w3_h]
    mov esi, 0x000D1117
    call gfx_fill_rect

    mov eax, [w3_x]
    mov ebx, [w3_y]
    mov ecx, [w3_w]
    mov edx, [w3_h]
    mov esi, 0x003B4252
    cmp byte [active_window], 3
    jne .w3_out
    mov esi, 0x00A3BE8C
.w3_out:
    call gfx_draw_rect_outline

    ; Baslik
    mov eax, [w3_x]
    mov ebx, [w3_y]
    mov ecx, [w3_w]
    mov edx, 26
    mov esi, 0x00161B22
    call gfx_fill_rect

    mov eax, [w3_x]
    add eax, 14
    mov ebx, [w3_y]
    add ebx, 13
    mov ecx, 4
    mov edx, 0x00BF616A
    call gfx_draw_circle_filled

    mov eax, [w3_x]
    add eax, 28
    mov ebx, [w3_y]
    add ebx, 13
    mov ecx, 4
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled

    mov eax, [w3_x]
    add eax, 42
    mov ebx, [w3_y]
    add ebx, 13
    mov ecx, 4
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    mov esi, str_w3_title
    mov eax, [w3_x]
    add eax, 56
    mov ebx, [w3_y]
    add ebx, 6
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Terminal Ciktisi
    call term_render_lines

    popad
    ret

; ------------------------------------------------------------
; Pencere 4: YknBrowser (Yukinary Modern Web Engine)
; ------------------------------------------------------------
gui_draw_window_4:
    pushad

    ; Golge
    mov eax, [w4_x]
    add eax, 4
    mov ebx, [w4_y]
    add ebx, 4
    mov ecx, [w4_w]
    mov edx, [w4_h]
    mov esi, 0x00080A10
    call gfx_fill_rect

    ; Govde
    mov eax, [w4_x]
    mov ebx, [w4_y]
    mov ecx, [w4_w]
    mov edx, [w4_h]
    mov esi, 0x00131622
    call gfx_fill_rect

    ; Kenarlik
    mov eax, [w4_x]
    mov ebx, [w4_y]
    mov ecx, [w4_w]
    mov edx, [w4_h]
    mov esi, 0x003B4252
    cmp byte [active_window], 4
    jne .w4_out
    mov esi, 0x0088C0D0
.w4_out:
    call gfx_draw_rect_outline

    ; 1. Baslik Cubugu (y=0..27)
    mov eax, [w4_x]
    mov ebx, [w4_y]
    mov ecx, [w4_w]
    mov edx, 28
    mov esi, 0x00222638
    call gfx_fill_rect

    ; Traffic Lights 🔴 🟡 🟢
    mov eax, [w4_x]
    add eax, 14
    mov ebx, [w4_y]
    add ebx, 14
    mov ecx, 5
    mov edx, 0x00BF616A
    call gfx_draw_circle_filled

    mov eax, [w4_x]
    add eax, 30
    mov ebx, [w4_y]
    add ebx, 14
    mov ecx, 5
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled

    mov eax, [w4_x]
    add eax, 46
    mov ebx, [w4_y]
    add ebx, 14
    mov ecx, 5
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled

    ; Baslik
    mov esi, str_w4_title
    mov eax, [w4_x]
    add eax, 64
    mov ebx, [w4_y]
    add ebx, 7
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 2. Tarayici Sekme Cubugu (y=28..53, h=26)
    mov eax, [w4_x]
    mov ebx, [w4_y]
    add ebx, 28
    mov ecx, [w4_w]
    mov edx, 26
    mov esi, 0x001A1D2B
    call gfx_fill_rect

    ; Sekme 1 (Aktif Sekme)
    mov eax, [w4_x]
    add eax, 8
    mov ebx, [w4_y]
    add ebx, 28
    mov ecx, 155
    mov edx, 26
    mov esi, 0x00252A3D
    call gfx_fill_rect

    ; Sekme 1 Alt Vurgu
    mov eax, [w4_x]
    add eax, 8
    mov ebx, [w4_y]
    add ebx, 52
    mov ecx, 155
    mov edx, 2
    mov esi, 0x0088C0D0
    call gfx_fill_rect

    mov esi, str_web_tab1
    mov eax, [w4_x]
    add eax, 16
    mov ebx, [w4_y]
    add ebx, 34
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sekme 2
    mov esi, str_web_tab2
    mov eax, [w4_x]
    add eax, 172
    mov ebx, [w4_y]
    add ebx, 34
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sekme 3
    mov esi, str_web_tab3
    mov eax, [w4_x]
    add eax, 332
    mov ebx, [w4_y]
    add ebx, 34
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sekme 4 (+)
    mov esi, str_web_tab4
    mov eax, [w4_x]
    add eax, 452
    mov ebx, [w4_y]
    add ebx, 34
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 3. Navigasyon ve Omnibox Adres Cubugu (y=54..87, h=34)
    mov eax, [w4_x]
    mov ebx, [w4_y]
    add ebx, 54
    mov ecx, [w4_w]
    mov edx, 34
    mov esi, 0x001D2130
    call gfx_fill_rect

    ; Navigasyon Butonlari (<, >, R, H)
    ; Buton <
    mov eax, [w4_x]
    add eax, 10
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x00252A3D
    call gfx_fill_rect
    mov eax, [w4_x]
    add eax, 10
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x003B4252
    call gfx_draw_rect_outline
    mov esi, str_nav_back
    mov eax, [w4_x]
    add eax, 18
    mov ebx, [w4_y]
    add ebx, 64
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Buton >
    mov eax, [w4_x]
    add eax, 38
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x00252A3D
    call gfx_fill_rect
    mov eax, [w4_x]
    add eax, 38
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x003B4252
    call gfx_draw_rect_outline
    mov esi, str_nav_fwd
    mov eax, [w4_x]
    add eax, 46
    mov ebx, [w4_y]
    add ebx, 64
    mov edx, 0x004C566A
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Buton R (Reload)
    mov eax, [w4_x]
    add eax, 66
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x00252A3D
    call gfx_fill_rect
    mov eax, [w4_x]
    add eax, 66
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x003B4252
    call gfx_draw_rect_outline
    mov esi, str_nav_reload
    mov eax, [w4_x]
    add eax, 74
    mov ebx, [w4_y]
    add ebx, 64
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Buton H (Home)
    mov eax, [w4_x]
    add eax, 94
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x00252A3D
    call gfx_fill_rect
    mov eax, [w4_x]
    add eax, 94
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 24
    mov edx, 26
    mov esi, 0x003B4252
    call gfx_draw_rect_outline
    mov esi, str_nav_home
    mov eax, [w4_x]
    add eax, 102
    mov ebx, [w4_y]
    add ebx, 64
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Omnibox URL Alani (🔒 http://127.0.0.1:8080/index.html)
    mov eax, [w4_x]
    add eax, 124
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 436
    mov edx, 26
    mov esi, 0x00121420
    call gfx_fill_rect

    mov eax, [w4_x]
    add eax, 124
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 436
    mov edx, 26
    mov esi, 0x00434C5E
    call gfx_draw_rect_outline

    mov esi, str_web_url
    mov eax, [w4_x]
    add eax, 134
    mov ebx, [w4_y]
    add ebx, 64
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; AI Ozetle Aksiyon Butonu
    mov eax, [w4_x]
    add eax, 570
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 110
    mov edx, 26
    mov esi, 0x002E3440
    call gfx_fill_rect

    mov eax, [w4_x]
    add eax, 570
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 110
    mov edx, 26
    mov esi, 0x005E81AC
    call gfx_draw_rect_outline

    mov esi, str_btn_summarize
    mov eax, [w4_x]
    add eax, 578
    mov ebx, [w4_y]
    add ebx, 64
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Sandbox L3 Guvenlik Simgesi
    mov eax, [w4_x]
    add eax, 688
    mov ebx, [w4_y]
    add ebx, 58
    mov ecx, 42
    mov edx, 26
    mov esi, 0x0024283B
    call gfx_fill_rect

    mov esi, str_ico_l3
    mov eax, [w4_x]
    add eax, 694
    mov ebx, [w4_y]
    add ebx, 64
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 4. Web Icerik Govdesi (HTML Viewport)
    mov eax, [w4_x]
    mov ebx, [w4_y]
    add ebx, 88
    mov ecx, [w4_w]
    mov edx, [w4_h]
    sub edx, 110
    mov esi, 0x000E1018
    call gfx_fill_rect

    ; Web Hero Banner (Kart 0)
    mov eax, [w4_x]
    add eax, 12
    mov ebx, [w4_y]
    add ebx, 98
    mov ecx, [w4_w]
    sub ecx, 24
    mov edx, 54
    mov esi, 0x00171A26
    call gfx_fill_rect

    mov eax, [w4_x]
    add eax, 12
    mov ebx, [w4_y]
    add ebx, 98
    mov ecx, [w4_w]
    sub ecx, 24
    mov edx, 54
    mov esi, 0x003B4252
    call gfx_draw_rect_outline

    mov esi, str_hero_t1
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 106
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_hero_t2
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 126
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Kart 1: RamFS REST Mikroservisleri
    mov eax, [w4_x]
    add eax, 12
    mov ebx, [w4_y]
    add ebx, 160
    mov ecx, [w4_w]
    sub ecx, 24
    mov edx, 92
    mov esi, 0x00141622
    call gfx_fill_rect

    mov eax, [w4_x]
    add eax, 12
    mov ebx, [w4_y]
    add ebx, 160
    mov ecx, [w4_w]
    sub ecx, 24
    mov edx, 92
    mov esi, 0x002E3440
    call gfx_draw_rect_outline

    mov esi, str_c1_hdr
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 168
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_c1_l1
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 188
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_c1_l2
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 208
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_c1_l3
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 228
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Kart 2: Y.U.K.I Guvenlik Duvari & Derin Paket Filtresi
    mov eax, [w4_x]
    add eax, 12
    mov ebx, [w4_y]
    add ebx, 260
    mov ecx, [w4_w]
    sub ecx, 24
    mov edx, 74
    mov esi, 0x00141622
    call gfx_fill_rect

    mov eax, [w4_x]
    add eax, 12
    mov ebx, [w4_y]
    add ebx, 260
    mov ecx, [w4_w]
    sub ecx, 24
    mov edx, 74
    mov esi, 0x002E3440
    call gfx_draw_rect_outline

    mov esi, str_c2_hdr
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 268
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_c2_l1
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 288
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_c2_l2
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 308
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Hizli Baglantilar / Aksiyon Bari
    mov esi, str_web_links
    mov eax, [w4_x]
    add eax, 24
    mov ebx, [w4_y]
    add ebx, 344
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 5. Tarayici Durum Cubugu (Browser Status Bar)
    mov eax, [w4_x]
    mov ebx, [w4_y]
    add ebx, [w4_h]
    sub ebx, 22
    mov ecx, [w4_w]
    mov edx, 22
    mov esi, 0x00161824
    call gfx_fill_rect

    mov eax, [w4_x]
    mov ebx, [w4_y]
    add ebx, [w4_h]
    sub ebx, 22
    mov ecx, [w4_w]
    mov edx, 1
    mov esi, 0x002E3440
    call gfx_fill_rect

    mov esi, str_web_status
    mov eax, [w4_x]
    add eax, 12
    mov ebx, [w4_y]
    add ebx, [w4_h]
    sub ebx, 17
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    popad
    ret

; ------------------------------------------------------------
; Alt Dock (Floating Acrylic Capsule)
; ------------------------------------------------------------
gui_draw_dock:
    ; Dock Paneli (x=246, y=712, w=532, h=46)
    mov eax, 246
    mov ebx, 712
    mov ecx, 532
    mov edx, 46
    mov esi, 0x00181A26
    call gfx_fill_rect

    mov eax, 246
    mov ebx, 712
    mov ecx, 532
    mov edx, 46
    mov esi, 0x004C566A
    call gfx_draw_rect_outline

    ; 1: Baslat
    mov esi, str_ico_d1
    mov eax, 260
    mov ebx, 726
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 2: Y.U.K.I AI
    mov esi, str_ico_d2
    mov eax, 324
    mov ebx, 726
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    cmp byte [w1_state], 0
    je .skip_dot1
    mov eax, 336
    mov ebx, 746
    mov ecx, 2
    mov edx, 0x0088C0D0
    call gfx_draw_circle_filled
.skip_dot1:

    ; 3: Dosyalar (RamFS)
    mov esi, str_ico_d3
    mov eax, 388
    mov ebx, 726
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    cmp byte [w2_state], 0
    je .skip_dot2
    mov eax, 400
    mov ebx, 746
    mov ecx, 2
    mov edx, 0x00EBCB8B
    call gfx_draw_circle_filled
.skip_dot2:

    ; 4: Terminal
    mov esi, str_ico_d4
    mov eax, 452
    mov ebx, 726
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    cmp byte [w3_state], 0
    je .skip_dot3
    mov eax, 464
    mov ebx, 746
    mov ecx, 2
    mov edx, 0x00A3BE8C
    call gfx_draw_circle_filled
.skip_dot3:

    ; 5: Web Tarayici (YknBrowser)
    mov esi, str_ico_d5
    mov eax, 516
    mov ebx, 726
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string
    cmp byte [w4_state], 0
    je .skip_dot4
    mov eax, 528
    mov ebx, 746
    mov ecx, 2
    mov edx, 0x0088C0D0
    call gfx_draw_circle_filled
.skip_dot4:

    ; 6: Sandbox
    mov esi, str_ico_d6
    mov eax, 580
    mov ebx, 726
    mov edx, 0x00B48EAD
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 7: SQL
    mov esi, str_ico_d7
    mov eax, 644
    mov ebx, 726
    mov edx, 0x00D08770
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; 8: Ayarlar
    mov esi, str_ico_d8
    mov eax, 708
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
    mov al, 1
    call bring_to_front
    mov byte [w1_state], 1
    mov byte [yuki_active_tab], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_dock:
    ; 3. Alt Dock Tiklamasi (y=712..758, x=246..778)
    cmp ebx, 712
    jb .check_windows_hit
    cmp ebx, 758
    ja .check_windows_hit
    cmp eax, 246
    jb .check_windows_hit
    cmp eax, 778
    ja .check_windows_hit

    ; Dock Simgesi Hesabi: index = (x - 246) / 64
    sub eax, 246
    mov edx, 0
    mov ecx, 64
    div ecx ; EAX = 0..7

    cmp eax, 0
    je .dock_menu
    cmp eax, 1
    je .dock_yuki
    cmp eax, 2
    je .dock_fs
    cmp eax, 3
    je .dock_term
    cmp eax, 4
    je .dock_browser
    cmp eax, 5
    je .dock_sec
    jmp .done_click

.dock_menu:
    xor byte [menu_open], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.dock_yuki:
    mov al, 1
    call toggle_or_focus_win
    jmp .done_click

.dock_fs:
    mov al, 2
    call toggle_or_focus_win
    jmp .done_click

.dock_term:
    mov al, 3
    call toggle_or_focus_win
    jmp .done_click

.dock_browser:
    mov al, 4
    call toggle_or_focus_win
    jmp .done_click

.dock_sec:
    mov al, 1
    call bring_to_front
    mov byte [w1_state], 1
    mov byte [yuki_active_tab], 4
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

; ------------------------------------------------------------
; Z-Order Hit Testing: En Ustteki Pencereden En Alta Dogru (3 -> 0)
; ------------------------------------------------------------
.check_windows_hit:
    mov ecx, 3
.hit_loop:
    cmp ecx, 0
    jl .check_desktop_icons_hit
    push ecx

    movzx edx, byte [win_z_order + ecx] ; edx = win_id (1..4)
    cmp edx, 1
    je .test_w1
    cmp edx, 2
    je .test_w2
    cmp edx, 3
    je .test_w3
    cmp edx, 4
    je .test_w4
    jmp .next_hit_cand

.test_w1:
    cmp byte [w1_state], 0
    je .next_hit_cand
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    cmp eax, [w1_x]
    jb .next_hit_cand
    mov esi, [w1_x]
    add esi, [w1_w]
    cmp eax, esi
    jae .next_hit_cand
    cmp ebx, [w1_y]
    jb .next_hit_cand
    mov esi, [w1_y]
    add esi, [w1_h]
    cmp ebx, esi
    jae .next_hit_cand

    ; Pencere 1 Vuruldu!
    pop ecx
    mov al, 1
    call bring_to_front
    call handle_click_w1
    jmp .done_click

.test_w2:
    cmp byte [w2_state], 0
    je .next_hit_cand
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    cmp eax, [w2_x]
    jb .next_hit_cand
    mov esi, [w2_x]
    add esi, [w2_w]
    cmp eax, esi
    jae .next_hit_cand
    cmp ebx, [w2_y]
    jb .next_hit_cand
    mov esi, [w2_y]
    add esi, [w2_h]
    cmp ebx, esi
    jae .next_hit_cand

    ; Pencere 2 Vuruldu!
    pop ecx
    mov al, 2
    call bring_to_front
    call handle_click_w2
    jmp .done_click

.test_w3:
    cmp byte [w3_state], 0
    je .next_hit_cand
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    cmp eax, [w3_x]
    jb .next_hit_cand
    mov esi, [w3_x]
    add esi, [w3_w]
    cmp eax, esi
    jae .next_hit_cand
    cmp ebx, [w3_y]
    jb .next_hit_cand
    mov esi, [w3_y]
    add esi, [w3_h]
    cmp ebx, esi
    jae .next_hit_cand

    ; Pencere 3 Vuruldu!
    pop ecx
    mov al, 3
    call bring_to_front
    call handle_click_w3
    jmp .done_click

.test_w4:
    cmp byte [w4_state], 0
    je .next_hit_cand
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    cmp eax, [w4_x]
    jb .next_hit_cand
    mov esi, [w4_x]
    add esi, [w4_w]
    cmp eax, esi
    jae .next_hit_cand
    cmp ebx, [w4_y]
    jb .next_hit_cand
    mov esi, [w4_y]
    add esi, [w4_h]
    cmp ebx, esi
    jae .next_hit_cand

    ; Pencere 4 Vuruldu!
    pop ecx
    mov al, 4
    call bring_to_front
    call handle_click_w4
    jmp .done_click

.next_hit_cand:
    pop ecx
    dec ecx
    jmp .hit_loop

.check_desktop_icons_hit:
    ; Sol Masaustu Simgeleri Tiklamasi (x=24..76)
    mov eax, [mouse_x]
    mov ebx, [mouse_y]
    cmp eax, 24
    jb .done_click
    cmp eax, 76
    ja .done_click

    ; Simge 1: Y.U.K.I (y=80..132)
    cmp ebx, 80
    jb .done_click
    cmp ebx, 132
    ja .check_dico_2
    mov al, 1
    call bring_to_front
    mov byte [w1_state], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_dico_2:
    ; Simge 2: RamFS (y=160..212)
    cmp ebx, 160
    jb .done_click
    cmp ebx, 212
    ja .check_dico_3
    mov al, 2
    call bring_to_front
    mov byte [w2_state], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_dico_3:
    ; Simge 3: Terminal (y=240..292)
    cmp ebx, 240
    jb .done_click
    cmp ebx, 292
    ja .check_dico_4
    mov al, 3
    call bring_to_front
    mov byte [w3_state], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    jmp .done_click

.check_dico_4:
    ; Simge 4: Web Tarayici (y=320..372)
    cmp ebx, 320
    jb .done_click
    cmp ebx, 372
    ja .done_click
    mov al, 4
    call bring_to_front
    mov byte [w4_state], 1
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

; ------------------------------------------------------------
; Pencere 1 Olay Isleme (Baslik, Surukleme, Sekmeler, Butonlar)
; ------------------------------------------------------------
handle_click_w1:
    mov eax, [mouse_x]
    mov ebx, [mouse_y]

    ; Baslik cubugu mu? (y < w1_y + 28)
    mov esi, [w1_y]
    add esi, 28
    cmp ebx, esi
    jae .w1_body_click

    ; Traffic Lights kontrolu
    ; 🔴 Kapat (w1_x + 8 .. w1_x + 20)
    mov edx, [w1_x]
    add edx, 8
    cmp eax, edx
    jb .start_drag_w1
    add edx, 14
    cmp eax, edx
    ja .check_w1_min
    mov byte [w1_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.check_w1_min:
    ; 🟡 Simge Durumu (w1_x + 24 .. w1_x + 36)
    mov edx, [w1_x]
    add edx, 24
    cmp eax, edx
    jb .start_drag_w1
    add edx, 14
    cmp eax, edx
    ja .start_drag_w1
    mov byte [w1_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.start_drag_w1:
    ; Pencereyi Suruklemeye Basla!
    mov byte [is_dragging], 1
    mov eax, [mouse_x]
    sub eax, [w1_x]
    mov [drag_off_x], eax
    mov eax, [mouse_y]
    sub eax, [w1_y]
    mov [drag_off_y], eax
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.w1_body_click:
    ; Sekme Cubugu Tiklamasi (y < w1_y + 52)
    mov esi, [w1_y]
    add esi, 52
    cmp ebx, esi
    ja .w1_content_click

    ; Sekme X Koordinatlari
    mov eax, [mouse_x]
    sub eax, [w1_x]
    cmp eax, 100
    jb .set_t1
    cmp eax, 220
    jb .set_t2
    cmp eax, 320
    jb .set_t3
    cmp eax, 430
    jb .set_t4
    mov byte [yuki_active_tab], 5
    jmp .refresh_w1

.set_t1:
    mov byte [yuki_active_tab], 1
    jmp .refresh_w1
.set_t2:
    mov byte [yuki_active_tab], 2
    jmp .refresh_w1
.set_t3:
    mov byte [yuki_active_tab], 3
    jmp .refresh_w1
.set_t4:
    mov byte [yuki_active_tab], 4

.refresh_w1:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.w1_content_click:
    ; Sekme 1 Hizli Butonlar (y = w1_y + 310 .. w1_y + 342)
    cmp byte [yuki_active_tab], 1
    jne .w1_just_focus
    mov esi, [w1_y]
    add esi, 310
    cmp ebx, esi
    jb .w1_just_focus
    add esi, 32
    cmp ebx, esi
    ja .w1_just_focus

    mov eax, [mouse_x]
    sub eax, [w1_x]
    cmp eax, 16
    jb .w1_just_focus
    cmp eax, 196
    jbe .btn_to_t2
    cmp eax, 210
    jb .w1_just_focus
    cmp eax, 390
    jbe .btn_to_t3
    cmp eax, 404
    jb .w1_just_focus
    cmp eax, 604
    jbe .btn_to_t4
    jmp .w1_just_focus

.btn_to_t2:
    mov byte [yuki_active_tab], 2
    jmp .refresh_w1
.btn_to_t3:
    mov byte [yuki_active_tab], 3
    jmp .refresh_w1
.btn_to_t4:
    mov byte [yuki_active_tab], 4
    jmp .refresh_w1

.w1_just_focus:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

; ------------------------------------------------------------
; Pencere 2 Olay Isleme (RamFS)
; ------------------------------------------------------------
handle_click_w2:
    mov eax, [mouse_x]
    mov ebx, [mouse_y]

    mov esi, [w2_y]
    add esi, 26
    cmp ebx, esi
    jae .w2_body_click

    ; Traffic Lights
    mov edx, [w2_x]
    add edx, 8
    cmp eax, edx
    jb .start_drag_w2
    add edx, 14
    cmp eax, edx
    ja .check_w2_min
    mov byte [w2_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.check_w2_min:
    mov edx, [w2_x]
    add edx, 24
    cmp eax, edx
    jb .start_drag_w2
    add edx, 14
    cmp eax, edx
    ja .start_drag_w2
    mov byte [w2_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.start_drag_w2:
    mov byte [is_dragging], 2
    mov eax, [mouse_x]
    sub eax, [w2_x]
    mov [drag_off_x], eax
    mov eax, [mouse_y]
    sub eax, [w2_y]
    mov [drag_off_y], eax
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.w2_body_click:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

; ------------------------------------------------------------
; Pencere 3 Olay Isleme (Terminal)
; ------------------------------------------------------------
handle_click_w3:
    mov eax, [mouse_x]
    mov ebx, [mouse_y]

    mov esi, [w3_y]
    add esi, 26
    cmp ebx, esi
    jae .w3_body_click

    ; Traffic Lights
    mov edx, [w3_x]
    add edx, 8
    cmp eax, edx
    jb .start_drag_w3
    add edx, 14
    cmp eax, edx
    ja .check_w3_min
    mov byte [w3_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.check_w3_min:
    mov edx, [w3_x]
    add edx, 24
    cmp eax, edx
    jb .start_drag_w3
    add edx, 14
    cmp eax, edx
    ja .start_drag_w3
    mov byte [w3_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.start_drag_w3:
    mov byte [is_dragging], 3
    mov eax, [mouse_x]
    sub eax, [w3_x]
    mov [drag_off_x], eax
    mov eax, [mouse_y]
    sub eax, [w3_y]
    mov [drag_off_y], eax
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.w3_body_click:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

; ------------------------------------------------------------
; Pencere 4 Olay Isleme (YknBrowser)
; ------------------------------------------------------------
handle_click_w4:
    mov eax, [mouse_x]
    mov ebx, [mouse_y]

    mov esi, [w4_y]
    add esi, 28
    cmp ebx, esi
    jae .w4_body_click

    ; Traffic Lights
    mov edx, [w4_x]
    add edx, 8
    cmp eax, edx
    jb .start_drag_w4
    add edx, 14
    cmp eax, edx
    ja .check_w4_min
    mov byte [w4_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.check_w4_min:
    mov edx, [w4_x]
    add edx, 24
    cmp eax, edx
    jb .start_drag_w4
    add edx, 14
    cmp eax, edx
    ja .start_drag_w4
    mov byte [w4_state], 0
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.start_drag_w4:
    mov byte [is_dragging], 4
    mov eax, [mouse_x]
    sub eax, [w4_x]
    mov [drag_off_x], eax
    mov eax, [mouse_y]
    sub eax, [w4_y]
    mov [drag_off_y], eax
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.w4_body_click:
    ; Tarayici Sekme Tiklamasi (y < w4_y + 54)
    mov esi, [w4_y]
    add esi, 54
    cmp ebx, esi
    ja .w4_toolbar_click

    mov eax, [mouse_x]
    sub eax, [w4_x]
    cmp eax, 170
    jb .set_wtab1
    cmp eax, 330
    jb .set_wtab2
    cmp eax, 450
    jb .set_wtab3
    mov byte [browser_active_tab], 4
    jmp .refresh_w4

.set_wtab1:
    mov byte [browser_active_tab], 1
    jmp .refresh_w4
.set_wtab2:
    mov byte [browser_active_tab], 2
    jmp .refresh_w4
.set_wtab3:
    mov byte [browser_active_tab], 3

.refresh_w4:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.w4_toolbar_click:
    ; Sayfayi Ozetle Butonu Tiklamasi (y < w4_y + 88)
    mov esi, [w4_y]
    add esi, 88
    cmp ebx, esi
    ja .w4_content_click

    mov eax, [mouse_x]
    sub eax, [w4_x]
    cmp eax, 570
    jb .w4_content_click
    cmp eax, 680
    ja .w4_content_click

    ; AI Ozetleme Yapildi Bildirimi: Y.U.K.I penceresine gec
    mov al, 1
    call bring_to_front
    mov byte [w1_state], 1
    mov byte [yuki_active_tab], 1
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

.w4_content_click:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
    ret

; ------------------------------------------------------------
; Dock Simgesi Tiklandiginda Pencere Ac/Odaklan
; ------------------------------------------------------------
toggle_or_focus_win:
    ; AL = win_id (1..4)
    push eax
    call bring_to_front
    pop eax
    cmp al, 1
    je .t_w1
    cmp al, 2
    je .t_w2
    cmp al, 3
    je .t_w3
    cmp al, 4
    je .t_w4
    ret

.t_w1:
    mov byte [w1_state], 1
    jmp .redraw_toggle
.t_w2:
    mov byte [w2_state], 1
    jmp .redraw_toggle
.t_w3:
    mov byte [w3_state], 1
    jmp .redraw_toggle
.t_w4:
    mov byte [w4_state], 1

.redraw_toggle:
    call gui_render_all
    call cursor_save_bg
    call cursor_draw
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
    mov eax, [w3_x]
    add eax, 2
    mov ebx, [w3_y]
    add ebx, 28
    mov ecx, [w3_w]
    sub ecx, 4
    mov edx, [w3_h]
    sub edx, 30
    mov esi, 0x000D1117
    call gfx_fill_rect

    ; Sabit Banner
    mov esi, str_term_banner
    mov eax, [w3_x]
    add eax, 12
    mov ebx, [w3_y]
    add ebx, 36
    mov edx, 0x0088C0D0
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Onceki Cikti Satirlari
    mov esi, str_term_l1
    mov eax, [w3_x]
    add eax, 12
    mov ebx, [w3_y]
    add ebx, 58
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l2
    mov eax, [w3_x]
    add eax, 12
    mov ebx, [w3_y]
    add ebx, 78
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l3
    mov eax, [w3_x]
    add eax, 12
    mov ebx, [w3_y]
    add ebx, 98
    mov edx, 0x00EBCB8B
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l4
    mov eax, [w3_x]
    add eax, 12
    mov ebx, [w3_y]
    add ebx, 118
    mov edx, 0x00D8DEE9
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, str_term_l5
    mov eax, [w3_x]
    add eax, 12
    mov ebx, [w3_y]
    add ebx, 138
    mov edx, 0x0081A1C1
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Prompt ve Kullanici Girdisi
    mov esi, str_term_prompt
    mov eax, [w3_x]
    add eax, 12
    mov ebx, [w3_y]
    add ebx, 166
    mov edx, 0x00A3BE8C
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    mov esi, term_buffer
    mov eax, [w3_x]
    add eax, 116
    mov ebx, [w3_y]
    add ebx, 166
    mov edx, 0x00ECEFF4
    mov ecx, 0xFFFFFFFF
    call gfx_draw_string

    ; Yanip Sonen Imlec
    mov eax, [w3_x]
    add eax, 116
    movzx ecx, byte [term_buf_len]
    shl ecx, 3
    add eax, ecx
    mov ebx, [w3_y]
    add ebx, 166
    mov ecx, 8
    mov edx, 14
    mov esi, 0x00A3BE8C
    call gfx_fill_rect
    ret

term_execute_cmd:
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
    mov edi, str_cmd_web
    call strcmp
    jz .do_web_cmd

    mov esi, term_buffer
    mov edi, str_cmd_clear
    call strcmp
    jz .do_clear_cmd

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

.do_web_cmd:
    mov esi, msg_term_web
    call copy_to_term_l5
    mov al, 4
    call bring_to_front
    mov byte [w4_state], 1
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
                  db "  Linux LTS Kernel + YKN Userspace + YknBrowser Live Web Engine ", 10
                  db "================================================================", 10, 0
msg_serial_ready: db "[YknOS]: Hardware PS/2 Mouse, Keyboard, BGA & PIT 100Hz ONLINE.", 10, 0

str_menu_brand:   db "YknOS", 0
str_menu_items:   db "Dosya  Duzenle  Gorunum  Pencere  Yardim", 0
str_os_title_top: db "YknOS 6.1 LTS | Web & AI Desktop", 0
str_tray_items:   db "WiFi [ON] | Sandbox L0-L4 | 22:30", 0

str_spotlight_text: db "Y.U.K.I: 'Sistem sagligi nasil?', 'tarayici ac', 'yuki arkeoloji'...", 0

str_ico_ai:   db "[AI]", 0
str_lbl_ai:   db "Y.U.K.I", 0
str_ico_fs:   db "[DIR]", 0
str_lbl_fs:   db "RamFS", 0
str_ico_term: db "[>_]", 0
str_lbl_term: db "Terminal", 0
str_ico_web:  db "[WEB]", 0
str_lbl_web:  db "Browser", 0
str_ico_sec:  db "[SEC]", 0
str_lbl_sec:  db "Sandbox", 0

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
str_term_banner:  db "YknOS GNU/Linux 6.1 Interactive Shell. Komutlar: help, yuki, web, uptime", 0
str_term_l1:      db "[0.00s] Linux LTS cekirdek Long Mode & MMU aktif.", 0
str_term_l2:      db "[0.02s] 8259 PIC ve 100 Hz PIT timer kesmeleri devrede.", 0
str_term_l3:      db "[0.04s] RamFS VFS ve dinamik /proc sanal tablosu yuklendi.", 0
str_term_l4:      db "[0.08s] Y.U.K.I Core Noral Isletim Sistemi Beyni aktif.", 0
str_term_l5:      db "Sistem hazir. Komut yazabilirsiniz...", 0
str_term_prompt:  db "root@yknos:/#", 0

msg_term_help:    db "Komutlar: help, yuki, web, uptime, date, uname, free, clear", 0
msg_term_yuki:    db "Y.U.K.I: Sistem %100 nominal. Tum alt sistemler operasyonel.", 0
msg_term_uptime:  db "22:30:15 up 25 min, 1 user, load average: 0.12, 0.08, 0.02", 0
msg_term_web:     db "[HTTP]: YknBrowser penceresi odaklandi -> http://127.0.0.1:8080", 0
msg_term_unknown: db "Bilinmeyen komut. Yardim icin 'help' yazin.", 0

str_cmd_help:   db "help", 0
str_cmd_yuki:   db "yuki", 0
str_cmd_uptime: db "uptime", 0
str_cmd_web:    db "web", 0
str_cmd_clear:  db "clear", 0

; Window 4 (YknBrowser) Metinleri
str_w4_title:       db "YknBrowser - Yukinary Modern Web Engine (HTTP 1.1 Live Client)", 0
str_web_tab1:       db "[ YknOS Portali ]", 0
str_web_tab2:       db "[ Y.U.K.I Search ]", 0
str_web_tab3:       db "[ REST API ]", 0
str_web_tab4:       db "[ + ]", 0

str_nav_back:       db "<", 0
str_nav_fwd:        db ">", 0
str_nav_reload:     db "R", 0
str_nav_home:       db "H", 0
str_web_url:        db "[SEC] http://127.0.0.1:8080/index.html", 0
str_btn_summarize:  db "[ Y.U.K.I Ozetle ]", 0
str_ico_l3:         db "[L3]", 0

str_hero_t1:        db "Yukinary OS - Noral Web Ag Gecidi (Port 8080 Live)", 0
str_hero_t2:        db "* 200 OK | Sunucu: YknOS-Embedded/2.1 | Gecikme: 0.08 ms | TLS 1.3", 0

str_c1_hdr:         db "[ REST API ENDPOINTS - CANLI VERI SUNUCUSU ]", 0
str_c1_l1:          db "GET /api/v1/system/status -> 200 OK { 'cpu': 'x86_64', 'ram_free': '61MB' }", 0
str_c1_l2:          db "GET /api/v1/yuki/memory   -> 200 OK { 'vector_nodes': 14, 'mode': 'BitNet' }", 0
str_c1_l3:          db "GET /api/v1/sandbox/state -> 200 OK { 'level': 'L3-Isolated', 'sec': 'OK' }", 0

str_c2_hdr:         db "[ SIFIR GUVENILIRLIK (ZERO-TRUST) & NORAL CSP FILTRESI ]", 0
str_c2_l1:          db "[OK] Zero-Trust HTTP/1.1 ve WebSocket oturum izolasyonu devrede.", 0
str_c2_l2:          db "[OK] Yerel Noral Filtre: XSS, SQLi ve yetkisiz bellek erisimi engellendi.", 0

str_web_links:      db "[ /api/telemetry ]   [ Guvenlik Raporu ]   [ RamFS Indir ]   [ Konsol ]", 0
str_web_status:     db "* Bagli: 127.0.0.1:8080 | TLS 1.3 Yukinary Secure | JS: YknV8-Micro | 60 FPS", 0

str_ico_d1: db "[*]", 0
str_ico_d2: db "[AI]", 0
str_ico_d3: db "[DIR]", 0
str_ico_d4: db "[>_]", 0
str_ico_d5: db "[WEB]", 0
str_ico_d6: db "[SEC]", 0
str_ico_d7: db "[SQL]", 0
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

; Pencere Durumlari ve Pozisyonlari
alignb 4
w1_x:            resd 1
w1_y:            resd 1
w1_w:            resd 1
w1_h:            resd 1
w1_state:        resb 1 ; 1: normal, 0: gizli/kucuk

w2_x:            resd 1
w2_y:            resd 1
w2_w:            resd 1
w2_h:            resd 1
w2_state:        resb 1

w3_x:            resd 1
w3_y:            resd 1
w3_w:            resd 1
w3_h:            resd 1
w3_state:        resb 1

w4_x:            resd 1
w4_y:            resd 1
w4_w:            resd 1
w4_h:            resd 1
w4_state:        resb 1

win_z_order:     resb 4 ; [0..3]: sira ile cizilen pencere ID'leri (en sondaki en ustte)
active_window:   resb 1 ; 1: Y.U.K.I, 2: RamFS, 3: Terminal, 4: Browser
yuki_active_tab: resb 1 ; 1..5
browser_active_tab: resb 1 ; 1..4
menu_open:       resb 1

; Surukleme Durumu
is_dragging:     resb 1 ; 0: yok, 1..4: suruklenen pencere
alignb 4
drag_off_x:      resd 1
drag_off_y:      resd 1

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
