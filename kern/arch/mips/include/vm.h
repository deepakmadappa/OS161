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

#ifndef _MIPS_VM_H_
#define _MIPS_VM_H_
#include <lib.h>
typedef uint8_t page_state_t;

#define PAGE_FREE 0
#define PAGE_CLEAN 1
#define PAGE_DIRTY 2
#define PAGE_FIXED 3

#define VPAGE_UNINIT 0
#define VPAGE_INMEMORY 1
#define VPAGE_INSWAP 2
#define VPAGE_BUSY 4
/*
 * Machine-dependent VM system definitions.
 */

#define PAGE_SIZE  4096         /* size of VM page */
#define PAGE_FRAME 0xfffff000   /* mask for getting page number from addr */

/*
 * MIPS-I hardwired memory layout:
 *    0xc0000000 - 0xffffffff   kseg2 (kernel, tlb-mapped)
 *    0xa0000000 - 0xbfffffff   kseg1 (kernel, unmapped, uncached)
 *    0x80000000 - 0x9fffffff   kseg0 (kernel, unmapped, cached)
 *    0x00000000 - 0x7fffffff   kuseg (user, tlb-mapped)
 *
 * (mips32 is a little different)
 */

#define MIPS_KUSEG  0x00000000
#define MIPS_KSEG0  0x80000000
#define MIPS_KSEG1  0xa0000000
#define MIPS_KSEG2  0xc0000000

/* 
 * The first 512 megs of physical space can be addressed in both kseg0 and
 * kseg1. We use kseg0 for the kernel. This macro returns the kernel virtual
 * address of a given physical address within that range. (We assume we're
 * not using systems with more physical space than that anyway.)
 *
 * N.B. If you, say, call a function that returns a paddr or 0 on error,
 * check the paddr for being 0 *before* you use this macro. While paddr 0
 * is not legal for memory allocation or memory management (it holds 
 * exception handler code) when converted to a vaddr it's *not* NULL, *is*
 * a valid address, and will make a *huge* mess if you scribble on it.
 */
#define PADDR_TO_KVADDR(paddr) ((paddr)+MIPS_KSEG0)

#define KVADDR_TO_PADDR(vaddr) ((vaddr)-MIPS_KSEG0)

#define ROUNDDOWN(X,Y) ( (X/Y) * Y)

#define VADDR_TO_UBERINDEX(X) ( (int)(X>>22))

#define VADDR_TO_SUBINDEX(X) ( (int)((X & 0x003FF000) >> 12) )

#define BYTES_TO_PAGES(bytes) ( (bytes/PAGE_SIZE) + ((bytes%PAGE_SIZE ==0)?0:1 ) )

#define INDECES_TO_VADDR(uber, sub)	( uber << 22 | sub <<12)

#define NUM_UBERPAGES 512
#define NUM_SUBPAGES 1024
#define STACK_MAX 511
/*
 * The top of user space. (Actually, the address immediately above the
 * last valid user address.)
 */
#define USERSPACETOP  MIPS_KSEG0

/*
 * The starting value for the stack pointer at user level.  Because
 * the stack is subtract-then-store, this can start as the next
 * address after the stack area.
 *
 * We put the stack at the very top of user virtual memory because it
 * grows downwards.
 */
#define USERSTACK     USERSPACETOP

/*
 * Interface to the low-level module that looks after the amount of
 * physical memory we have.
 *
 * ram_getsize returns the lowest valid physical address, and one past
 * the highest valid physical address. (Both are page-aligned.) This
 * is the memory that is available for use during operation, and
 * excludes the memory the kernel is loaded into and memory that is
 * grabbed in the very early stages of bootup.
 *
 * ram_stealmem can be used before ram_getsize is called to allocate
 * memory that cannot be freed later. This is intended for use early
 * in bootup before VM initialization is complete.
 */

void ram_bootstrap(void);
paddr_t ram_stealmem(unsigned long npages);
void ram_getsize(paddr_t *lo, paddr_t *hi);

/*
 * TLB shootdown bits.
 *
 * We'll take up to 16 invalidations before just flushing the whole TLB.
 */

typedef uint16_t index_t;

struct tlbshootdown {
	/*
	 * Change this to what you need for your VM design.
	 */
	struct addrspace *ts_addrspace;
	vaddr_t ts_vaddr;
};


#define TLBSHOOTDOWN_MAX 16

paddr_t allocate_onepage(void);
paddr_t allocate_multiplepages(int npages);
int allocate_userpage(struct addrspace*, int, int, index_t*);
void free_userpage(index_t index);
void copy_page(index_t dst, index_t src);
void* memset(void *ptr, int ch, size_t len);

int swapin(struct addrspace *as, int uberindex, int subindex);
int swapout(index_t *coremapindex, bool allocate, struct addrspace *as);
int readfromswap(index_t coremapindex, index_t swapoffset);
int writetoswap(index_t coremapindex, index_t *swapoffset);
void evict(index_t coremapindex);
index_t findfreeswapoffset(void);
int chooseframetoevict(index_t*);

struct virtualpage
{
	index_t coremapindex;	//this traslates to physical address coremapindex * PAGE_SIZE
	index_t swapfileoffset;
	uint8_t permission:3;
	uint8_t status:3;
};

struct memorypage
{

    /* page state */
    page_state_t state;


    uint32_t numallocations;
    //uint64_t timestamp;
    struct addrspace *as;
    vaddr_t vpage;
    //add more stuff here
};

//static struct spinlock spinlkcore;
struct struct_coremap
{
	struct memorypage *physicalpages;
	paddr_t freeaddr;
	paddr_t firstaddr;
	paddr_t lastaddr;
	index_t numpages;
	bool bisbootstrapdone;
    int swapcounter;

}g_coremap;

struct struct_swapper
{
        struct vnode *swapfile;
        off_t fileend;
        struct lock *lk_swapper;
}g_swapper;

void dumpcoremap(void);

//static paddr_t getppages(unsigned long npages);



#endif /* _MIPS_VM_H_ */
