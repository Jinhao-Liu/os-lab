#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2510670
void schedule(void)
{
    bool intr_flag;
    local_intr_save(intr_flag);

    current->need_resched = 0;
    list_entry_t *start = current == idleproc ? &proc_list : &current->list_link;
    list_entry_t *entry = start;
    struct proc_struct *next = idleproc;

    do
    {
        entry = list_next(entry);
        if (entry != &proc_list)
        {
            struct proc_struct *candidate = le2proc(entry, list_link);
            if (candidate->state == PROC_RUNNABLE)
            {
                next = candidate;
                break;
            }
        }
    } while (entry != start);

    next->runs++;
    proc_run(next);
    local_intr_restore(intr_flag);
}
