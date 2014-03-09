#include <syscall.h>
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


struct trapframe* clone(struct trapframe *inframe)
{
	struct trapframe* returnframe = kmalloc(sizeof(struct trapframe));

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

	return returnframe;
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

pid_t sys_fork(struct trapframe *ptf)
{
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

int sys_execv(const char *program, char **args)
{
	(void)program, (void)args;
	return 0;
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
pid_t sys_waitpid(pid_t pid, int *status, int options)
{
	(void)pid,(void)status,(void)options;

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

void sys__exit(int exitcode)
{
	(void)exitcode;


}
