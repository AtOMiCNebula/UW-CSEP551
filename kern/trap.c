#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

extern void int0();
extern void int1();
extern void int2();
extern void int3();
extern void int4();
extern void int5();
extern void int6();
extern void int7();
extern void int8();
extern void int9();
extern void int10();
extern void int11();
extern void int12();
extern void int13();
extern void int14();
extern void int15();
extern void int16();
extern void int17();
extern void int18();
extern void int19();
extern void int20();
extern void int21();
extern void int22();
extern void int23();
extern void int24();
extern void int25();
extern void int26();
extern void int27();
extern void int28();
extern void int29();
extern void int30();
extern void int31();
extern void int32();
extern void int33();
extern void int34();
extern void int35();
extern void int36();
extern void int37();
extern void int38();
extern void int39();
extern void int40();
extern void int41();
extern void int42();
extern void int43();
extern void int44();
extern void int45();
extern void int46();
extern void int47();
extern void int48();	// syscall

void
trap_init(void)
{
	extern struct Segdesc gdt[];

	SETGATE(idt[0], false, GD_KT, int0, 0);
	SETGATE(idt[1], false, GD_KT, int1, 0);
	SETGATE(idt[2], false, GD_KT, int2, 0);
	SETGATE(idt[3], false, GD_KT, int3, 3);
	SETGATE(idt[4], false, GD_KT, int4, 0);
	SETGATE(idt[5], false, GD_KT, int5, 0);
	SETGATE(idt[6], false, GD_KT, int6, 0);
	SETGATE(idt[7], false, GD_KT, int7, 0);
	SETGATE(idt[8], false, GD_KT, int8, 0);
	SETGATE(idt[9], false, GD_KT, int9, 0);
	SETGATE(idt[10], false, GD_KT, int10, 0);
	SETGATE(idt[11], false, GD_KT, int11, 0);
	SETGATE(idt[12], false, GD_KT, int12, 0);
	SETGATE(idt[13], false, GD_KT, int13, 0);
	SETGATE(idt[14], false, GD_KT, int14, 0);
	SETGATE(idt[15], false, GD_KT, int15, 0);
	SETGATE(idt[16], false, GD_KT, int16, 0);
	SETGATE(idt[17], false, GD_KT, int17, 0);
	SETGATE(idt[18], false, GD_KT, int18, 0);
	SETGATE(idt[19], false, GD_KT, int19, 0);
	SETGATE(idt[20], false, GD_KT, int20, 0);
	SETGATE(idt[21], false, GD_KT, int21, 0);
	SETGATE(idt[22], false, GD_KT, int22, 0);
	SETGATE(idt[23], false, GD_KT, int23, 0);
	SETGATE(idt[24], false, GD_KT, int24, 0);
	SETGATE(idt[25], false, GD_KT, int25, 0);
	SETGATE(idt[26], false, GD_KT, int26, 0);
	SETGATE(idt[27], false, GD_KT, int27, 0);
	SETGATE(idt[28], false, GD_KT, int28, 0);
	SETGATE(idt[29], false, GD_KT, int29, 0);
	SETGATE(idt[30], false, GD_KT, int30, 0);
	SETGATE(idt[31], false, GD_KT, int31, 0);
	SETGATE(idt[32], false, GD_KT, int32, 0);
	SETGATE(idt[33], false, GD_KT, int33, 0);
	SETGATE(idt[34], false, GD_KT, int34, 0);
	SETGATE(idt[35], false, GD_KT, int35, 0);
	SETGATE(idt[36], false, GD_KT, int36, 0);
	SETGATE(idt[37], false, GD_KT, int37, 0);
	SETGATE(idt[38], false, GD_KT, int38, 0);
	SETGATE(idt[39], false, GD_KT, int39, 0);
	SETGATE(idt[40], false, GD_KT, int40, 0);
	SETGATE(idt[41], false, GD_KT, int41, 0);
	SETGATE(idt[42], false, GD_KT, int42, 0);
	SETGATE(idt[43], false, GD_KT, int43, 0);
	SETGATE(idt[44], false, GD_KT, int44, 0);
	SETGATE(idt[45], false, GD_KT, int45, 0);
	SETGATE(idt[46], false, GD_KT, int46, 0);
	SETGATE(idt[47], false, GD_KT, int47, 0);
	SETGATE(idt[48], false, GD_KT, int48, 3);

	// Per-CPU setup
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	thiscpu->cpu_ts.ts_esp0 = (KSTACKTOP - cpunum() * (KSTKSIZE + KSTKGAP));
	thiscpu->cpu_ts.ts_ss0 = GD_KD;

	// Initialize the TSS slot of the gdt.
	gdt[(GD_TSS0 >> 3) + cpunum()] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate), 0);
	gdt[(GD_TSS0 >> 3) + cpunum()].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(((GD_TSS0 >> 3) + cpunum()) << 3);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	uint32_t ret;
	switch (tf->tf_trapno) {
		case T_BRKPT:
			monitor(tf);
			return;
		case T_PGFLT:
			page_fault_handler(tf);
			return;
		case T_SYSCALL:
			ret = syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx, tf->tf_regs.reg_ecx,
										tf->tf_regs.reg_ebx, tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
			tf->tf_regs.reg_eax = ret;
			return;
		default:
			break;
	}

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		lapic_eoi();
		sched_yield();
		return; // yield doesn't return, but just in case...
	}

	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		lock_kernel();
		assert(curenv);

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.
	if ((tf->tf_cs & 3) == 0) {
		panic("page_fault_handler: kernel should not have hit a page fault!");
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	if (curenv->env_pgfault_upcall) {
		uintptr_t ux_stack_top = UXSTACKTOP;

		// Check if we were already handling a page fault, and if so, don't
		// overwrite what's already on the stack, and leave another dword free.
		if (tf->tf_esp < UXSTACKTOP && tf->tf_esp >= (UXSTACKTOP - PGSIZE)) {
			ux_stack_top = tf->tf_esp - sizeof(uintptr_t);
		}

		ux_stack_top -= sizeof(struct UTrapframe);
		struct UTrapframe* utf = (struct UTrapframe*) ux_stack_top;
		user_mem_assert(curenv, utf, sizeof (struct UTrapframe), PTE_W);

		// Copy relevant details from original trap frame
		utf->utf_fault_va = fault_va;
		utf->utf_err = tf->tf_err;
		utf->utf_regs = tf->tf_regs;
		utf->utf_eip = tf->tf_eip;
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_esp = tf->tf_esp;

		tf->tf_esp = ux_stack_top;
		tf->tf_eip = (uintptr_t)(curenv->env_pgfault_upcall);
		env_run(curenv);
	}



	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

