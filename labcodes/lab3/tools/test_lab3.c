#include <clock.h>
#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <test_lab3.h>
#include <trap.h>

extern uintptr_t test_lab3_register_context(void);
extern void interrupt_handler(struct trapframe *tf);
extern void exception_handler(struct trapframe *tf);

static void
test_lab3_illegal_instruction(void)
{
    /* Custom opcode 0x7b is invalid on the target RV64 platform and is 4 bytes. */
    asm volatile(".word 0x0000007b");
}

static void
test_lab3_handler_fallbacks(void)
{
    struct trapframe tf;

    memset(&tf, 0, sizeof(tf));
    tf.cause = IRQ_S_SOFT | ((uintptr_t)1 << 63);
    interrupt_handler(&tf);

    tf.cause = 15;
    interrupt_handler(&tf);

    tf.cause = 20;
    exception_handler(&tf);
    cprintf("[Test 2] handler fallback completed\\n");
}

void
test_lab3_exception_context(void)
{
#if defined(GRADE_LAB3_CONTEXT)
    if (test_lab3_register_context() != 0) {
        cprintf("[Test 1] Trap entry and breakpoint return: FAIL (register context)\n");
        return;
    }

    cprintf("[Test 1] Trap entry and breakpoint return: PASS (35/35)\n");
#elif defined(GRADE_LAB3_ILLEGAL)
    test_lab3_illegal_instruction();
    test_lab3_handler_fallbacks();
    cprintf("[Test 2] Illegal instruction return: PASS (25/25)\n");
#endif
}

void
test_lab3_timer_is_armed(void)
{
#if defined(GRADE_LAB3_TIMER)
    if (ticks != 0) {
        cprintf("[Test 3] Sustained supervisor timer: FAIL (ticks before enable=%d)\n",
                (int)ticks);
    }
#endif
}

void
test_lab3_wait_for_timer(void)
{
#if defined(GRADE_LAB3_TIMER)
    while (ticks < 200) {
        asm volatile("" ::: "memory");
    }

    cprintf("[Test 3] Sustained supervisor timer: PASS (40/40, ticks=%d)\n",
            (int)ticks);
    while (1) {
    }
#endif
}
