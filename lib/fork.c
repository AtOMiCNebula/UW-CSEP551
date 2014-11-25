// implement fork from user space

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// copy-on-write page.  If not, panic.
	if (!(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW)) {
		panic("Cannot handle non-CoW page fault: %08x", addr);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W);
	if (r < 0) {
		panic("sys_page_alloc failed with: %e", r);
	}

	void* rounded_addr = ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, rounded_addr, PGSIZE);

	r = sys_page_map(0, PFTEMP, 0, rounded_addr, PTE_P|PTE_U|PTE_W);
	if (r < 0) {
		panic("sys_page_map failed with: %e", r);
	}

	r = sys_page_unmap(0, PFTEMP);
	if (r < 0) {
		panic("sys_page_unmap failed with: %e", r);
	}
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	int pte = uvpt[pn];
	void* addr = (void*)(pn*PGSIZE);

	// Get allowed permissions out of PTE, and make sure CoW is included, if
	// page is writeable.
	int perm = pte & (PTE_AVAIL|PTE_P|PTE_U);
	if (pte & (PTE_W|PTE_COW)) {
		perm |= PTE_COW;
	}

	// Map page into new environment.
	r = sys_page_map(0, addr, envid, addr, perm);
	if (r < 0) {
		panic("sys_page_map failed with: %e", r);
	}

	// If the new page is CoW, then we need to remap our page with CoW too.
	// Otherwise, we're just done here.
	if (perm & PTE_COW) {
		perm = ((pte & (PTE_P|PTE_U|PTE_AVAIL)) | PTE_COW);
		return sys_page_map(0, addr, 0, addr, perm);
	} else {
		return 0;
	}
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// Set up our page fault handler appropriately.
	set_pgfault_handler(pgfault);

	// Create a child.
	envid_t envid = sys_exofork();
	if (envid < 0) {
		panic("sys_exofork failed with: %e", envid);
	}

	if (envid == 0) {
		// Remember to fix "thisenv" in the child process.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// Alloc a brand new page, just for the child's exception stack.
	int r = sys_page_alloc(envid, (void*) (UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W);
	if (r) {
		panic("sys_page_alloc failed with: %e", r);
	}

	// In the parent.  Dupe pages, starting one page after UTOP (because we
	// already fixed up UXSTACKTOP above) and work downward.
	int page_num = (UTOP-PGSIZE) / PGSIZE;
	while (--page_num >= 0) {
		uint32_t dir_num = page_num >> (PDXSHIFT - PTXSHIFT);
		if (!(uvpd[dir_num] & PTE_P)) {
			// The directory entry for these pages is not present. Move down to
			// the next dir entry.
			page_num -= NPTENTRIES;
		}
		else if (uvpt[page_num] & PTE_P) {
			// This page is present, should dupe it.
			duppage(envid, page_num);
		}
	}

	sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);

	// Mark the child as runnable and return.
	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if (r < 0) {
		panic("sys_env_set_status failed with: %e", r);
	}

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
