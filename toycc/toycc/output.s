    .text
    .globl main
    main:
    addi sp, sp, -256
    li t0, 0
    sw t0, -4(sp)
    li t1, 0
    sw t1, -8(sp)
    loop_0:
    lw t3, -8(sp)
    li t4, 10
    slt t2, t3, t4
    beqz t2, endloop_1
    lw t6, -8(sp)
    li t0, 5
    sub t5, t6, t0
    seqz t5, t5
    beqz t5, else_2
    lw t2, -8(sp)
    li t3, 1
    add t1, t2, t3
    sw t1, -8(sp)
    j loop_0
    j endif_3
    else_2:
    endif_3:
    lw t5, -8(sp)
    li t6, 8
    sub t4, t5, t6
    seqz t4, t4
    beqz t4, else_4
    j endloop_1
    j endif_5
    else_4:
    endif_5:
    lw t1, -4(sp)
    lw t3, -8(sp)
    mv a0, t3
    li t4, 1
    mv a1, t4
    call add
    mv t2, a0
    add t0, t1, t2
    sw t0, -4(sp)
    lw t6, -8(sp)
    li t0, 1
    add t5, t6, t0
    sw t5, -8(sp)
    j loop_0
    endloop_1:
    lw t1, -4(sp)
    mv a0, t1
    addi sp, sp, 256
    ret
    add:
    addi sp, sp, -256
    lw t3, -4(sp)
    lw t4, -8(sp)
    add t2, t3, t4
    mv a0, t2
    addi sp, sp, 256
    ret
