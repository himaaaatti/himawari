#include <init.h>
#include <kernel.h>
#include <process.h>
#include <segment.h>
#include <x86_64.h>
#include <string.h>
#include <util.h>
#include <elf.h>

// TODO
bool init_scheduler(struct task_struct *first, struct task_struct *second)
{
    start_task_array[0] = first;
    start_task_array[1] = second;
    current_task_num    = 0;
    return true;
}

void start_first_task() { start_task(start_task_array[0]); }

void start_task(struct task_struct *tsk)
{
    //    load_cr3(tsk->pml4);
    __asm__ volatile("pushq %0;\n\t"
                     "pushq %1;\n\t"
                     "pushq %2;\n\t"
                     "pushq %3;\n\t"
                     "pushq %4;\n\t"
                     "iretq;"
                     : "=m"(tsk->ss), "=m"(tsk->rsp), "=m"(tsk->rflags),
                       "=m"(tsk->cs), "=m"(tsk->entry_addr));
    //"=m"(tsk->cs), "=m"(tsk->rip));

    /*         "movq %0, %%rsp;\n\t" */
    /*         "iretq;" */
    /*         : "=m"(start_task_array[0]->stack_pointer)); */
}

void task_entry()
{
    struct task_struct *tsk;

    __asm__ volatile("movq 8(%%rbp), %0;" : "=r"(tsk));

    irq_eoi();
    __asm__ volatile("pushq %0;\n\t"
                     "pushq %1;\n\t"
                     "pushq %2;\n\t"
                     "pushq %3;\n\t"
                     "pushq %4;\n\t"
                     "iretq;"
                     : "=m"(tsk->ss), "=m"(tsk->rsp), "=m"(tsk->rflags),
                       "=m"(tsk->cs), "=m"(tsk->entry_addr));
}

void _switch_to(void) { return; }

void context_switch(struct task_struct *prev, struct task_struct *next,
                    struct trap_frame_struct *trap_frame)
{
    if (next->pml4 != NULL)
    {
        load_cr3(next->pml4);
    }

    
    memcpy(&prev->context, trap_frame, sizeof(struct trap_frame_struct));
    memcpy(trap_frame, &next->context, sizeof(struct trap_frame_struct));

/*     __asm__ volatile("movq %%rsp, %0;\n\t" */
/*                      "movq $1f, %1;\n\t" */
/*                      "movq %2, %%rsp;\n\t" */
/*                      "pushq %3;\n\t" */
/*                      "jmp _switch_to;\n\t" */
/*                      "1:;" */
/*                      : "=m"(prev->rsp), "=m"(prev->rip) */
/*                      : "m"(next->rsp), "m"(next->rip)); */
}

bool create_first_thread(uintptr_t func_addr, uintptr_t stack_end_addr,
                         int stack_size, struct task_struct *task)
{
    uintptr_t stack_head = stack_end_addr + stack_size;
    task->rip            = func_addr;
    task->cs             = KERNEL_CODE_SEGMENT;
    task->rflags         = 0x0UL | RFLAGS_IF;
    task->rsp            = stack_head;
    task->ss             = KERNEL_DATA_SEGMENT;
    task->pml4           = NULL;

    return true;
}

bool create_kernel_thread(uintptr_t func_addr, uintptr_t stack_end_addr,
                          uintmax_t stack_size, struct task_struct *task)
{
    uintptr_t stack_head = stack_end_addr + stack_size;

    stack_head = (stack_head >> 8) << 8;
    /*     task->stack_pointer = stack_head; */
    task->stack_size = stack_size;

    // task->stack_frame = (struct stack_frame*)(stack_head);

    /*     task->rip  = func_addr; */
    //    task->rip = (uintptr_t)start_kernel_thread;
    task->cs     = KERNEL_CODE_SEGMENT;
    task->rflags = 0x0UL | RFLAGS_IF;
    task->rsp    = stack_head;
    task->ss     = KERNEL_DATA_SEGMENT;

    // for start_kernel_thread argument
    *(uintptr_t *)stack_head = (uintptr_t)task;

    return true;
}

bool create_user_process(uintptr_t func_addr, uintptr_t stack_end_addr,
                         uintmax_t stack_size, struct task_struct *task)
{
    uintptr_t stack_head = stack_end_addr + stack_size;

    task->stack_size = stack_size;

    task->rip    = (uintptr_t)func_addr;
    task->cs     = USER_CODE_SEGMENT;
    task->rflags = 0x0UL | RFLAGS_IF;
    task->rsp    = stack_head;
    task->ss     = USER_DATA_SEGMENT;

    return true;
}

struct task_struct startup_processes[BOOT_MODULES_NUM];
bool setup_server_process(uintptr_t elf_header, struct task_struct *task,
                          char *name)
{
    // should be clear set
    memcpy(task->name, name, 32);
    task->pml4 = create_pml4();
    if (task->pml4 == 0)
    {
        return false;
    }
//    task->cs = SERVER_CODE_SEGMENT;
//    task->ss = SERVER_DATA_SEGMENT;
    task->context.ret_cs = SERVER_CODE_SEGMENT;
    task->context.ret_ss = SERVER_DATA_SEGMENT;

    struct elf_header *header = (struct elf_header *)elf_header;

    struct elf_program_header *p_header =
        (struct elf_program_header *)(elf_header + header->e_phoff);

    for (int i = 0; i < header->e_phnum; ++i)
    {
        uint64_t offset       = p_header[i].p_offset;
        uintptr_t load_v_addr = p_header[i].p_vaddr;
        int size              = p_header[i].p_memsz;

        switch (p_header[i].p_type)
        {
            case PT_LOAD:
                break;
            default:
                continue;
        }

        if (size == 0)
        {
            continue;
        }

        {
            char buf[32];
            itoa(load_v_addr, buf, 16);
            puts("0x");
            puts(buf);
            puts("(");
            itoa(size, buf, 16);
            puts(buf);
            puts(")");
            puts("\n");
        }

        // server process can use memory(1G)
        // TODO beyond usbale memory range
        if ((load_v_addr + size) >= 0x40000000)
        {
            return false;
        }

        uint64_t *pdpt = (uint64_t *)early_malloc(1);
        if (pdpt == 0)
        {
            return false;
        }
        memset(pdpt, 0, 0x1000);

        uint64_t *pd = (uint64_t *)early_malloc(1);
        if (pd == 0)
        {
            return false;
        }
        memset(pd, 0, 0x1000);

        task->pml4[0] =
            create_entry((uintptr_t)pdpt, PAGE_READ_WRITE | PAGE_USER_SUPER);

        pdpt[0] =
            create_entry((uintptr_t)pd, PAGE_READ_WRITE | PAGE_USER_SUPER);

        int pd_index = load_v_addr / 0x200000;
        // int pd_num              = (load_v_addr + size) / 0x200000;
        int pd_num             = (size / 0x200000) + 1;
        uintptr_t v_addr_floor = round_down(load_v_addr, 0x1000);
        for (int j = 0; j < pd_num; ++j)
        {
            uint64_t *pt = (uint64_t *)early_malloc(1);
            if (pt == 0)
            {
                return false;
            }
            memset(pt, 0, 0x1000);

            for (int k = 0; k < 512; ++k)
            {

                uintptr_t physical_addr = early_malloc(1);
                uintptr_t load_to_addr =
                    physical_addr + load_v_addr - v_addr_floor;

                memcpy((void *)load_to_addr, (void *)(uintptr_t)header + offset,
                       size);

                pt[i] = create_entry(physical_addr,
                                     PAGE_READ_WRITE | PAGE_USER_SUPER);

                // TODO below branch should replace above ?
                if ((v_addr_floor + ((uintptr_t)k << 12) + (j * 0x200000)) >
                    (load_v_addr + size))
                {
                    break;
                }
            }
            pd[j + pd_index] =
                create_entry((uintptr_t)pt, PAGE_READ_WRITE | PAGE_USER_SUPER);
        }
    }

//    task->rip        = (uintptr_t)task_entry;
//    task->entry_addr = header->e_entry;
//    task->rflags     = 0x0UL | RFLAGS_IF;

    task->context.ret_rip = (uintptr_t)header->e_entry;
    task->context.ret_rflags = 0x0UL | RFLAGS_IF;

    // TODO this is tempolary stack address
    // You should change to 0x0000 7fff ffff f000
    // and setup page tables
//    task->rsp = 0x1000;
    task->context.ret_rsp = 0x1000;

    // setup for stack
    uint64_t *pdpt =
        (uint64_t *)((uintptr_t)(task->pml4[0] & (0xfffffffffffff000)) +
                     START_KERNEL_MAP);
    uint64_t *pd, *pt;

    if (pdpt[0] == 0)
    {
        pd = (uint64_t *)early_malloc(1);
        if (pd == 0)
        {
            return false;
        }
        memset(pd, 0, 0x1000);
        pdpt[0] =
            create_entry((uintptr_t)pd, PAGE_READ_WRITE | PAGE_USER_SUPER);
    }
    else
    {
        pd = (uint64_t *)((uintptr_t)(pdpt[0] & 0xfffffffffffff000) +
                          START_KERNEL_MAP);
    }

    if (pd[0] == 0)
    {
        pt = (uint64_t *)early_malloc(1);
        if (pt == 0)
        {
            return false;
        }
        memset(pt, 0, 0x1000);
        pd[0] = create_entry((uintptr_t)pt, PAGE_READ_WRITE | PAGE_USER_SUPER);
    }
    else
    {
        pt = (uint64_t *)(uintptr_t)((pd[0] & 0xfffffffffffff000) +
                                     START_KERNEL_MAP);
    }

    uint64_t *stack_addr = (uint64_t *)early_malloc(1);
    if (stack_addr == 0)
    {
        return false;
    }
    memset(stack_addr, 0, 0x1000);
    pt[0] =
        create_entry((uintptr_t)stack_addr, PAGE_READ_WRITE | PAGE_USER_SUPER);

    /*    stack_addr = (uint64_t*)early_malloc(1);
        if (stack_addr == 0)
        {
            return false;
        }
        memset(stack_addr, 0, 0x1000);
        pt[1] = create_entry((uintptr_t)stack_addr, PAGE_READ_WRITE |
       PAGE_USER_SUPER);

        */

    uintptr_t stack_head = ((uintptr_t)stack_addr + 0x1000);
//    task->rsp -= sizeof(uint64_t);
//    task->rsp -= sizeof(uint64_t);
    //*(stack_head--) = task->ss;
    //*(stack_head--) = task->rsp;
    //*(stack_head--) = task->rflags;
    //*(stack_head--) = task->cs;
    //*(stack_head--) = task->rip;
//    *(struct task_struct **)(stack_head - sizeof(uint64_t)) = task;

    return true;
}