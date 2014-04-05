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

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <copyinout.h>

int kstrcpy(char* src, char* dest)
{
	int i=0;
	while(src[i] != '\0')
	{
		dest[i] = src[i];
		i++;
	}
	dest[i++]= '\0';
	return i;
}

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname,char** args, unsigned long nargs)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	//Copy the arguments into kernel buffer
	unsigned long j=0;
	size_t strlen=0;
	int size=0;
	unsigned long argc;
	//Calculate the size of the array to allocate
	char * tempArgs=kmalloc(1000);

	while(j< nargs){
		strlen = kstrcpy(args[j], tempArgs);//copyinstr((userptr_t)(args[j]), tempArgs, 1000,&strlen);
		strlen=strlen + 4 - strlen%4;
		size+=strlen;
		j++;
	}
	argc=j;
	//Add space for 4 integers
	size+=argc*4;
	size+=4;//for Null

	char * kargv= kmalloc(size);
	int top=argc*4+4;
	j=0;
	while(j< nargs){
		strlen=0;
		*((int*)(kargv + j*4))=top;
		strlen = kstrcpy(args[j], kargv+top);//copyinstr((userptr_t)args[j], kargv+top, 1000,&strlen);
		top+=strlen;
		while(top%4!=0){
			*(kargv+top)='\0';
			top++;
		}
		j++;
	}
	int * ka = (int*)(kargv + j*4);
	*(ka)=0;

	//kargv is constructed




	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_addrspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}



	//Initializing STD IN
	struct vnode *std;
	char con[5] = "con:";
	int err = vfs_open(con, O_RDONLY, 0x660, &std);
	if(err)
		KASSERT("Initializing STDIN failed");
	struct filehandle *fh = kmalloc(sizeof(struct filehandle));
	if(fh==NULL)
		KASSERT("Memory allocation for file handle failed");
	fh->fileobject = std;
	fh->offset = 0;
	fh->open_mode = O_RDONLY;
	fh->lk_fileaccess = lock_create("filelock");
	fh->refcount = 0;
	fh->isSeekable = 0;

	// *fd = addtofiletable(fh);	we'll set fd once we implement filetable;
	curthread->filetable[0] = fh;

	//Initializing STDOUT
	err = vfs_open(con, O_WRONLY, 0x660, &std);
	if(err)
		KASSERT("Initializing STDOUT failed");
	fh = kmalloc(sizeof(struct filehandle));
	if(fh==NULL)
		KASSERT("Memory allocation for file handle failed");
	fh->fileobject = std;
	fh->offset = 0;
	fh->open_mode = O_WRONLY;
	fh->lk_fileaccess = lock_create("filelock");
	fh->refcount = 0;
	fh->isSeekable = 0;

	// *fd = addtofiletable(fh);	we'll set fd once we implement filetable;
	curthread->filetable[1] = fh;

	//Initializing STDERR
	err = vfs_open(con, O_WRONLY, 0660, &std);
	if(err)
		KASSERT("Initializing STDERR failed");
	fh = kmalloc(sizeof(struct filehandle));
	if(fh==NULL)
		panic("Memory allocation for file handle failed");
	fh->fileobject = std;
	fh->offset = 0;
	fh->open_mode = O_WRONLY;
	fh->lk_fileaccess = lock_create("filelock");
	fh->refcount = 0;
	fh->isSeekable = 0;

	// *fd = addtofiletable(fh);	we'll set fd once we implement filetable;
	curthread->filetable[2] = fh;

	//initialize filetable to NULL except for STDIO
	int i;
	for(i=3; i < OPEN_MAX;i++)
		curthread->filetable[i] = NULL;



	//	lock_acquire(&g_lk_pid);
	for(i=3; i<PID_MAX; i++)
	{
		g_pidlist[i]= NULL;
	}
	//	lock_release(&g_lk_pid);

	//let us assume this is the init process/thread set the pid to 1
	curthread->pid = PID_MIN;
	curthread->ppid = 0;
	struct pidentry* pident = kmalloc(sizeof(struct pidentry));
	pident->exitstatus = 0;
	pident->thread = curthread;
	pident->sem = sem_create("threadsem", 0);
	g_pidlist[PID_MIN] = pident;

	V(g_runprogsem);




	//Copy the arguments to userstack
	j=0;
	int * userAddr=(int *)(stackptr-size);

	while(j<argc){
		ka = (int*)(kargv + j*4);
		*ka=*ka+(int)userAddr;
		//userAddr+=2;
		j++;
	}
	copyout(kargv, (userptr_t)userAddr, (size_t)size);
	//Copied to user space

	char* copyinstack = kmalloc(size);
	copyin((userptr_t)userAddr,(void *)copyinstack,(size_t)size);

	int k=0;
	for(k=0;k<size;k++)
	{
		kprintf("%c", copyinstack[i]);
	}
	//while(1);
	/* Warp to user mode. */
	enter_new_process(argc, (userptr_t)(userAddr),(vaddr_t)(userAddr), entrypoint);

	/* Warp to user mode. */
	//	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
	//			stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
