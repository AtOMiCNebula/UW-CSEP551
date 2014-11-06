// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display the current stack backtrace", mon_backtrace },
	{ "rainbow", "Display a rainbow of colorful text", mon_rainbow },
	{ "dumptable", "Display a given page table (defaults to pgdir)", mon_dumptable },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int safestackread(void* va)
{
	static struct {
		void* base;
		int size;
	} regions[] = {
		{ (void*)KSTACKTOP, KSTKSIZE },
		{ (void*)USTACKTOP, PGSIZE },
		{ (void*)UXSTACKTOP, PGSIZE } };

	bool safe = false;
	int i;
	for (i = 0; i < (sizeof(regions)/sizeof(regions[0])); i++) {
		safe = (regions[i].base-regions[i].size <= va && va < regions[i].base);
		if (safe) {
			break;
		}
	}

	if (safe) {
		return *((int*)va);
	}
	else {
		return 0xBADF00D;
	}
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Stack backtrace:\n");

	int* stackbase = (int*)read_ebp();
	while (stackbase != NULL)
	{
		int next_stackbase = stackbase[0];
		int eip = stackbase[1];
		int arg0 = safestackread(&stackbase[2]);
		int arg1 = safestackread(&stackbase[3]);
		int arg2 = safestackread(&stackbase[4]);
		int arg3 = safestackread(&stackbase[5]);
		int arg4 = safestackread(&stackbase[6]);
		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", stackbase, eip, arg0, arg1, arg2, arg3, arg4);

		struct Eipdebuginfo info;
		debuginfo_eip(eip, &info);
		int offset = (eip - info.eip_fn_addr);
		cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, offset);

		stackbase = (int*)next_stackbase;
	}

	return 0;
}

int
mon_rainbow(int argc, char **argv, struct Trapframe *tf)
{
	int colors[] = { 0, 1, 3, 2, 6, 4, 5, 7 };
	int height = 16;
	int width = 60;

	int y;
	int x;

	cprintf("/");
	for (x = 0; x < width; x++)
	{
		cprintf("-");
	}
	cprintf("\\\n");

	for (y = 0; y < height; y++)
	{
		cprintf("|");
		for (x = 0; x < width; x++)
		{
			cprintf("\033[%d;3%d;4%dmO", (x%2), colors[(y+x)%8], colors[((y+x)/8)%8]);
		}
		cprintf("\033[0m|\n");
	}

	cprintf("\\");
	for (x = 0; x < width; x++)
	{
		cprintf("-");
	}
	cprintf("/\n");

	return 0;
}

int
atox(char* input)
{
	if (input[0] != '0' || input[1] != 'x') {
		cprintf("atox: Expected \"0x\"...\n");
		return 0;
	}
	input += 2;

	int val = 0;
	while (input[0] != '\0') {
		val *= 16;
		if ('0' <= input[0] && input[0] <= '9') {
			val += (input[0] - '0');
		}
		else if ('A' <= input[0] && input[0] <= 'F') {
			val += (input[0] - 'A' + 10);
		}
		else if ('a' <= input[0] && input[0] <= 'f') {
			val += (input[0] - 'a' + 10);
		}
		else {
			cprintf("atox: Unexpected digit '%c' (0x%02x)\n", input[0], input[0]);
			return 0;
		}
		input++;
	}
	return val;
}

int
mon_dumptable(int argc, char **argv, struct Trapframe *tf)
{
	pte_t* table;
	if (argc > 1) {
		table = (pte_t*)atox(argv[1]);
	}
	else {
		extern pde_t *kern_pgdir;
		table = kern_pgdir;
	}
	cprintf("Dumping page table at 0x%08x\n", table);

	int notmapped = 0;
	size_t i;
	for (i = 0; i < (PGSIZE / sizeof(pte_t)); i++) {
		pte_t pt = table[i];
		if (pt & PTE_P) {
			cprintf("%4d:    0x%08x    PTE_P", i, PTE_ADDR(pt));

			#define PRINTFLAG(flag)	if(pt&flag){cprintf(","#flag);}
			PRINTFLAG(PTE_W);
			PRINTFLAG(PTE_U);
			PRINTFLAG(PTE_PWT);
			PRINTFLAG(PTE_PCD);
			PRINTFLAG(PTE_A);
			PRINTFLAG(PTE_D);
			PRINTFLAG(PTE_PS);
			PRINTFLAG(PTE_G);
			#undef PRINTFLAG

			cprintf("\n");
		}
		else {
			notmapped++;
		}
	}
	cprintf("(also %d unmapped pages)\n", notmapped);

	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
