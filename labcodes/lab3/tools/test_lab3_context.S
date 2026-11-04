    .text
    .align 2
    .globl test_lab3_register_context
test_lab3_register_context:
    # Keep gp and tp intact because the C trap handler may rely on them.
    li t0, 0x105
    li t1, 0x106
    li t2, 0x107
    li s0, 0x108
    li s1, 0x109
    li a1, 0x10b
    li a2, 0x10c
    li a3, 0x10d
    li a4, 0x10e
    li a5, 0x10f
    li a6, 0x110
    li a7, 0x111
    li s2, 0x112
    li s3, 0x113
    li s4, 0x114
    li s5, 0x115
    li s6, 0x116
    li s7, 0x117
    li s8, 0x118
    li s9, 0x119
    li s10, 0x11a
    li s11, 0x11b
    li t3, 0x11c
    li t4, 0x11d
    li t5, 0x11e
    li t6, 0x11f

    ebreak

    li a0, 0x105
    bne t0, a0, .Lcontext_fail
    li a0, 0x106
    bne t1, a0, .Lcontext_fail
    li a0, 0x107
    bne t2, a0, .Lcontext_fail
    li a0, 0x108
    bne s0, a0, .Lcontext_fail
    li a0, 0x109
    bne s1, a0, .Lcontext_fail
    li a0, 0x10b
    bne a1, a0, .Lcontext_fail
    li a0, 0x10c
    bne a2, a0, .Lcontext_fail
    li a0, 0x10d
    bne a3, a0, .Lcontext_fail
    li a0, 0x10e
    bne a4, a0, .Lcontext_fail
    li a0, 0x10f
    bne a5, a0, .Lcontext_fail
    li a0, 0x110
    bne a6, a0, .Lcontext_fail
    li a0, 0x111
    bne a7, a0, .Lcontext_fail
    li a0, 0x112
    bne s2, a0, .Lcontext_fail
    li a0, 0x113
    bne s3, a0, .Lcontext_fail
    li a0, 0x114
    bne s4, a0, .Lcontext_fail
    li a0, 0x115
    bne s5, a0, .Lcontext_fail
    li a0, 0x116
    bne s6, a0, .Lcontext_fail
    li a0, 0x117
    bne s7, a0, .Lcontext_fail
    li a0, 0x118
    bne s8, a0, .Lcontext_fail
    li a0, 0x119
    bne s9, a0, .Lcontext_fail
    li a0, 0x11a
    bne s10, a0, .Lcontext_fail
    li a0, 0x11b
    bne s11, a0, .Lcontext_fail
    li a0, 0x11c
    bne t3, a0, .Lcontext_fail
    li a0, 0x11d
    bne t4, a0, .Lcontext_fail
    li a0, 0x11e
    bne t5, a0, .Lcontext_fail
    li a0, 0x11f
    bne t6, a0, .Lcontext_fail
    li a0, 0
    ret

.Lcontext_fail:
    li a0, 1
    ret
