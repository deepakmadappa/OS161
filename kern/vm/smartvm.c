/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void
vm_bootstrap(void)
{
	g_coremap.lkcore = lock_create("corelock");
	KASSERT(g_coremap.lkcore != NULL);
	paddr_t firstaddr, lastaddr;
	ram_getsize(&firstaddr, &lastaddr);
	g_coremap.numpages = ROUNDDOWN(lastaddr, PAGE_SIZE) / PAGE_SIZE;
	g_coremap.physicalpages = (struct page*) PADDR_TO_KVADDR(firstaddr);
	g_coremap.freeaddr = firstaddr + g_coremap.numpages * sizeof(struct page);
	g_coremap.firstaddr = firstaddr;
	g_coremap.lastaddr = lastaddr;

	int pad = (g_coremap.freeaddr % PAGE_SIZE) == 0 ? 0:1;
	uint32_t ncpages = g_coremap.freeaddr/PAGE_SIZE + pad;
	uint32_t i = 0;
	for(i = 0; i< ncpages; i++)
	{
		g_coremap.physicalpages[i].state = PAGE_FIXED;
	}
	for(;i<g_coremap.numpages;i++)
	{
		g_coremap.physicalpages[i].state = PAGE_FREE;
	}
	g_coremap.bisbootstrapdone = true;


	//TODO set flag for alloc kpages


	//core_map =(struct page*) kmalloc(npages * sizeof(struct page));
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
paddr_t
alloc_kpages(int npages)
{
	lock_acquire(g_coremap.lkcore);
	if(g_coremap.bisbootstrapdone != true)
	{
		paddr_t pa;
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		return PADDR_TO_KVADDR(pa);
	}
	else
	{
		//allocate from coremap
		if(npages == 1)
		{
			return allocate_page();
		}
		else
		{
			return allocate_multiplepages(npages);
		}
	}
	lock_release(g_coremap.lkcore);
}

paddr_t allocate_page()
{

	for(uint32_t i=0;i<g_coremap.numpages;i++)
	{
		if( g_coremap.physicalpages[i].state == PAGE_FREE)
		{
			g_coremap.physicalpages[i].numallocations = 1;
			g_coremap.physicalpages[i].state = PAGE_FIXED;
			return g_coremap.physicalpages[i].pa;
		}
	}
	panic("there is no free page");
	//TODO: need to evict here
	return NULL;
}

paddr_t allocate_multiplepages(int npages)
{

	uint32_t i=0;
	int count = 0;
	while(i< g_coremap.numpages - npages)
	{
		if(g_coremap.physicalpages[i].state == PAGE_FREE)
		{
			count++;
		}
		else
		{
			count = 0;
		}
		if(count == npages)
			break;
		i++;
	}
	if(count == npages)
	{
		for(int j = i - npages + 1; j <= i ; j++)
		{
			g_coremap.physicalpages[i].state = PAGE_FIXED;
		}
	}
	else
	{
		panic("insufficient contiguous pages");
		//TODO:Need to call evict here and make enuf room
	}
	g_coremap.physicalpages[i-npages +1].numallocations = npages;
	return g_coremap.physicalpages[i-npages +1].pa;
}

void
free_kpages(paddr_t addr)
{
	lock_acquire(g_coremap.lkcore);

	int startindex = (addr - g_coremap.firstaddr)/PAGE_SIZE;
	KASSERT(startindex + g_coremap.physicalpages[startindex].numallocations < g_coremap.numpages);
	for(int i=startindex; i< startindex + g_coremap.physicalpages[startindex].numallocations; i++ )
	{
		KASSERT(g_coremap.physicalpages[i].state != PAGE_FREE);
		g_coremap.physicalpages[i].state = PAGE_FREE;
	}
	lock_release(g_coremap.lkcore);
}


void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	(void)faulttype; (void)faultaddress;
	return 0;
	/*
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		 We always create pages read-write, so we can't get this
		panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	as = curthread->t_addrspace;
	if (as == NULL) {

	 * No address space set up. This is probably a kernel
	 * fault early in boot. Return EFAULT so as to panic
	 * instead of getting into an infinite faulting loop.

		return EFAULT;
	}

	 Assert that the address space has been set up properly.
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	 make sure it's page-aligned
	KASSERT((paddr & PAGE_FRAME) == paddr);

	 Disable interrupts on this CPU while frobbing the TLB.
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
	 */
}
