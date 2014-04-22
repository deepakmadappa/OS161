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
#include <addrspace.h>
#include <vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	for(int i=0;i<NUM_UBERPAGES;i++){
		as->uberArray[i] = NULL;
	}

	as->as_heapbase = 0;
	as->as_heapend = 0;
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	newas = kmalloc(sizeof(struct addrspace));
	for(int i=0; i<NUM_UBERPAGES; i++)
	{
		if(old->uberArray[i] != NULL)
		{
			as_init_uberarray_section(newas, i);
			for(int j=0; j<NUM_SUBPAGES; j++)
			{
				if(old->uberArray[i][j] != NULL)
				{
					//allocate user page

					newas->uberArray[i][j] = kmalloc(sizeof(struct virtualpage));
					newas->uberArray[i][j]->swapfileoffset = old->uberArray[i][j]->swapfileoffset;
					newas->uberArray[i][j]->permission = old->uberArray[i][j]->permission;
					if(old->uberArray[i][j]->coremapindex != -1)
					{
						int32_t address = allocate_userpage();
						if(address == -1)
							KASSERT(!"User page allocation must not fail");
						newas->uberArray[i][j]->coremapindex = address;
						memcpy( (char*)PADDR_TO_KVADDR(g_coremap.physicalpages[address].pa), (char*)PADDR_TO_KVADDR(g_coremap.physicalpages[old->uberArray[i][j]->coremapindex].pa), PAGE_SIZE);
					}

				}
			}
		}
	}

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */

	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */
	int uberindex = VADDR_TO_UBERINDEX(vaddr);
	int subindex = VADDR_TO_SUBINDEX(vaddr);
	int permission = (readable << 2) & (writeable <<1) & (executable);

	/*if(as->uberArray[uberindex] == NULL)
	{
		as_init_uberarray_section(as, uberindex);
		int subindex = VADDR_TO_SUBINDEX(vaddr);
		int pages = BYTES_TO_PAGES(sz);
		struct virtualpage * vpages = kmalloc(pages * sizeof(struct virtualpage));
		for(int i=0; i< pages; i++)
		{
			as->uberArray[uberindex][subindex+i] = &(vpages[i]);
			vpages[i].coremapindex = -1;
			vpages[i].swapfileoffset = -1;
			vpages[i].permission = (int8_t)permission;
		}
	}
	else	//uberindex exists
	{
		int subindex = VADDR_TO_SUBINDEX(vaddr);
		int pages = BYTES_TO_PAGES(sz);
		for(int i=subindex; i< subindex + pages; i++)
		{
			if (as->uberArray[uberindex][i] != NULL)
				return EFAULT;
		}
		struct virtualpage * vpages = kmalloc(pages * sizeof(struct virtualpage));
		for(int i=0; i< pages; i++)
		{
			as->uberArray[uberindex][subindex+i] = &(vpages[i]);
			vpages[i].coremapindex = -1;
			vpages[i].swapfileoffset = -1;
			vpages[i].permission = (int8_t)permission;
		}
	}*/

	uint32_t numpages = (sz + (vaddr & 0xFFF)) / PAGE_SIZE;	// vaddr & 0xFFF is necessary because loadelf may define region from middle of page so if we compute just with size we may not allocate enough
	//struct virtualpage * vpages = kmalloc(numpages * sizeof(struct virtualpage));
	for(uint32_t i=0; i < numpages; i ++)
	{
		if(as->uberArray[uberindex] == NULL)
		{
			as_init_uberarray_section(as, uberindex);
		}

		if(as->uberArray[uberindex][subindex] !=NULL)
		{
			return EFAULT;	//address already in use
		}
		as->uberArray[uberindex][subindex] = kmalloc(sizeof(struct virtualpage));
		as->uberArray[uberindex][subindex]->coremapindex = -1;
		as->uberArray[uberindex][subindex]->swapfileoffset = -1;
		as->uberArray[uberindex][subindex]->permission = (int8_t)permission;

		subindex++;
		if(subindex >= 1024)
		{
			subindex = 0;
			uberindex++;
		}
	}

	if(vaddr + sz > as->as_heapbase)
	{
		as->as_heapbase = INDECES_TO_VADDR(uberindex, subindex);
		as->as_heapend = as->as_heapbase;
	}
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	for(int i=0; i<1024; i++)
	{
		if(as->uberArray[i] != NULL)
		{
			for(int j=0; j<1024; j++)
			{
				if(as->uberArray[i][j] != NULL)
				{
					//allocate user page
					int32_t address = allocate_userpage();
					if(address == -1)
						KASSERT(!"User page allocation must not fail");
					as->uberArray[i][j]->coremapindex = address;
				}
			}
		}
	}
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;


	return 0;
}

void as_init_uberarray_section(struct addrspace *as, int index)
{
	as->uberArray[index] =  (struct virtualpage **)kmalloc(1024 * sizeof(struct virtualpage*));
	for(int i=0;i < 1024; i++)
	{
		as->uberArray[index][i] = NULL;
	}
}
