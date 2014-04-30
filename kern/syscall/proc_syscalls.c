#include <types.h>
#include <syscall.h>
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
#include <mips/trapframe.h>
#include <synch.h>
#include <copyinout.h>
#include <kern/wait.h>
void clonetrapframe(struct trapframe *inframe, struct trapframe *returnframe)
{
	//struct trapframe* returnframe = kmalloc(sizeof(struct trapframe));

	returnframe->tf_vaddr = inframe->tf_vaddr;	/* coprocessor 0 vaddr register */
	returnframe->tf_status = inframe->tf_status;	/* coprocessor 0 status register */
	returnframe->tf_cause = inframe->tf_cause;	/* coprocessor 0 cause register */
	returnframe->tf_lo = inframe->tf_lo;
	returnframe->tf_hi = inframe->tf_hi;
	returnframe->tf_ra = inframe->tf_ra;		/* Saved register 31 */
	returnframe->tf_at = inframe->tf_at;		/* Saved register 1 (AT) */
	returnframe->tf_v0 = inframe->tf_v0;		/* Saved register 2 (v0) */
	returnframe->tf_v1 = inframe->tf_v1;		/* etc. */
	returnframe->tf_a0 = inframe->tf_a0;
	returnframe->tf_a1 = inframe->tf_a1;
	returnframe->tf_a2 = inframe->tf_a2;
	returnframe->tf_a3 = inframe->tf_a3;
	returnframe->tf_t0 = inframe->tf_t0;
	returnframe->tf_t1 = inframe->tf_t1;
	returnframe->tf_t2 = inframe->tf_t2;
	returnframe->tf_t3 = inframe->tf_t3;
	returnframe->tf_t4 = inframe->tf_t4;
	returnframe->tf_t5 = inframe->tf_t5;
	returnframe->tf_t6 = inframe->tf_t6;
	returnframe->tf_t7 = inframe->tf_t7;
	returnframe->tf_s0 = inframe->tf_s0;
	returnframe->tf_s1 = inframe->tf_s1;
	returnframe->tf_s2 = inframe->tf_s2;
	returnframe->tf_s3 = inframe->tf_s3;
	returnframe->tf_s4 = inframe->tf_s4;
	returnframe->tf_s5 = inframe->tf_s5;
	returnframe->tf_s6 = inframe->tf_s6;
	returnframe->tf_s7 = inframe->tf_s7;
	returnframe->tf_t8 = inframe->tf_t8;
	returnframe->tf_t9 = inframe->tf_t9;
	returnframe->tf_k0 = inframe->tf_k0;		/* dummy (see exception.S comments) */
	returnframe->tf_k1 = inframe->tf_k1;		/* dummy */
	returnframe->tf_gp = inframe->tf_gp;
	returnframe->tf_sp = inframe->tf_sp;
	returnframe->tf_s8 = inframe->tf_s8;
	returnframe->tf_epc = inframe->tf_epc;

	//return returnframe;
}

/*
 *
 * Name
getpid - get process id
Library
Standard C Library (libc, -lc)
Synopsis
#include <unistd.h>

pid_t
getpid(void);
Description
getpid returns the process id of the current process.
Errors
getpid does not fail.
 */

pid_t sys_getpid(void)
{
	return curthread->pid;
}


/*
 *
 * Name
fork - copy the current process
Library
Standard C Library (libc, -lc)
Synopsis
#include <unistd.h>

pid_t
fork(void);
Description
fork duplicates the currently running process. The two copies are identical, except that one (the "new" one, or "child"), has a new, unique process id, and in the other (the "parent") the process id is unchanged.

The process id must be greater than 0.

The two processes do not share memory or open file tables; this state is copied into the new process, and subsequent modification in one process does not affect the other.

However, the file handle objects the file tables point to are shared, so, for instance, calls to lseek in one process can affect the other.

Return Values
On success, fork returns twice, once in the parent process and once in the child process. In the child process, 0 is returned. In the parent process, the process id of the new child process is returned.

On error, no new process is created, fork only returns once, returning -1, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    EMPROC		The current user already has too many processes.
    ENPROC		There are already too many processes on the system.
    ENOMEM		Sufficient virtual memory for the new process was not available.
 */

struct message
{
	struct trapframe *tf;
	struct addrspace *as;
	struct semaphore *sem;
	int *pid;
};

int sys_fork(struct trapframe *ptf, pid_t *pid)
{
	//clone the parent trapframe

	//TODO: Currently leaking this part of memory need to fix this
	//struct trapframe *tf = kmalloc(sizeof(struct trapframe));
	//clonetrapframe(ptf, tf);

	struct addrspace *childas = NULL;
	int err = as_copy(curthread->t_addrspace, &childas);	//copy parent address space
	if(err)
		return err;
	struct message* msg = kmalloc(sizeof(struct message));
	if(msg == NULL)
		return ENOMEM;
	struct semaphore* s = sem_create("forksem",0);
	if(s == NULL)
		return ENOMEM;
	msg->as = childas;
	msg->tf= ptf;
	msg->sem = s;
	msg->pid = kmalloc(sizeof(int));
	if(msg->pid == NULL)
		return ENOMEM;
	struct thread* child=NULL;
	err = thread_fork("child", &child_fork, (void*)msg, 0, &child );
	if(err)
		return err;
	P(s);
	*pid = *(msg->pid);
	kfree(msg->sem);
	kfree(msg->pid);
	kfree(msg);

	if(err)
		return err;
	return 0;
}

/*
 * Name
execv - execute a program
Library
Standard C Library (libc, -lc)
Synopsis
#include <unistd.h>

int
execv(const char *program, char **args);
Description
execv replaces the currently executing program with a newly loaded program image. This occurs within one process; the process id is unchanged.

The pathname of the program to run is passed as program. The args argument is an array of 0-terminated strings. The array itself should be terminated by a NULL pointer.

The argument strings should be copied into the new process as the new process's argv[] array. In the new process, argv[argc] must be NULL.

By convention, argv[0] in new processes contains the name that was used to invoke the program. This is not necessarily the same as program, and furthermore is only a convention and should not be enforced by the kernel.

The process file table and current working directory are not modified by execve.
Return Values
On success, execv does not return; instead, the new program begins executing. On failure, execv returns -1, and sets errno to a suitable error code for the error condition encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    ENODEV		The device prefix of program did not exist.
    ENOTDIR		A non-final component of program was not a directory.
    ENOENT		program did not exist.
    EISDIR		program is a directory.
    ENOEXEC		program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields.
    ENOMEM		Insufficient virtual memory is available.
    E2BIG		The total size of the argument strings is too large.
    EIO		A hard I/O error occurred.
    EFAULT		One of the args is an invalid pointer.
 */

int sys_execv(userptr_t prog, userptr_t argsptr)
{
	char *program = kmalloc(1000);
	size_t proglen;

	int err = copyin(argsptr, program,1); // doing this just to make sure its valid address
	if(err)
	{
		kfree(program);
		return EFAULT;
	}

	err = copyinstr(prog, program, 1000, &proglen);
	if(err)
	{
		kfree(program);
		return EFAULT;
	}

	char** args=(char**)argsptr;
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


	while(args[j]!=NULL){
		//strlen = kstrcpy(args[j], tempArgs);//
		err = copyinstr((userptr_t)(args[j]), tempArgs, 1000,&strlen);
		if(err)
		{
			kfree(tempArgs);
			return EFAULT;
		}
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
	while(args[j]!=NULL){
		strlen=0;
		*((int*)(kargv + j*4))=top;
		//	strlen = kstrcpy(args[j], kargv+top);//
		copyinstr((userptr_t)args[j], kargv+top, 1000,&strlen);
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
	result = vfs_open((char*)program, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

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
	kfree(tempArgs);
	kfree(kargv);
	/* Warp to user mode. */
	enter_new_process(argc, (userptr_t)(userAddr),(vaddr_t)(userAddr), entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
/*
 * Name
waitpid - wait for a process to exit
Library
Standard C Library (libc, -lc)
Synopsis
#include <sys/wait.h>

pid_t
waitpid(pid_t pid, int *status, int options);
Description
Wait for the process specified by pid to exit, and return an encoded exit status in the integer pointed to by status. If that process has exited already, waitpid returns immediately. If that process does not exist, waitpid fails.

What it means for a process to move from "has exited already" to "does not exist", and when this occurs, is something you must decide.

If process P is "interested" in the exit code of process Q, process P should be able to find out that exit code by calling waitpid, even if Q exits somewhat before the time P calls waitpid. As described under _exit(), precisely what is meant by "interested" is up to you.

You might implement restrictions or requirements on who may wait for which processes, like Unix does. You might also add a system call for one process to express interest in another process's exit code. If you do this, be sure to write a man page for the system call, and discuss the rationale for your choices therein in your design document.

Note that in the absence of restrictions on who may wait for what, it is possible to set up situations that may result in deadlock. Your system must (of course) in some manner protect itself from these situations, either by prohibiting them or by detecting and resolving them.

In order to make the userlevel code that ships with OS/161 work, assume that a parent process is always interested in the exit codes of its child processes generated with fork(), unless it does something special to indicate otherwise.

The options argument should be 0. You are not required to implement any options. (However, your system should check to make sure that options you do not support are not requested.)

If you desire, you may implement the Unix option WNOHANG; this causes waitpid, when called for a process that has not yet exited, to return 0 immediately instead of waiting.

The Unix option WUNTRACED, to ask for reporting of processes that stop as well as exit, is also defined in the header files, but implementing this feature is not required or necessary unless you are implementing job control.

You may also make up your own options if you find them helpful. However, please, document anything you make up.

The encoding of the exit status is comparable to Unix and is defined by the flags found in <kern/wait.h>. (Userlevel code should include <sys/wait.h> to get these definitions.) A process can exit by calling _exit() or it can exit by receiving a fatal signal. In the former case the _MKWAIT_EXIT() macro should be used with the user-supplied exit code to prepare the exit status; in the latter, the _MKWAIT_SIG() macro (or _MKWAIT_CORE() if a core file was generated) should be used with the signal number. The result encoding also allows notification of processes that have stopped; this would be used in connection with job control and with ptrace-based debugging if you were to implement those things.

To read the wait status, use the macros WIFEXITED(), WIFSIGNALED(), and/or WIFSTOPPED() to find out what happened, and then WEXITSTATUS(), WTERMSIG(), or WSTOPSIG() respectively to get the exit code or signal number. If WIFSIGNALED() is true, WCOREDUMP() can be used to check if a core file was generated. This is the same as Unix, although the value encoding is different from the historic Unix format.

Return Values
waitpid returns the process id whose exit status is reported in status. In OS/161, this is always the value of pid.

If you implement WNOHANG, and WNOHANG is given, and the process specified by pid has not yet exited, waitpid returns 0.

(In Unix, but not by default OS/161, you can wait for any of several processes by passing magic values of pid, so this return value can actually be useful.)

On error, -1 is returned, and errno is set to a suitable error code for the error condition encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    EINVAL		The options argument requested invalid or unsupported options.
    ECHILD		The pid argument named a process that the current process was not interested in or that has not yet exited.
    ESRCH		The pid argument named a nonexistent process.
    EFAULT		The status argument was an invalid pointer.
 */
int sys_waitpid(pid_t pid, userptr_t status, int options, int *retval, int iskernspace)
{
	if(options!=0)
		return EINVAL;

	//if waitOnThread==NULL or exitSemaphore[i]==NULL(Thread exited or does not exist)
	//then exitCode[i]==-1 thread does not exist or exit code collected
	//else thread existed, exited and its exit code yet to be collected
	//oderwise thread currently active. Do a P() on exitSemaphone[i].
	//on V(), check exitCode[i], if it is nt -1 then return it and set it to -1
	//else error

	//As described under _exit(), precisely what is meant by "interested" is up to you...Decide on this
	//TO allow only parents to wait on child, check if current threads Pid is PPID of the child thread. But this wont work with already exited thread.
	//Status yet to understand
	int exit = 0;
	if(iskernspace == 0)//copysomevalue to check if its valid
	{
		int err = copyout(&exit, status, sizeof(int));
		if(err)
			return err;
	}


	if(pid<PID_MIN || pid > PID_MAX)
		return ESRCH;
	if(g_pidlist[pid]==NULL){
		//The pid argument named a nonexistent process.
		return ESRCH;
	}
	//Child Thread is still executing. Do a P() on the corresponding Semaphore.P will return immediately for a Zombie thread.
	//We will allow only parent to collect exitcode of child. This defines what "Interested" means and will also ensure that there is no deadlock
	struct thread* waitOnThread=g_pidlist[pid]->thread;

	//TODO: need to add PPID to pidentry struct only parent should be able to collect
	if(waitOnThread!=NULL && curthread->pid!=waitOnThread->ppid)
		return ECHILD;
	P(g_pidlist[pid]->sem);

	exit = _MKWAIT_EXIT(g_pidlist[pid]->exitstatus);
	if(iskernspace == 1)
	{
		*((int*)status) = g_pidlist[pid]->exitstatus;
	}
	else
	{
		int err = copyout(&exit, status, sizeof(int));
		if(err)
			return err;
	}

	sem_destroy(g_pidlist[pid]->sem);
	kfree(g_pidlist[pid]);
	g_pidlist[pid]=NULL;
	*retval=pid;
	return 0;

}

/*
 * Name
_exit - terminate process
Library
Standard C Library (libc, -lc)
Synopsis
#include <unistd.h>

void
_exit(int exitcode);
Description
Cause the current process to exit. The exit code exitcode is reported back to other process(es) via the waitpid() call. The process id of the exiting process should not be reused until all processes interested in collecting the exit code with waitpid have done so. (What "interested" means is intentionally left vague; you should design this.)
Return Values
_exit does not return.
 */

void sys_exit(int exitcode)
{
	int pid=curthread->pid;
	g_pidlist[pid]->exitstatus=exitcode;//_MKWAIT_EXIT(exitcode);
	g_pidlist[pid]->thread = NULL;
	V(g_pidlist[pid]->sem);
	thread_exit();
}


void child_fork(void* data1, unsigned long data2)
{
	struct message *msg = (struct message *) data1;
	struct trapframe* ptf = msg->tf;
	struct addrspace* as = msg->as;
	(void)data2;
	curthread->t_addrspace = as;
	as_activate(curthread->t_addrspace);

	*(msg->pid) = curthread->pid;
	struct trapframe tf;
	clonetrapframe(ptf, &tf);

	tf.tf_a3 = 0;
	tf.tf_v0 = 0;
	tf.tf_epc += 4;

	//kfree(ptf);

	V(msg->sem);


	mips_usermode(&tf);

}

int createpid(struct thread* newthread, pid_t *ret)
{
	pid_t i;
	lock_acquire(g_lk_pid);
	for(i=3; i<PID_MAX; i++)
		if(g_pidlist[i] == NULL)
		{
			struct pidentry *pident = kmalloc(sizeof(struct pidentry));
			if(pident == NULL)
				return ENOMEM;
			pident->exitstatus = 0;
			pident->thread = newthread;
			pident->sem = sem_create("threadsem", 0);
			g_pidlist[i]= pident;
			*ret = i;
			lock_release(g_lk_pid);
			return 0;
		}
	lock_release(g_lk_pid);
	return ENPROC;
}

/*
 *
 * Name
sbrk - set process break (allocate memory)
Library
Standard C Library (libc, -lc)
Synopsis
#include <unistd.h>

void *
sbrk(intptr_t amount);
Description
The "break" is the end address of a process's heap region. The sbrk call adjusts the "break" by the amount amount. It returns the old "break". Thus, to determine the current "break", call sbrk(0).

The heap region is initially empty, so at process startup, the beginning of the heap region is the same as the end and may thus be retrieved using sbrk(0).

In OS/161, the initial "break" must be page-aligned, and sbrk only need support values of amount that result in page-aligned "break" addresses. Other values of amount may be rejected. (This may simplify the implementation. On the other hand, you may choose to support unaligned values anyway, as that may simplify your malloc code.)

Traditionally, the initial "break" is specifically defined to be the end of the BSS (uninitialized data) region, and any amount, page-aligned or not, may legally be used with sbrk.

Ordinarily, user-level code should call malloc for memory allocation. The sbrk interface is intended only to be the back-end interface for malloc. Mixing calls to malloc and sbrk will likely confuse malloc and produces undefined behavior.

While one can lower the "break" by passing negative values of amount, one may not set the end of the heap to an address lower than the beginning of the heap. Attempts to do so must be rejected.

Return Values
On success, sbrk returns the previous value of the "break". On error, ((void *)-1) is returned, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    ENOMEM		Sufficient virtual memory to satisfy the request was not available, or the process has reached the limit of the memory it is allowed to allocate.
    EINVAL		The request would move the "break" below its initial value.

Restrictions
While you can return pages that happen to be at the end of the heap to the system, there is no way to use the sbrk interface to return unused pages in the middle of the heap. If you wish to do this, you will need to design a new or supplemental interface.

*/

int sys_sbrk(intptr_t amt, vaddr_t *retval)
{
	struct addrspace* as = curthread->t_addrspace;
	vaddr_t oldaddr = as->as_heapend;
	vaddr_t newaddr = as->as_heapend + amt;
	if(newaddr < as->as_heapbase)
		return EINVAL;
	if(newaddr > as->as_sttop)
		return ENOMEM;
	//need to check for stack passing
	as->as_heapend = newaddr;
	*retval = oldaddr;
	return 0;
}


