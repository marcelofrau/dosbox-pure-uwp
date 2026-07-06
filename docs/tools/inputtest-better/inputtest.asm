; DOSBox Pure Input Test v4
; COM file. Shows keyboard scan/ASCII/modifiers, mouse X,Y,buttons.
; Named display for special keys (ENTER, TAB, ESC, CTRL-X, etc).
; ESC to exit.
; Build: nasm -f bin -o inputtest.com inputtest.asm

CPU 386
org 100h

start:
    mov ax, 0003h
    int 10h
    mov ah, 01h
    mov cx, 2607h
    int 10h
    xor ax, ax
    int 33h
    mov [hasMouse], al
    cmp ax, 0
    je .ui
    mov ax, 0001h
    int 33h
.ui:
    call draw_ui

.main:
    call show_mods
    mov ah, 01h
    int 16h
    jz .mous
    mov ah, 00h
    int 16h
    mov [ascii], al
    mov [scan], ah
    cmp al, 27
    je exit
    call show_key
.mous:
    cmp byte [hasMouse], 0
    je .delay
    mov ax, 0003h
    int 33h
    mov [mx], cx
    mov [my], dx
    mov [mbtn], bl
    call show_mouse
.delay:
    mov cx, 0
.d: loop .d
    jmp .main

exit:
    cmp byte [hasMouse], 0
    je .x
    mov ax, 0002h
    int 33h
.x:
    mov ax, 0003h
    int 10h
    ret

draw_ui:
    mov dx, header
    call puts
    mov dh, 3
    mov dl, 0
    call gotoxy
    mov dx, l_mod
    call puts
    mov dh, 5
    mov dl, 0
    call gotoxy
    mov dx, l_scan
    call puts
    mov dh, 7
    mov dl, 0
    call gotoxy
    mov dx, l_ascii
    call puts
    mov dh, 9
    mov dl, 0
    call gotoxy
    mov dx, l_char
    call puts
    mov dh, 11
    mov dl, 0
    call gotoxy
    mov dx, l_mouse
    call puts
    mov dh, 13
    mov dl, 0
    call gotoxy
    mov dx, l_btns
    call puts
    ret

show_mods:
    push ax
    push bx
    push cx
    mov dh, 3
    mov dl, 5
    call gotoxy
    mov ah, 12h
    int 16h
    mov bx, ax
    ; BL = FLAGS1 (basic): bit0=RSHIFT, bit1=LSHIFT, bit2=CTRL, bit3=ALT
    ; BH = extended:    bit0=LCTRL, bit1=LALT, bit2=RCTRL, bit3=RALT
    ; -- Shift --
    mov dx, l_sh
    call puts
    test bl, 2
    mov al, '0'
    jz .s1
    mov al, '1'
.s1: call putchar
    mov dx, l_rsep
    call puts
    test bl, 1
    mov al, '0'
    jz .s2
    mov al, '1'
.s2: call putchar
    ; -- Ctrl --
    mov dx, l_ctl
    call puts
    test bh, 1
    mov al, '0'
    jz .c1
    mov al, '1'
.c1: call putchar
    mov dx, l_rsep
    call puts
    test bh, 4
    mov al, '0'
    jz .c2
    mov al, '1'
.c2: call putchar
    ; -- Alt --
    mov dx, l_altl
    call puts
    test bh, 2
    mov al, '0'
    jz .a1
    mov al, '1'
.a1: call putchar
    mov dx, l_rsep
    call puts
    test bh, 8
    mov al, '0'
    jz .a2
    mov al, '1'
.a2: call putchar
    ; Clear rest of line
    push cx
    mov cx, 18
.clr: mov al, ' '
    call putchar
    loop .clr
    pop cx
    pop cx
    pop bx
    pop ax
    ret

show_key:
    push ax
    push cx
    ; Scan code
    mov dh, 5
    mov dl, 12
    call gotoxy
    mov al, [scan]
    call puthex
    ; ASCII
    mov dh, 7
    mov dl, 12
    call gotoxy
    mov al, [ascii]
    xor ah, ah
    call putdec
    ; Char — clear area first
    mov dh, 9
    mov dl, 12
    call gotoxy
    mov cx, 24
.clr: mov al, ' '
    call putchar
    loop .clr
    ; Char — show value
    mov dh, 9
    mov dl, 12
    call gotoxy
    mov al, [ascii]
    mov ah, [scan]
    cmp al, 0
    jne .ascii
    ; AL=0 = extended key (F-keys, arrows, etc.)
    mov dx, k_ext
    call puts
    mov dh, 9
    mov dl, 22
    call gotoxy
    mov al, [scan]
    call puthex
    pop cx
    pop ax
    ret

.ascii:
    cmp al, 32
    jb .ctrl
    cmp al, 126
    ja .ctrl
    ; Printable ASCII
    mov ah, 0Ah
    mov bh, 0
    mov cx, 1
    int 10h
    pop cx
    pop ax
    ret

.ctrl:
    mov al, [ascii]
    cmp al, 8
    jne .t1
    mov dx, k_bs
    jmp .named
.t1:cmp al, 9
    jne .t2
    mov dx, k_tab
    jmp .named
.t2:cmp al, 10
    jne .t3
    mov dx, k_lf
    jmp .named
.t3:cmp al, 13
    jne .t4
    mov dx, k_enter
    jmp .named
.t4:cmp al, 27
    jne .t5
    mov dx, k_esc
    jmp .named
.t5:cmp al, 1
    jne .t6
    mov dx, k_ctrla
    jmp .named
.t6:cmp al, 3
    jne .t7
    mov dx, k_ctrlc
    jmp .named
.t7:cmp al, 26
    jne .t8
    mov dx, k_ctrlz
    jmp .named
.t8:cmp al, 24
    jne .t9
    mov dx, k_ctrlx
    jmp .named
.t9:cmp al, 22
    jne .t10
    mov dx, k_ctrlv
    jmp .named
.t10:
    ; Generic CTRL-X: show  CTRL-<letter>
    mov dx, k_ctrl
    call puts
    mov al, [ascii]
    add al, 64
    call putchar
    pop cx
    pop ax
    ret

.named:
    call puts
    pop cx
    pop ax
    ret

show_mouse:
    push ax
    push cx
    ; Mouse X,Y
    mov dh, 11
    mov dl, 11
    call gotoxy
    mov ax, [mx]
    call putdec
    mov al, ','
    call putchar
    mov ax, [my]
    call putdec
    mov cx, 4
.pad1: mov al, ' '
    call putchar
    loop .pad1
    ; Buttons
    mov dh, 13
    mov dl, 11
    call gotoxy
    test byte [mbtn], 1
    jz .lb0
    mov al, 'L'
    jmp .lb1
.lb0: mov al, '-'
.lb1: call putchar
    test byte [mbtn], 2
    jz .rb0
    mov al, 'R'
    jmp .rb1
.rb0: mov al, '-'
.rb1: call putchar
    test byte [mbtn], 4
    jz .mb0
    mov al, 'M'
    jmp .mb1
.mb0: mov al, '-'
.mb1: call putchar
    call putspc
    pop cx
    pop ax
    ret

gotoxy:
    mov ah, 02h
    mov bh, 0
    int 10h
    ret

putchar:
    push ax
    push bx
    mov ah, 0Eh
    mov bx, 7
    int 10h
    pop bx
    pop ax
    ret

putspc:
    push ax
    mov al, ' '
    call putchar
    pop ax
    ret

puts:
    push ax
    push si
    mov si, dx
.l: lodsb
    cmp al, 0
    je .d
    call putchar
    jmp .l
.d: pop si
    pop ax
    ret

puthex:
    push ax
    push ax
    shr al, 4
    call .n
    pop ax
    and al, 15
    call .n
    pop ax
    ret
.n: add al, '0'
    cmp al, '9'
    jbe .w
    add al, 7
.w: call putchar
    ret

putdec:
    push bx
    push cx
    push dx
    xor cx, cx
    mov bx, 10
.l: xor dx, dx
    div bx
    add dl, '0'
    push dx
    inc cx
    or ax, ax
    jnz .l
.p: pop ax
    call putchar
    loop .p
    pop dx
    pop cx
    pop bx
    ret

header  db 'DOSBox Pure Input Test v4', 13, 10
        db 'ESC=Exit', 13, 10, 0
l_mod   db 'Mod ', 0
l_sh    db 'SH L:', 0
l_rsep  db ' R:', 0
l_ctl   db '  CT L:', 0
l_altl  db '  AL L:', 0
l_scan  db 'Scan Code:', 0
l_ascii db 'ASCII:', 0
l_char  db 'Char:', 0
l_mouse db 'Mouse XY:', 0
l_btns  db 'Buttons:', 0
k_bs    db 'BKSP', 0
k_tab   db 'TAB', 0
k_lf    db 'LF', 0
k_enter db 'ENTER', 0
k_esc   db 'ESC', 0
k_ctrl  db 'CTRL-', 0
k_ctrla db 'CTRL-A', 0
k_ctrlc db 'CTRL-C', 0
k_ctrlz db 'CTRL-Z', 0
k_ctrlx db 'CTRL-X', 0
k_ctrlv db 'CTRL-V', 0
k_ext   db 'EXTENDED:', 0

hasMouse db 0
ascii   db 0
scan    db 0
mx      dw 0
my      dw 0
mbtn    db 0
