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
#include <kern/fcntl.h>
#include <vfs.h>
#include <uio.h>
#include <cpu.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock spinlkcore =  SPINLOCK_INITIALIZER;;

void
vm_bootstrap(void)
{
	paddr_t firstaddr, lastaddr;
	ram_getsize(&firstaddr, &lastaddr);
	g_coremap.numpages = ROUNDDOWN(lastaddr, PAGE_SIZE) / PAGE_SIZE;
	g_coremap.physicalpages = (struct memorypage*) PADDR_TO_KVADDR(firstaddr);
	g_coremap.freeaddr = firstaddr + g_coremap.numpages * sizeof(struct memorypage);
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

	g_swapper.fileend=0;
	g_swapper.lk_swapper = lock_create("swapperlock");


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
vaddr_t
alloc_kpages(int npages)
{
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
		spinlock_acquire(&spinlkcore);
		vaddr_t retaddress = 0;
		//allocate from coremap
		if(npages == 1)
		{
			retaddress = PADDR_TO_KVADDR(allocate_onepage());
		}
		else
		{
			retaddress = PADDR_TO_KVADDR(allocate_multiplepages(npages));
		}
		spinlock_release(&spinlkcore);
		return retaddress;
	}

	return 0; //just to let it compile now
}

paddr_t allocate_onepage(void)
{

	for(uint32_t i=0;i<g_coremap.numpages;i++)
	{
		if( g_coremap.physicalpages[i].state == PAGE_FREE)
		{
			g_coremap.physicalpages[i].numallocations = 1;
			g_coremap.physicalpages[i].state = PAGE_FIXED;
			return (i*PAGE_SIZE);
		}
	}

	index_t index;
	int err= chooseframetoevict(&index);
	if(err)
		panic("nopage for kernel :(");
	 err = swapout(&index, false);
	KASSERT(g_coremap.physicalpages[index].state != PAGE_FIXED && g_coremap.physicalpages[index].state != PAGE_FREE);
	if(err)
		panic("err");
	g_coremap.physicalpages[index].numallocations = 1;
	g_coremap.physicalpages[index].state = PAGE_FIXED;
	return (index*PAGE_SIZE);

	//TODO: need to evict here
	panic("no page");
	return 0;
}

paddr_t allocate_multiplepages(int npages)
{

	index_t i=0;
	int count = 0;
	bool found = false;
	while(i< g_coremap.numpages)
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
		{
			break; //found enough contiguous pages, so stop the search
			found = true;
		}
		i++;
	}
	if(!found)
	{
		count = 0;
		int err=chooseframetoevict(&i);
		if(err)
			panic("no page for kernel");
		while(i< g_coremap.numpages)
		{
			if(g_coremap.physicalpages[i].state != PAGE_FIXED)
			{
				count++;
			}
			else
			{
				count = 0;
			}
			if(count == npages)
			{
				break; //found enough contiguous pages, so stop the search
				found = true;
			}
			i++;
		}
	}
	if(count == npages)
	{
		for(index_t j = i - npages + 1; j <= i ; j++)	// counter i would have stopped after npages of our start
		{
			if(g_coremap.physicalpages[j].state!= PAGE_FREE)
			{
				index_t index = j;
				swapout(&index, false);
			}
			g_coremap.physicalpages[j].state = PAGE_FIXED;
			g_coremap.physicalpages[j].numallocations = 0;//to indicate that this is part of multiple page allocation
		}
	}
	else
	{
		panic("insufficient contiguous pages");
		//TODO:Need to call evict here and make enuf room
	}
	g_coremap.physicalpages[i-npages +1].numallocations = npages;
	return (PAGE_SIZE * (i-npages +1));
}

int allocate_userpage(struct addrspace* for_as, int uberindex, int subindex, index_t *retval)
{
	KASSERT(for_as != NULL);
	spinlock_acquire(&spinlkcore);
	for(index_t i=0;i<g_coremap.numpages;i++)
	{
		if( g_coremap.physicalpages[i].state == PAGE_FREE)
		{
			g_coremap.physicalpages[i].numallocations = 1;
			g_coremap.physicalpages[i].state = PAGE_DIRTY;
			g_coremap.physicalpages[i].as = for_as;
			g_coremap.physicalpages[i].vpage = INDECES_TO_VADDR(uberindex,subindex);
			spinlock_release(&spinlkcore);
			memset((void *)(PADDR_TO_KVADDR(i * PAGE_SIZE)), 0, PAGE_SIZE );
			*retval = i;

			return 0;
		}
	}
	*retval = -1;
	spinlock_release(&spinlkcore);
	int err = swapout(retval, true);
	if(err)
	{
		return err;
	}
	spinlock_acquire(&spinlkcore);
	g_coremap.physicalpages[*retval].numallocations = 1;
	g_coremap.physicalpages[*retval].state = PAGE_DIRTY;
	g_coremap.physicalpages[*retval].as = for_as;
	g_coremap.physicalpages[*retval].vpage = INDECES_TO_VADDR(uberindex,subindex);
	memset((void *)(PADDR_TO_KVADDR((*retval) * PAGE_SIZE)), 0, PAGE_SIZE );

	spinlock_release(&spinlkcore);

	return 0;
}

void free_userpage(index_t index)
{
	KASSERT(g_coremap.physicalpages[index].state != PAGE_FREE);
	KASSERT(g_coremap.physicalpages[index].state != PAGE_FIXED);
	//KASSERT(g_coremap.physicalpages[index].pa )
	spinlock_acquire(&spinlkcore);
	g_coremap.physicalpages[index].numallocations = 0;
	g_coremap.physicalpages[index].state = PAGE_FREE;
	g_coremap.physicalpages[index].as = NULL;
	//memset((void *)PADDR_TO_KVADDR(g_coremap.physicalpages[index].pa), 0, PAGE_SIZE );
	spinlock_release(&spinlkcore);
}

void* memset(void *ptr, int ch, size_t len)
{
	char *p = ptr;
	size_t i;

	for (i=0; i<len; i++) {
		p[i] = ch;
	}

	return ptr;
}

void
free_kpages(vaddr_t kaddr)
{
	if(g_coremap.bisbootstrapdone == false || KVADDR_TO_PADDR(kaddr) < g_coremap.freeaddr)
		return ;// leak memory for memsteals
	KASSERT(kaddr % PAGE_SIZE == 0);
	paddr_t addr = KVADDR_TO_PADDR(kaddr);
	spinlock_acquire(&spinlkcore);

	int startindex = addr/PAGE_SIZE;
	KASSERT(g_coremap.physicalpages[startindex].numallocations != 0);
	KASSERT(startindex + g_coremap.physicalpages[startindex].numallocations < g_coremap.numpages);
	for(uint32_t i=startindex; i< startindex + g_coremap.physicalpages[startindex].numallocations; i++ )
	{
		KASSERT(g_coremap.physicalpages[i].state != PAGE_FREE);
		KASSERT(g_coremap.physicalpages[i].state == PAGE_FIXED);
		g_coremap.physicalpages[i].state = PAGE_FREE;
		g_coremap.physicalpages[i].as = NULL;
		//memset((void *)PADDR_TO_KVADDR(g_coremap.physicalpages[i].pa), 0, PAGE_SIZE );
	}
	spinlock_release(&spinlkcore);
}

void copy_page(index_t dst, index_t src)
{
	spinlock_acquire(&spinlkcore);
	memcpy( (char*)PADDR_TO_KVADDR(PAGE_SIZE * dst), (char*)PADDR_TO_KVADDR(src * PAGE_SIZE), PAGE_SIZE);
	spinlock_release(&spinlkcore);
}

void
vm_tlbshootdown_all(void)
{
	int spl = splhigh();

	for (int i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	//as->tlbclock = 0;
	splx(spl);
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	int spl = splhigh();
	uint32_t ehi, elo;
	//int32_t uberindex = VADDR_TO_UBERINDEX(ts->ts_vaddr);
	//int32_t subindex = VADDR_TO_SUBINDEX(ts->ts_vaddr);
	for (int i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if(ehi == ts->ts_vaddr)
			tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	//as->tlbclock = 0;
	splx(spl);
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;
	int uberIndex=VADDR_TO_UBERINDEX(faultaddress);
	int subIndex=VADDR_TO_SUBINDEX(faultaddress);
	as = curthread->t_addrspace;
	if (as == NULL) {

		// * No address space set up. This is probably a kernel
		// * fault early in boot. Return EFAULT so as to panic
		// * instead of getting into an infinite faulting loop.
		return EFAULT;
	}
	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);


	if( (as->uberArray[uberIndex]== NULL || as->uberArray[uberIndex][subIndex] == NULL) && uberIndex < STACK_MAX && !(faultaddress >= as->as_heapbase && faultaddress <= as->as_heapend))
		return EFAULT;
	if(uberIndex < STACK_MAX && !(faultaddress >= as->as_heapbase && faultaddress <= as->as_heapend))
	{
		switch (faulttype) {
		case VM_FAULT_READONLY:
			if( (as->uberArray[uberIndex][subIndex]->permission & 0x2) == 0)
				return EFAULT;
			break;
		case VM_FAULT_READ:
			if((as->uberArray[uberIndex][subIndex]->permission & 0x4) == 0)
				return EFAULT;
			break;
		case VM_FAULT_WRITE:
			if((as->uberArray[uberIndex][subIndex]->permission & 0x2) == 0)
				return EFAULT;
			break;
		default:
			return EINVAL;
		}
	}



	//if control comes here it means its not a segmentation fault
	//Address translation-> find paddr

	if(uberIndex >= STACK_MAX)
	{
		//if last 1024 pages just assume its stack.... for now?
		if(as->uberArray[uberIndex]==NULL)
		{
			as_init_uberarray_section(as, uberIndex);
		}
		if(as->uberArray[uberIndex][subIndex] == NULL)
		{
			as->uberArray[uberIndex][subIndex] = as_init_virtualpage();
			as->uberArray[uberIndex][subIndex]->permission = 0x6;
			index_t index;
			int err= allocate_userpage(as, uberIndex, subIndex, &index);
			if(err)
				return err;
			as->uberArray[uberIndex][subIndex]->coremapindex = index;
			as->uberArray[uberIndex][subIndex]->status |= VPAGE_INMEMORY;
		}
		if(as->as_sttop < faultaddress)
			as->as_sttop = faultaddress;
	}
	else if(faultaddress >= as->as_heapbase && faultaddress <= as->as_heapend)
	{
		if(as->uberArray[uberIndex]==NULL)
		{
			as_init_uberarray_section(as, uberIndex);
		}
		if(as->uberArray[uberIndex][subIndex] == NULL)
		{
			as->uberArray[uberIndex][subIndex] = as_init_virtualpage();
			as->uberArray[uberIndex][subIndex]->permission = 0x6;
			index_t index;
			int err= allocate_userpage(as, uberIndex, subIndex, &index);
			if(err)
				return err;
			as->uberArray[uberIndex][subIndex]->coremapindex = index;
			as->uberArray[uberIndex][subIndex]->status |= VPAGE_INMEMORY;
		}
	}
	else if((as->uberArray[uberIndex][subIndex]->status & VPAGE_INMEMORY)==0 && (as->uberArray[uberIndex][subIndex]->status & VPAGE_INSWAP)==0)
	{
		KASSERT(as->uberArray[uberIndex] !=NULL );
		KASSERT(as->uberArray[uberIndex][subIndex] != NULL);
		index_t index;
		int err= allocate_userpage(as, uberIndex, subIndex, &index);
		if(err)
			return err;
		as->uberArray[uberIndex][subIndex]->coremapindex = index;
		as->uberArray[uberIndex][subIndex]->status |= VPAGE_INMEMORY;
	}
	if( (as->uberArray[uberIndex][subIndex]->status & VPAGE_INMEMORY) == 0 && (as->uberArray[uberIndex][subIndex]->status & VPAGE_INSWAP) != 0)	//page needs to be swapped in
	{
		int err = swapin(as, uberIndex, subIndex);
		if(err)
			return err;
		as->uberArray[uberIndex][subIndex]->status |= VPAGE_INMEMORY;
	}

	//we will check if the virtual page is in coremap or not....right now we are assuming it is in coremap.

	KASSERT((as->uberArray[uberIndex][subIndex]->status & VPAGE_INMEMORY) != 0);
	paddr= PAGE_SIZE * as->uberArray[uberIndex][subIndex]->coremapindex;


	KASSERT(g_coremap.physicalpages[as->uberArray[uberIndex][subIndex]->coremapindex].state != PAGE_FREE);
	//if(g_coremap.physicalpages[as->uberArray[uberIndex][subIndex]->coremapindex].as != as && g_coremap.physicalpages[as->uberArray[uberIndex][subIndex]->coremapindex].as != NULL)
	KASSERT(g_coremap.physicalpages[as->uberArray[uberIndex][subIndex]->coremapindex].as == as || g_coremap.physicalpages[as->uberArray[uberIndex][subIndex]->coremapindex].as == NULL);
	//KASSERT(g_coremap.physicalpages[as->uberArray[uberIndex][subIndex]->coremapindex]);
	//KASSERT((paddr & PAGE_FRAME) == paddr);

	// Disable interrupts on this CPU while frobbing the TLB.
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

	as->tlbclock = (as->tlbclock + 1) % NUM_TLB;
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_write(ehi, elo, as->tlbclock);
	splx(spl);
	return 0;
	//kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	//splx(spl);
	//return EFAULT;


}
void dumpcoremap(void)
{
	for(uint32_t i=0; i<g_coremap.numpages; i++)
	{
		kprintf("%d", g_coremap.physicalpages[i].state);

	}
	kprintf("\n");
}

int chooseframetoevict(index_t *retval)
{
	for(index_t i = (g_coremap.swapcounter + 1)%g_coremap.numpages ; i!= g_coremap.swapcounter; i=(i+1)%g_coremap.numpages)
	{
		if(g_coremap.physicalpages[i].state == PAGE_CLEAN || g_coremap.physicalpages[i].state == PAGE_DIRTY)
		{
			g_coremap.swapcounter = (i + 5) % g_coremap.numpages;
			//			struct tlbshootdown ts;
			*retval = i;
			return 0;
		}
	}
	panic("Out of memory"); //no free page;
	return ENOMEM;
}

index_t findfreeswapoffset()
{
	off_t retval = g_swapper.fileend;
	g_swapper.fileend += PAGE_SIZE;
	return (retval / PAGE_SIZE);
}

void evict(index_t coremapindex)
{
	KASSERT(g_coremap.physicalpages[coremapindex].state == PAGE_CLEAN);
	struct addrspace *as = g_coremap.physicalpages[coremapindex].as;
	int32_t uberindex = VADDR_TO_UBERINDEX(g_coremap.physicalpages[coremapindex].vpage);
	int32_t subindex = VADDR_TO_SUBINDEX(g_coremap.physicalpages[coremapindex].vpage);
	as->uberArray[uberindex][subindex]->status &= 0xFFFE;	//set the first bit to 0;
	g_coremap.physicalpages[coremapindex].state = PAGE_FREE;
	g_coremap.physicalpages[coremapindex].as = NULL;
	g_coremap.physicalpages[coremapindex].numallocations = 0;

}

int writetoswap(index_t coremapindex, index_t *swapoffset)
{
	if(g_swapper.swapfile == NULL)
	{
		int err = vfs_open((char*)"os161.swap",O_RDWR|O_CREAT|O_TRUNC, 0, &g_swapper.swapfile);
		if(err)
			return err;
	}
	//struct addrspace *as = g_coremap.physicalpages[coremapindex].as;
	//KASSERT(*swapoffset != -1);
	struct iovec iov;
	struct uio ku;
	char *writebuf = (char*) ( PADDR_TO_KVADDR((coremapindex * PAGE_SIZE)));
	uio_kinit(&iov, &ku, writebuf, PAGE_SIZE, ( (*swapoffset) * PAGE_SIZE), UIO_WRITE);
	int err = vfs_write(g_swapper.swapfile, &ku);
	if(err)
	{
		return err;
	}
	g_coremap.physicalpages[coremapindex].state = PAGE_CLEAN;
	return 0;
}

int readfromswap(index_t coremapindex, index_t swapoffset)
{
	KASSERT(g_swapper.swapfile != NULL);
	//KASSERT((swapoffset != -1));

	struct iovec iov;
	struct uio ku;
	char *readbuf = (char*) ( PADDR_TO_KVADDR( (coremapindex * PAGE_SIZE)));
	uio_kinit(&iov, &ku, readbuf, PAGE_SIZE, (swapoffset * PAGE_SIZE), UIO_READ);

	int err = vfs_read(g_swapper.swapfile, &ku);
	if(err)
	{
		return err;
	}
	g_coremap.physicalpages[coremapindex].state = PAGE_DIRTY;
	g_coremap.physicalpages[coremapindex].numallocations = 1;
	return 0;
}

int swapout(index_t *coremapindex, bool allocate)
{
	lock_acquire(g_swapper.lk_swapper);
	int err=0;
	if(allocate == true)	//if caller did not mention then we pick one
		err = chooseframetoevict(coremapindex);
	if(err)
		return ENOMEM;

	struct addrspace *victim_as = g_coremap.physicalpages[*coremapindex].as;
	int32_t uberindex = VADDR_TO_UBERINDEX(g_coremap.physicalpages[*coremapindex].vpage);
	int32_t subindex = VADDR_TO_SUBINDEX(g_coremap.physicalpages[*coremapindex].vpage);
	struct tlbshootdown ts;

	ts.ts_addrspace = g_coremap.physicalpages[*coremapindex].as;
	ts.ts_vaddr = g_coremap.physicalpages[*coremapindex].vpage;

	victim_as->uberArray[uberindex][subindex]->status &= 0xFFFE;	//set the first bit to 0;
	vm_tlbshootdown(&ts);
	ipi_tlbshootdown_broadcast(&ts);
	if(g_coremap.physicalpages[*coremapindex].state == PAGE_DIRTY)
	{
		if((victim_as->uberArray[uberindex][subindex]->status & VPAGE_INSWAP) == 0)	//this is the first time this frame is being written to file, so allocate a place in file
		{
			victim_as->uberArray[uberindex][subindex]->swapfileoffset = findfreeswapoffset();	//repeated twice keep in one place
		}
		writetoswap(*coremapindex, &(victim_as->uberArray[uberindex][subindex]->swapfileoffset));
		victim_as->uberArray[uberindex][subindex]->status |= VPAGE_INSWAP;
	}
	KASSERT(g_coremap.physicalpages[*coremapindex].state == PAGE_CLEAN && (victim_as->uberArray[uberindex][subindex]->status & VPAGE_INSWAP) != 0);

	evict(*coremapindex);

	lock_release(g_swapper.lk_swapper);
	return 0;
}

int swapin(struct addrspace *as, int uberindex, int subindex)
{

	//first try to find a free frame to swap in to.
	index_t freemem = 0;
	int err;
	err=allocate_userpage(as, uberindex, subindex, &freemem);
	lock_acquire(g_swapper.lk_swapper);
	if(err)
		return err;

	//now we have a  free memory frame read from file
	err = readfromswap(freemem, as->uberArray[uberindex][subindex]->swapfileoffset);
	spinlock_acquire(&spinlkcore);
	g_coremap.physicalpages[freemem].as = as;
	g_coremap.physicalpages[freemem].numallocations  = 1;
	g_coremap.physicalpages[freemem].state = PAGE_DIRTY;
	g_coremap.physicalpages[freemem].vpage = INDECES_TO_VADDR(uberindex,subindex);
	as->uberArray[uberindex][subindex]->coremapindex = freemem;
	as->uberArray[uberindex][subindex]->status &= VPAGE_INMEMORY;
	spinlock_release(&spinlkcore);
	lock_release(g_swapper.lk_swapper);
	if(err)
		return err;

	return 0;
}


