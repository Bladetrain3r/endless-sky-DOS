; SPDX-License-Identifier: GPL-3.0-or-later
; Diagnostic only: VBE 800x600x8 banked fill, not an Endless Sky benchmark.
; nasm -f bin vbe_fill.asm -o VBEFILL.COM
bits 16
org 100h
start:
    push cs
    pop ds
    push cs
    pop es
    mov ax,4f01h
    mov cx,103h
    mov di,mode_info
    int 10h
    cmp ax,004fh
    jne failed
    test word [mode_info],1
    jz failed
    mov al,[mode_info+2]       ; window A must exist and be writable
    and al,5
    cmp al,5
    jne failed
    cmp word [mode_info+8],0a000h
    jne failed
    cmp word [mode_info+16],800
    jne failed
    cmp word [mode_info+18],800
    jne failed
    cmp word [mode_info+20],600
    jne failed
    cmp byte [mode_info+25],8
    jne failed
    cmp word [mode_info+6],64 ; window size at least 64KiB
    jb failed
    mov cx,[mode_info+4]     ; bank granularity in KiB
    test cx,cx
    jz failed
    mov ax,64
    xor dx,dx
    div cx
    test dx,dx
    jnz failed
    mov [bank_step],ax
    mov ax,4f02h
    mov bx,103h
    int 10h
    cmp ax,004fh
    jne failed
    mov ah,0
    int 1ah
    mov [start_lo],dx
    mov [start_hi],cx
    mov bp,1200
frame:
    mov word [bank],0
    mov byte [banks_left],8
bank_loop:
    mov ax,4f05h
    xor bx,bx
    mov dx,[bank]
    int 10h
    cmp ax,004fh
    jne failed
    mov ax,0a000h
    mov es,ax
    xor di,di
    mov cx,32768
    cmp byte [banks_left],1
    jne fill
    mov cx,10624             ; 480000 - 7*65536 bytes, divided by 2
fill:
    mov al,[color]
    xor al,[banks_left]
    mov ah,al
    cld
    rep stosw
    mov ax,[bank_step]
    add [bank],ax
    dec byte [banks_left]
    jnz bank_loop
    inc byte [color]
    dec bp
    jnz frame
    mov ah,0
    int 1ah
    sub dx,[start_lo]
    sbb cx,[start_hi]
    ; BIOS clock wraps at midnight after 0x1800B0 ticks.
    jnc elapsed
    add dx,00b0h
    adc cx,0018h
elapsed:
    mov [ticks_lo],dx
    mov [ticks_hi],cx
    ; Verify first/last byte of each bank after timing. Distinct bank colors
    ; catch ignored/aliased bank switches as well as missing final bank writes.
    mov word [bank],0
    mov byte [banks_left],8
verify_bank:
    mov ax,4f05h
    xor bx,bx
    mov dx,[bank]
    int 10h
    cmp ax,004fh
    jne failed
    mov ax,0a000h
    mov es,ax
    mov al,[color]
    dec al
    xor al,[banks_left]
    cmp [es:0],al
    jne failed
    mov di,65535
    cmp byte [banks_left],1
    jne verify_end
    mov di,21247
verify_end:
    cmp [es:di],al
    jne failed
    mov ax,[bank_step]
    add [bank],ax
    dec byte [banks_left]
    jnz verify_bank
    mov ax,3
    int 10h
    mov dx,heading
    call puts
    mov ax,[ticks_hi]
    call hex4
    mov ax,[ticks_lo]
    call hex4
    mov dx,ending
    call puts
    mov ax,4c00h
    int 21h
failed:
    mov ax,3
    int 10h
    mov dx,error
    call puts
    mov ax,4c01h
    int 21h
puts:
    mov ah,9
    int 21h
    ret
hex4:
    mov bx,ax
    mov cx,4
hex_digit:
    rol bx,4
    mov dl,bl
    and dl,15
    add dl,'0'
    cmp dl,'9'
    jbe emit
    add dl,7
emit:
    mov ah,2
    int 21h
    loop hex_digit
    ret
heading db 'probe=vbe_banked_fill',13,10,'mode=800x600x8',13,10
        db 'frames=1200',13,10,'bios_ticks_hex=$'
ending db 13,10,'sample_check=pass',13,10,'status=ok',13,10,'$'
error db 'status=unsupported_or_vbe_error',13,10,'$'
start_lo dw 0
start_hi dw 0
ticks_lo dw 0
ticks_hi dw 0
bank_step dw 0
bank dw 0
banks_left db 0
color db 1
align 4
mode_info times 256 db 0
