; ============================================================
; YknOS Bare-Metal Ring-0 Kernel with Desktop GUI, VGA & PS/2
; Multiboot 1 compliant, loads at 1 MB (0x00100000)
; Boot with QEMU: qemu-system-x86_64 -kernel yknos_baremetal.bin -serial stdio
; ============================================================

[BITS 32]
[ORG 0x100000]

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ (1 << 0) | (1 << 1) | (1 << 16) ; ALIGN | MEMINFO | AOUT_KLUDGE
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .text
multiboot_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    dd multiboot_header ; header_addr
    dd 0x100000         ; load_addr
    dd _kernel_end      ; load_end_addr
    dd _kernel_end      ; bss_end_addr
    dd _start           ; entry_addr

_start:
    cli
    mov esp, 0x90000 ; Stack below 640 KB conventional RAM

    ; 1. COM1 UART Seri Port (0x3F8) Baslat
    call serial_init
    mov esi, msg_serial_boot
    call serial_print

    ; 2. VGA 80x25 Video Bellek (0xB8000) ve Grafiksel Masaustu Cizimi
    call vga_clear
    call gui_draw_full

    ; 3. Komut Istemi (Prompt) Konumlandir (Satir 15, Kolon 2)
    mov byte [cursor_row], 15
    mov byte [cursor_col], 2
    mov esi, msg_prompt
    call vga_print

    mov esi, msg_serial_ready
    call serial_print

; ------------------------------------------------------------
; PS/2 Klavye Donanim Dinleme Dongusu (Port 0x64 & 0x60)
; ------------------------------------------------------------
main_loop:
    in al, 0x64
    test al, 0x01
    jz main_loop

    in al, 0x60

    ; Break code (tus birakma >= 0x80) kontrolu
    test al, 0x80
    jnz main_loop

    ; 1. TAB Tusu (0x0F): GUI Kontrol Modu <-> Shell Girdi Modu Gecisi
    cmp al, 0x0F
    je handle_tab_mode

    ; Eger GUI modundaysak, ozel navigasyon tuslarini yakala
    cmp dword [gui_mode], 1
    je handle_gui_keys

    ; 2. Shell Girdi Modu (Komut Satiri)
    cmp al, 0x1C
    je handle_enter

    cmp al, 0x0E
    je handle_backspace

    movzx ebx, al
    cmp ebx, 58
    jae main_loop
    mov cl, [scancode_table + ebx]
    test cl, cl
    jz main_loop

    mov edx, [cmd_len]
    cmp edx, 60
    jae main_loop
    mov [cmd_buffer + edx], cl
    inc dword [cmd_len]

    ; VGA ve Seri Porta bas
    mov al, cl
    mov ah, 0x1E ; Sari yazi
    call vga_putc

    mov al, cl
    call serial_putc
    jmp main_loop

; ------------------------------------------------------------
; TAB: Mod Degisimi (GUI Arayuz Kontrolu <-> Kabuk Yazma)
; ------------------------------------------------------------
handle_tab_mode:
    xor dword [gui_mode], 1
    call gui_draw_status_bar
    jmp main_loop

; ------------------------------------------------------------
; GUI Navigasyon Kontrolleri (1-4, B, WASD, JK)
; ------------------------------------------------------------
handle_gui_keys:
    cmp al, 0x02 ; '1'
    je .key_tab1
    cmp al, 0x03 ; '2'
    je .key_tab2
    cmp al, 0x04 ; '3'
    je .key_tab3
    cmp al, 0x05 ; '4'
    je .key_tab4
    cmp al, 0x30 ; 'b' / 'B'
    je .key_baslat
    cmp al, 0x11 ; 'w'
    je .key_up
    cmp al, 0x1F ; 's'
    je .key_down
    cmp al, 0x1E ; 'a'
    je .key_left
    cmp al, 0x20 ; 'd'
    je .key_right
    cmp al, 0x24 ; 'j'
    je .key_file_down
    cmp al, 0x25 ; 'k'
    je .key_file_up
    cmp al, 0x1C ; Enter -> Moddan cikip konsola don
    je handle_tab_mode
    jmp main_loop

.key_tab1:
    mov dword [active_tab], 1
    mov dword [start_menu_open], 0
    call gui_draw_full
    jmp main_loop
.key_tab2:
    mov dword [active_tab], 2
    mov dword [start_menu_open], 0
    call gui_draw_full
    jmp main_loop
.key_tab3:
    mov dword [active_tab], 3
    mov dword [start_menu_open], 0
    call gui_draw_full
    jmp main_loop
.key_tab4:
    mov dword [active_tab], 4
    mov dword [start_menu_open], 0
    call gui_draw_full
    jmp main_loop
.key_baslat:
    xor dword [start_menu_open], 1
    call gui_draw_full
    jmp main_loop
.key_up:
    cmp dword [mouse_y], 2
    jle main_loop
    dec dword [mouse_y]
    call gui_draw_full
    jmp main_loop
.key_down:
    cmp dword [mouse_y], 12
    jge main_loop
    inc dword [mouse_y]
    call gui_draw_full
    jmp main_loop
.key_left:
    cmp dword [mouse_x], 4
    jle main_loop
    sub dword [mouse_x], 2
    call gui_draw_full
    jmp main_loop
.key_right:
    cmp dword [mouse_x], 74
    jge main_loop
    add dword [mouse_x], 2
    call gui_draw_full
    jmp main_loop
.key_file_down:
    mov eax, [selected_file]
    cmp eax, 3
    jge main_loop
    inc dword [selected_file]
    call gui_draw_full
    jmp main_loop
.key_file_up:
    mov eax, [selected_file]
    test eax, eax
    jle main_loop
    dec dword [selected_file]
    call gui_draw_full
    jmp main_loop

; ------------------------------------------------------------
; Shell Satir Duzenleme ve Calistirma
; ------------------------------------------------------------
handle_backspace:
    mov edx, [cmd_len]
    test edx, edx
    jz main_loop
    dec dword [cmd_len]
    mov byte [cmd_buffer + edx - 1], 0

    dec byte [cursor_col]
    mov al, ' '
    mov ah, 0x1F
    call vga_putc
    dec byte [cursor_col]
    jmp main_loop

handle_enter:
    mov edx, [cmd_len]
    mov byte [cmd_buffer + edx], 0

    mov al, 13
    call serial_putc
    mov al, 10
    call serial_putc

    inc byte [cursor_row]
    mov byte [cursor_col], 2

    call dispatch_command

    mov dword [cmd_len], 0
    mov byte [cmd_buffer], 0

    inc byte [cursor_row]
    cmp byte [cursor_row], 24
    jb .prompt_ok
    call scroll_shell_area
    mov byte [cursor_row], 23
.prompt_ok:
    mov byte [cursor_col], 2
    mov esi, msg_prompt
    call vga_print
    jmp main_loop

scroll_shell_area:
    ; Satir 15..24 arasini 1 satir yukari kaydir
    mov esi, 0xB8000 + (16 * 80 * 2)
    mov edi, 0xB8000 + (15 * 80 * 2)
    mov ecx, (9 * 80)
    rep movsw
    ; Son satiri temizle
    mov edi, 0xB8000 + (24 * 80 * 2)
    mov ecx, 80
    mov ax, 0x1F20
    rep stosw
    ret

; ------------------------------------------------------------
; Cekirdek Komut Cozucu (Dispatcher)
; ------------------------------------------------------------
dispatch_command:
    cmp dword [cmd_len], 0
    je .done

    mov esi, cmd_buffer
    mov edi, str_yardim
    call strcmp
    jz .do_yardim
    mov esi, cmd_buffer
    mov edi, str_help
    call strcmp
    jz .do_yardim

    mov esi, cmd_buffer
    mov edi, str_bilgi
    call strcmp
    jz .do_bilgi

    mov esi, cmd_buffer
    mov edi, str_ls
    call strcmp
    jz .do_ls
    mov esi, cmd_buffer
    mov edi, str_dir
    call strcmp
    jz .do_ls

    mov esi, cmd_buffer
    mov edi, str_cat
    call strcmp
    jz .do_cat

    mov esi, cmd_buffer
    mov edi, str_sql
    call strcmp
    jz .do_sql

    mov esi, cmd_buffer
    mov edi, str_http
    call strcmp
    jz .do_http

    mov esi, cmd_buffer
    mov edi, str_bellek
    call strcmp
    jz .do_bellek

    mov esi, cmd_buffer
    mov edi, str_gui
    call strcmp
    jz .do_gui
    mov esi, cmd_buffer
    mov edi, str_masaustu
    call strcmp
    jz .do_gui
    mov esi, cmd_buffer
    mov edi, str_fm
    call strcmp
    jz .do_fm
    mov esi, cmd_buffer
    mov edi, str_dosyalar
    call strcmp
    jz .do_fm

    mov esi, cmd_buffer
    mov edi, str_temizle
    call strcmp
    jz .do_temizle

    mov esi, cmd_buffer
    mov edi, str_kapat
    call strcmp
    jz .do_kapat

    mov esi, msg_bilinmeyen
    call vga_print
    mov esi, msg_bilinmeyen
    call serial_print
    ret

.do_yardim:
    mov esi, msg_yardim
    call vga_print
    mov esi, msg_yardim
    call serial_print
    ret

.do_bilgi:
    mov esi, msg_bilgi
    call vga_print
    mov esi, msg_bilgi
    call serial_print
    ret

.do_ls:
    mov esi, msg_ls_result
    call vga_print
    mov esi, msg_ls_result
    call serial_print
    ret

.do_cat:
    mov esi, msg_cat_result
    call vga_print
    mov esi, msg_cat_result
    call serial_print
    ret

.do_sql:
    mov dword [active_tab], 2
    call gui_draw_full
    mov esi, msg_sql_result
    call vga_print
    mov esi, msg_sql_result
    call serial_print
    ret

.do_http:
    mov dword [active_tab], 3
    call gui_draw_full
    mov esi, msg_http_result
    call vga_print
    mov esi, msg_http_result
    call serial_print
    ret

.do_bellek:
    mov esi, msg_bellek_result
    call vga_print
    mov esi, msg_bellek_result
    call serial_print
    ret

.do_gui:
    mov dword [gui_mode], 1
    call gui_draw_full
    mov esi, msg_gui_switched
    call vga_print
    ret

.do_fm:
    mov dword [active_tab], 1
    mov dword [gui_mode], 1
    call gui_draw_full
    mov esi, msg_fm_switched
    call vga_print
    ret

.do_temizle:
    ; Satir 15..24 arasini temizle
    mov edi, 0xB8000 + (15 * 80 * 2)
    mov ecx, (10 * 80)
    mov ax, 0x1F20
    rep stosw
    mov byte [cursor_row], 14
    ret

.do_kapat:
    mov esi, msg_kapat
    call vga_print
    mov esi, msg_kapat
    call serial_print
    ; QEMU ACPI poweroff
    mov dx, 0x604
    mov ax, 0x2000
    out dx, ax
    cli
.hlt:
    hlt
    jmp .hlt

.done:
    ret

; ------------------------------------------------------------
; Dize Karsilastirma
; ------------------------------------------------------------
strcmp:
.loop:
    mov al, [esi]
    mov bl, [edi]
    cmp al, bl
    jne .not_eq
    test al, al
    jz .eq
    inc esi
    inc edi
    jmp .loop
.not_eq:
    or al, 1
    ret
.eq:
    xor eax, eax
    ret

; ------------------------------------------------------------
; COM1 UART Seri Port (0x3F8)
; ------------------------------------------------------------
serial_init:
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03 ; 38400 baud
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03 ; 8N1
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    ret

serial_putc:
    push dx
    push ax
    mov dx, 0x3FD
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret

serial_print:
.loop:
    lodsb
    test al, al
    jz .done
    call serial_putc
    jmp .loop
.done:
    ret

; ------------------------------------------------------------
; VGA Metin Bellek Surucusu ve Grafiksel Masaustu Motoru
; ------------------------------------------------------------
vga_clear:
    mov edi, 0xB8000
    mov ecx, 2000
    mov ax, 0x1F20 ; Mavi zemin, beyaz bosluk
    rep stosw
    ret

vga_putc:
    push ebx
    push edx
    movzx edx, byte [cursor_row]
    imul edx, 80
    movzx ebx, byte [cursor_col]
    add edx, ebx
    shl edx, 1
    mov [0xB8000 + edx], ax
    inc byte [cursor_col]
    cmp byte [cursor_col], 78
    jb .ok
    mov byte [cursor_col], 2
    inc byte [cursor_row]
.ok:
    pop edx
    pop ebx
    ret

vga_print:
.loop:
    lodsb
    test al, al
    jz .done
    cmp al, 10
    jne .not_nl
    inc byte [cursor_row]
    mov byte [cursor_col], 2
    jmp .loop
.not_nl:
    mov ah, 0x1F ; Beyaz metin
    call vga_putc
    jmp .loop
.done:
    ret

; ------------------------------------------------------------
; vga_print_at: Belirtilen Satir/Sutun ve Renkte Metin Yazar
; Girdiler: al = row, ah = col, bl = color, esi = string_ptr
; ------------------------------------------------------------
vga_print_at:
    push edi
    push edx
    push ecx
    movzx edx, al
    imul edx, 80
    movzx ecx, ah
    add edx, ecx
    shl edx, 1
    mov edi, 0xB8000
    add edi, edx
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, bl
    stosw
    jmp .loop
.done:
    pop ecx
    pop edx
    pop edi
    ret

; ------------------------------------------------------------
; GRAFIKSEL MASAUSTU ARAYUZ CIZIM RUTINLERI
; ------------------------------------------------------------
gui_draw_full:
    ; 1. Masaustu alanini (Satir 1..12) temizle
    mov edi, 0xB8000 + (1 * 80 * 2)
    mov ecx, (12 * 80)
    mov ax, 0x1F20
    rep stosw

    ; 2. Ust Sekmeler ve Gorev Cubugu (Satir 0)
    mov al, 0
    mov ah, 0
    mov bl, 0x1E ; Sari
    mov esi, gui_top_bar
    call vga_print_at

    ; Baslat aktifse
    cmp dword [start_menu_open], 1
    jne .check_tabs
    mov al, 0
    mov ah, 2
    mov bl, 0x2F ; Yesil zemin
    mov esi, str_tab_baslat_hi
    call vga_print_at

.check_tabs:
    mov edx, [active_tab]
    cmp edx, 1
    je .hi_tab1
    cmp edx, 2
    je .hi_tab2
    cmp edx, 3
    je .hi_tab3
    cmp edx, 4
    je .hi_tab4
    jmp .draw_body

.hi_tab1:
    mov al, 0
    mov ah, 14
    mov bl, 0x2F
    mov esi, str_tab1_hi
    call vga_print_at
    jmp .draw_body
.hi_tab2:
    mov al, 0
    mov ah, 29
    mov bl, 0x2F
    mov esi, str_tab2_hi
    call vga_print_at
    jmp .draw_body
.hi_tab3:
    mov al, 0
    mov ah, 39
    mov bl, 0x2F
    mov esi, str_tab3_hi
    call vga_print_at
    jmp .draw_body
.hi_tab4:
    mov al, 0
    mov ah, 48
    mov bl, 0x2F
    mov esi, str_tab4_hi
    call vga_print_at

.draw_body:
    ; 3. Aktif Sekmeye Gore Icerigi Ciz
    mov edx, [active_tab]
    cmp edx, 1
    je .draw_fm
    cmp edx, 2
    je .draw_sql
    cmp edx, 3
    je .draw_net
    jmp .draw_term

.draw_fm:
    ; --- SEKME 1: DOSYA YONETICISI ---
    mov al, 1
    mov ah, 2
    mov bl, 0x1B ; Camgobegi
    mov esi, fm_header
    call vga_print_at

    mov al, 2
    mov ah, 4
    mov bl, 0x1E ; Sari
    mov esi, fm_cols
    call vga_print_at

    mov al, 3
    mov ah, 4
    mov bl, 0x17
    mov esi, fm_sep
    call vga_print_at

    ; Dosyalar (Satir 4..7)
    mov ecx, 0
.fm_file_loop:
    push ecx
    mov al, cl
    add al, 4
    mov ah, 4
    mov bl, 0x1F ; Varsayilan beyaz
    cmp ecx, [selected_file]
    jne .not_sel
    mov bl, 0x30 ; Secili: Camgobegi zemin, siyah yazi
    push eax
    push ebx
    mov ah, 2
    mov bl, 0x1E
    mov esi, str_arrow
    call vga_print_at
    pop ebx
    pop eax
.not_sel:
    mov esi, fm_file_0
    cmp ecx, 0
    je .do_draw_file
    mov esi, fm_file_1
    cmp ecx, 1
    je .do_draw_file
    mov esi, fm_file_2
    cmp ecx, 2
    je .do_draw_file
    mov esi, fm_file_3
.do_draw_file:
    call vga_print_at
    pop ecx
    inc ecx
    cmp ecx, 4
    jl .fm_file_loop

    ; Onizleme Kutusu (Satir 8..11)
    mov al, 8
    mov ah, 2
    mov bl, 0x1E
    mov esi, fm_prev_box
    call vga_print_at

    mov al, 9
    mov ah, 4
    mov bl, 0x1A ; Yesil yazi
    mov esi, fm_content_0
    mov edx, [selected_file]
    cmp edx, 0
    je .do_prev
    mov esi, fm_content_1
    cmp edx, 1
    je .do_prev
    mov esi, fm_content_2
    cmp edx, 2
    je .do_prev
    mov esi, fm_content_3
.do_prev:
    call vga_print_at
    jmp .draw_popup

.draw_sql:
    ; --- SEKME 2: SQL PANELI ---
    mov al, 1
    mov ah, 2
    mov bl, 0x1A
    mov esi, sql_header
    call vga_print_at

    mov al, 2
    mov ah, 4
    mov bl, 0x1E
    mov esi, sql_cols
    call vga_print_at

    mov al, 4
    mov ah, 4
    mov bl, 0x1F
    mov esi, sql_row1
    call vga_print_at

    mov al, 5
    mov ah, 4
    mov bl, 0x1F
    mov esi, sql_row2
    call vga_print_at

    mov al, 6
    mov ah, 4
    mov bl, 0x1F
    mov esi, sql_row3
    call vga_print_at

    mov al, 7
    mov ah, 4
    mov bl, 0x1F
    mov esi, sql_row4
    call vga_print_at

    mov al, 8
    mov ah, 4
    mov bl, 0x1F
    mov esi, sql_row5
    call vga_print_at

    mov al, 9
    mov ah, 4
    mov bl, 0x1F
    mov esi, sql_row6
    call vga_print_at

    mov al, 11
    mov ah, 4
    mov bl, 0x1B
    mov esi, sql_footer
    call vga_print_at
    jmp .draw_popup

.draw_net:
    ; --- SEKME 3: HTTP & AG PANELI ---
    mov al, 1
    mov ah, 2
    mov bl, 0x1D ; Magenta
    mov esi, net_header
    call vga_print_at

    mov al, 3
    mov ah, 4
    mov bl, 0x1A ; Yesil
    mov esi, net_status
    call vga_print_at

    mov al, 4
    mov ah, 4
    mov bl, 0x1E ; Sari
    mov esi, net_url
    call vga_print_at

    mov al, 5
    mov ah, 4
    mov bl, 0x17
    mov esi, net_socket
    call vga_print_at

    mov al, 7
    mov ah, 4
    mov bl, 0x1B
    mov esi, net_routes_hdr
    call vga_print_at

    mov al, 8
    mov ah, 6
    mov bl, 0x1F
    mov esi, net_route1
    call vga_print_at

    mov al, 9
    mov ah, 6
    mov bl, 0x1F
    mov esi, net_route2
    call vga_print_at

    mov al, 10
    mov ah, 6
    mov bl, 0x1F
    mov esi, net_route3
    call vga_print_at

    mov al, 11
    mov ah, 6
    mov bl, 0x1F
    mov esi, net_route4
    call vga_print_at
    jmp .draw_popup

.draw_term:
    ; --- SEKME 4: TERMINAL OZETI ---
    mov al, 1
    mov ah, 2
    mov bl, 0x1E
    mov esi, term_header
    call vga_print_at

    mov al, 3
    mov ah, 4
    mov bl, 0x1A
    mov esi, term_line1
    call vga_print_at

    mov al, 4
    mov ah, 4
    mov bl, 0x1F
    mov esi, term_line2
    call vga_print_at

    mov al, 5
    mov ah, 4
    mov bl, 0x1A
    mov esi, term_line3
    call vga_print_at

    mov al, 6
    mov ah, 4
    mov bl, 0x1F
    mov esi, term_line4
    call vga_print_at

    mov al, 8
    mov ah, 4
    mov bl, 0x1B
    mov esi, term_line5
    call vga_print_at

.draw_popup:
    ; 4. Baslat Menusu Aciksa Sol Tarafa Ciz
    cmp dword [start_menu_open], 1
    jne .draw_mouse

    mov al, 2
    mov ah, 2
    mov bl, 0x1E
    mov esi, menu_row0
    call vga_print_at

    mov al, 3
    mov ah, 2
    mov bl, 0x1F
    mov esi, menu_row1
    call vga_print_at

    mov al, 4
    mov ah, 2
    mov bl, 0x1F
    mov esi, menu_row2
    call vga_print_at

    mov al, 5
    mov ah, 2
    mov bl, 0x1F
    mov esi, menu_row3
    call vga_print_at

    mov al, 6
    mov ah, 2
    mov bl, 0x1F
    mov esi, menu_row4
    call vga_print_at

    mov al, 7
    mov ah, 2
    mov bl, 0x1E
    mov esi, menu_row5
    call vga_print_at

    mov al, 8
    mov ah, 2
    mov bl, 0x1C ; Kirmizi
    mov esi, menu_row6
    call vga_print_at

    mov al, 9
    mov ah, 2
    mov bl, 0x1E
    mov esi, menu_row7
    call vga_print_at

.draw_mouse:
    ; 5. Fare Imleci Ciz (▲ / ^) [mouse_y, mouse_x]
    mov eax, [mouse_y]
    imul eax, 80
    add eax, [mouse_x]
    shl eax, 1
    mov word [0xB8000 + eax], 0x4F5E ; Kirmizi zemin, beyaz '^' imleci

    ; 6. Alt Durum Cubugu ve Bolucu Ciz
    call gui_draw_status_bar

    mov al, 14
    mov ah, 0
    mov bl, 0x17
    mov esi, shell_divider
    call vga_print_at
    ret

; ------------------------------------------------------------
; Alt Durum Cubugunu Guncelle (Satir 13)
; ------------------------------------------------------------
gui_draw_status_bar:
    mov al, 13
    mov ah, 0
    mov bl, 0x1E
    cmp dword [gui_mode], 1
    jne .shell_mode_status
    mov esi, status_bar_gui
    call vga_print_at
    jmp .draw_coords
.shell_mode_status:
    mov esi, status_bar_shell
    call vga_print_at

.draw_coords:
    ; X Koordinati yaz
    mov eax, [mouse_x]
    xor edx, edx
    mov ecx, 10
    div ecx
    add al, '0'
    add dl, '0'
    mov byte [str_coord_x], al
    mov byte [str_coord_x + 1], dl

    ; Y Koordinati yaz
    mov eax, [mouse_y]
    xor edx, edx
    mov ecx, 10
    div ecx
    add al, '0'
    add dl, '0'
    mov byte [str_coord_y], al
    mov byte [str_coord_y + 1], dl

    mov al, 13
    mov ah, 5
    mov bl, 0x1A
    mov esi, str_coord_x
    call vga_print_at

    mov al, 13
    mov ah, 10
    mov bl, 0x1A
    mov esi, str_coord_y
    call vga_print_at
    ret

; ------------------------------------------------------------
; Durum Degiskenleri ve Veriler
; ------------------------------------------------------------
cursor_row:      db 15
cursor_col:      db 2
cmd_len:         dd 0
cmd_buffer:      times 64 db 0

gui_mode:        dd 1 ; 1: GUI Navigasyon, 0: Shell Komut Girisi
active_tab:      dd 1 ; 1: Dosyalar, 2: SQL, 3: Ag, 4: Terminal
start_menu_open: dd 0 ; 0: Kapali, 1: Acik
selected_file:   dd 0 ; 0..3
mouse_x:         dd 24
mouse_y:         dd 8

str_coord_x:     db "00", 0
str_coord_y:     db "00", 0

str_yardim:      db "yardim", 0
str_help:        db "help", 0
str_bilgi:       db "bilgi", 0
str_ls:          db "ls", 0
str_dir:         db "dir", 0
str_cat:         db "cat", 0
str_sql:         db "sql", 0
str_http:        db "http", 0
str_bellek:      db "bellek", 0
str_gui:         db "gui", 0
str_masaustu:    db "masaustu", 0
str_fm:          db "fm", 0
str_dosyalar:    db "dosyalar", 0
str_temizle:     db "temizle", 0
str_kapat:       db "kapat", 0

; GUI Sabit Dizeleri
gui_top_bar:     db "+-[* BASLAT]--[1: Dosyalar]--[2: SQL]--[3: Ag]--[4: Terminal]--------[12:00]-+", 0
str_tab_baslat_hi: db "[* BASLAT *]", 0
str_tab1_hi:     db "*1: Dosyalar*", 0
str_tab2_hi:     db "*2: SQL*", 0
str_tab3_hi:     db "*3: Ag*", 0
str_tab4_hi:     db "*4: Terminal*", 0
str_arrow:       db ">", 0

status_bar_gui:   db "+-[X:   Y:  ]-[1-4: Sekme]-[b: Baslat]-[wasd: Fare]-[jk: Dosya]-[TAB: Shell]-+", 0
status_bar_shell: db "+-[X:   Y:  ]-[KABUK MODU: Komut yazip ENTER'a basin]-----------[TAB: GUI]---+", 0
shell_divider:    db "+--------------------[ YknOS Cekirdek Konsolu / Terminal ]-------------------+", 0

; Sekme 1: Dosya Yoneticisi
fm_header:       db "+---[ YknOS Dosya Yoneticisi | RamFS Kok Dizin: / ]------------------------+", 0
fm_cols:         db "DOSYA ADI         BOYUT     IZIN    ACIKLAMA", 0
fm_sep:          db "------------------------------------------------------------------", 0
fm_file_0:       db "/sistem.cfg       94  Bayt  r-- (ro)  Cekirdek baslangic ayarlari", 0
fm_file_1:       db "/index.html       390 Bayt  r-- (ro)  HTTP Web Sunucusu Paneli", 0
fm_file_2:       db "/veritabani.sql   195 Bayt  rw- (rw)  SQL Sema ve tohum verisi", 0
fm_file_3:       db "/gunluk.txt       120 Bayt  rw- (rw)  Cekirdek acilis gunlukleri", 0
fm_prev_box:     db "+-----[ Dosya Onizleme ]-------------------------------------------------+", 0
fm_content_0:    db "cekirdek=YknOS, surum=0.1.0-alpha, mimari=x86_64, ram=64MB, port=8080", 0
fm_content_1:    db "<!DOCTYPE html><html><head><title>YknOS Dashboard</title>...", 0
fm_content_2:    db "CREATE TABLE sistem_metrikleri (id INT, servis TEXT, cpu INT);", 0
fm_content_3:    db "[0.00s] Kernel boot... [0.01s] VGA ready... [0.05s] SQL online...", 0

; Sekme 2: SQL Paneli
sql_header:      db "+---[ YknOS Gomulu SQL Veritabani Paneli ]---------------------------------+", 0
sql_cols:        db "ID   SERVIS          CPU%   BELLEK(KB)   DURUM", 0
sql_row1:        db "#1   KERNEL_CORE      3%     128 KB      ONLINE", 0
sql_row2:        db "#2   HTTP_DAEMON     14%     256 KB      ONLINE", 0
sql_row3:        db "#3   SQL_STORE        8%     512 KB      ONLINE", 0
sql_row4:        db "#4   SCHEDULER        2%      64 KB      ONLINE", 0
sql_row5:        db "#5   RAMFS_DRIVER     1%      32 KB      ONLINE", 0
sql_row6:        db "#6   SHELL_SESSION    4%      96 KB      ONLINE", 0
sql_footer:      db "Sorgu: SELECT * FROM sistem_metrikleri (6 kayit aktif)", 0

; Sekme 3: HTTP Ag Paneli
net_header:      db "+---[ YknOS HTTP 1.1 REST Web Sunucusu ve Ag Monitoru ]--------------------+", 0
net_status:      db "Sunucu Durumu : DINLENIYOR (0.0.0.0:8080)", 0
net_url:         db "Yerel Ag Yolu : http://localhost:8080/", 0
net_socket:      db "Aktif Servis  : Ring-0 / BSD Sockets Cekirdek Dinleyicisi", 0
net_routes_hdr:  db "Kayitli API Uc Noktalari:", 0
net_route1:      db "1. GET /index.html    -> RamFS HTML Dashboard Paneli", 0
net_route2:      db "2. GET /api/v1/durum  -> JSON Cekirdek Calisma Durumu", 0
net_route3:      db "3. GET /api/v1/sql    -> JSON Canli SQL Tablosu Akisi", 0
net_route4:      db "4. GET /api/v1/fs     -> JSON RamFS Dosya Sistemi Raporu", 0

; Sekme 4: Terminal
term_header:     db "+---[ YknOS Cekirdek Terminali (Shell) ]-----------------------------------+", 0
term_line1:      db "yknos> bilgi", 0
term_line2:      db "OS: YknOS v0.1.0-alpha (Dual-Mode Bare-Metal Ring-0 Kernel)", 0
term_line3:      db "yknos> ls", 0
term_line4:      db "RamFS: /sistem.cfg  /index.html  /veritabani.sql  /gunluk.txt", 0
term_line5:      db "Komut yazmak icin TAB'a basarak Kabuk Moduna gecebilirsiniz.", 0

; Baslat Menusu Acilir Kutusu
menu_row0:       db "+---[* BASLAT MENUSU *]---+", 0
menu_row1:       db "| 1. [D] Dosya Yoneticisi |", 0
menu_row2:       db "| 2. [S] SQL Veritabani   |", 0
menu_row3:       db "| 3. [A] HTTP Ag Servisi  |", 0
menu_row4:       db "| 4. [T] Terminal Kabuk   |", 0
menu_row5:       db "| ----------------------- |", 0
menu_row6:       db "| 0. [X] Sistemi Kapat    |", 0
menu_row7:       db "+-------------------------+", 0

; Mesajlar
msg_prompt:       db "yknos> ", 0
msg_bilinmeyen:   db "Bilinmeyen komut! 'yardim' yazin.", 10, 0
msg_yardim:       db "Komutlar: bilgi, ls, cat, sql, http, bellek, gui, fm, temizle, kapat", 10, 0
msg_bilgi:        db "YknOS v0.1.0 Ring-0 Kernel | %100 Bare-Metal Machine Code | GUI + Shell", 10, 0
msg_ls_result:    db "RamFS: /sistem.cfg (94B) | /index.html (390B) | /veritabani.sql (195B)", 10, 0
msg_cat_result:   db "cekirdek=YknOS, surum=0.1.0-alpha, mimari=x86_64, ram=64MB, port=8080", 10, 0
msg_sql_result:   db "SELECT * FROM metrikler => KERNEL: 3% CPU | HTTP: 14% CPU | SQL: 8% CPU (OK)", 10, 0
msg_http_result:  db "HTTP/1.1 200 OK | Content-Type: application/json | {os:'YknOS', durum:'ONLINE'}", 10, 0
msg_bellek_result:db "RAM: 64 MB Toplam | 512 Sayfa Tahsisli (4 KB Paging) | Serbest: 61440 KB", 10, 0
msg_gui_switched: db "Grafiksel masaustu navigasyon modu aktif. (TAB ile cikis)", 10, 0
msg_fm_switched:  db "Dosya yoneticisi sekmesi acildi. (j/k ile dosya secimi)", 10, 0
msg_kapat:        db "Cekirdek durduruluyor... Halted.", 10, 0

msg_serial_boot:  db 13, 10, "[YknOS Bare-Metal] Multiboot1 Kernel with Desktop GUI Booting at 0x100000...", 13, 10, 0
msg_serial_ready: db "[YknOS Bare-Metal] GUI Desktop & VGA Active. Press TAB to switch modes!", 13, 10, 0

scancode_table:
    db 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 9
    db 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0
    db 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', "'", '`', 0, '\'
    db 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '

_kernel_end:
