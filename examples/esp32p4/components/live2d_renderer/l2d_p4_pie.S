/* ESP32-P4 only. All loads/stores are 16-byte aligned. No quantization:
 * the masks implement the same RGB565 truncation as the portable C path.
 * QR registers are caller-saved; FreeRTOS manages lazy PIE task context.
 * Only caller-saved scalar registers are used; SAR is restored on return.
 */
    .section .rodata.l2d_pie_masks,"a"
    .balign 16
.Lrgb_masks:
    .word 0xf800,0xf800,0xf800,0xf800
    .word 0x07e0,0x07e0,0x07e0,0x07e0
    .word 0x001f,0x001f,0x001f,0x001f
    .text
    .balign 4
    .global l2d_p4_pie_rgb565_blocks
    .type l2d_p4_pie_rgb565_blocks,@function
l2d_p4_pie_rgb565_blocks:
    # a0=src, a1=dst, a2=number of 8-pixel blocks (positive)
    esp.movx.r.sar a4
    la a3,.Lrgb_masks
    esp.vld.128.ip q4,a3,16
    esp.vld.128.ip q5,a3,16
    esp.vld.128.ip q6,a3,16
.Lrgb_loop:
    esp.vld.128.ip q0,a0,16
    esp.vld.128.ip q1,a0,16
    li a5,8
    esp.movx.w.sar a5
    esp.vsr.u32 q2,q0
    esp.vsr.u32 q3,q1
    esp.andq q2,q2,q4
    esp.andq q3,q3,q4
    li a5,5
    esp.movx.w.sar a5
    esp.vsr.u32 q7,q0
    esp.andq q7,q7,q5
    esp.orq q2,q2,q7
    esp.vsr.u32 q7,q1
    esp.andq q7,q7,q5
    esp.orq q3,q3,q7
    li a5,3
    esp.movx.w.sar a5
    esp.vsr.u32 q0,q0
    esp.andq q0,q0,q6
    esp.orq q0,q0,q2
    esp.vsr.u32 q1,q1
    esp.andq q1,q1,q6
    esp.orq q1,q1,q3
    esp.vunzip.16 q0,q1
    esp.vst.128.ip q0,a1,16
    addi a2,a2,-1
    bnez a2,.Lrgb_loop
    esp.movx.w.sar a4
    ret
    .size l2d_p4_pie_rgb565_blocks,.-l2d_p4_pie_rgb565_blocks

    .balign 4
    .global l2d_p4_pie_zero_blocks
    .type l2d_p4_pie_zero_blocks,@function
l2d_p4_pie_zero_blocks:
    # a0=destination, a1=number of 16-byte blocks (positive)
    esp.zero.q q0
.Lzero_loop:
    esp.vst.128.ip q0,a0,16
    addi a1,a1,-1
    bnez a1,.Lzero_loop
    ret
    .size l2d_p4_pie_zero_blocks,.-l2d_p4_pie_zero_blocks
