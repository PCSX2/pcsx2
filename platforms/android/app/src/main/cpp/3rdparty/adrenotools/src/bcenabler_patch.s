        cmp     w0, 131
        bne     .L2
        mov     w0, 71
        b       .L3
.L2:
        cmp     w0, 133
        bne     .L4
        mov     w0, 71
        b       .L3
.L4:
        cmp     w0, 132
        bne     .L5
        mov     w0, 72
        b       .L3
.L5:
        cmp     w0, 134
        bne     .L6
        mov     w0, 72
        b       .L3
.L6:
        cmp     w0, 135
        bne     .L7
        mov     w0, 74
        b       .L3
.L7:
        cmp     w0, 136
        bne     .L8
        mov     w0, 75
        b       .L3
.L8:
        cmp     w0, 137
        bne     .L9
        mov     w0, 77
        b       .L3
.L9:
        cmp     w0, 138
        bne     .L10
        mov     w0, 78
        b       .L3
.L10:
        cmp     w0, 139
        bne     .L11
        mov     w0, 80
        b       .L3
.L11:
        cmp     w0, 140
        bne     .L12
        mov     w0, 81
        b       .L3
.L12:
        cmp     w0, 141
        bne     .L13
        mov     w0, 83
        b       .L3
.L13:
        cmp     w0, 142
        bne     .L14
        mov     w0, 84
        b       .L3
.L14:
        cmp     w0, 143
        bne     .L15
        mov     w0, 95
        b       .L3
.L15:
        cmp     w0, 144
        bne     .L16
        mov     w0, 96
        b       .L3
.L16:
        cmp     w0, 145
        bne     .L17
        mov     w0, 98
        b       .L3
.L17:
        cmp     w0, 146
        bne     .L18
        mov     w0, 99
        b       .L3
.L18:
        mov     w0, 0
.L3:
        .word   0xffffffff  // Branch fixup
