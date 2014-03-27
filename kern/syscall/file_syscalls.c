#include <types.h>
#include <syscall.h>
#include <lib.h>
#include <vfs.h>
#include <kern/errno.h>
#include <thread.h>
#include <kern/seek.h>
#include <current.h>
#include <lib.h>
#include <kern/iovec.h>
#include <uio.h>
#include <kern/stat.h>
#include <vnode.h>
#include <copyinout.h>

int createfd(struct thread* thread)
{
	int i;
	for(i=3; i < (__OPEN_MAX) ; i++)
		if (thread->filetable[i] == NULL )
			return i;
	return -1;	//file table full
}
/*
Description
open opens the file, device, or other kernel object named by the pathname filename. The flags argument specifies how to open the file. The optional mode argument is only meaningful in Unix (or if you choose to implement Unix-style security later on) and can be ignored.

The flags argument should consist of one of

    O_RDONLY		Open for reading only.
    O_WRONLY		Open for writing only.
    O_RDWR		Open for reading and writing.

It may also have any of the following flags OR'd in:

    O_CREAT		Create the file if it doesn't exist.
    O_EXCL		Fail if the file already exists.
    O_TRUNC		Truncate the file to length 0 upon open.
    O_APPEND		Open the file in append mode.

O_EXCL is only meaningful if O_CREAT is also used.

O_APPEND causes all writes to the file to occur at the end of file, no matter what gets written to the file by whoever else. (This functionality may be optional; consult your course's assignments.)

open returns a file handle suitable for passing to read, write, close, etc. This file handle must be greater than or equal to zero. Note that file handles 0 (STDIN_FILENO), 1 (STDOUT_FILENO), and 2 (STDERR_FILENO) are used in special ways and are typically assumed by user-level code to always be open.
Return Values
On success, open returns a nonnegative file handle. On error, -1 is returned, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    ENODEV		The device prefix of filename did not exist.
    ENOTDIR		A non-final component of filename was not a directory.
    ENOENT		A non-final component of filename did not exist.
    ENOENT		The named file does not exist, and O_CREAT was not specified.
    EEXIST		The named file exists, and O_EXCL was specified.
    EISDIR		The named object is a directory, and it was to be opened for writing.
    EMFILE		The process's file table was full, or a process-specific limit on open files was reached.
    ENFILE		The system file table is full, if such a thing exists, or a system-wide limit on open files was reached.
    ENXIO		The named object is a block device with no mounted filesystem.
    ENOSPC		The file was to be created, and the filesystem involved is full.
    EINVAL		flags contained invalid values.
    EIO		A hard I/O error occurred.
    EFAULT		filename was an invalid pointer.
 */
int sys_open(userptr_t filename, int flags, int32_t *fd, ...)
{
//	kprintf("FileName:%s, Flags:%d", (char*)filename, flags);
	char kfilename[__PATH_MAX + __NAME_MAX + 1];


	struct vnode *file_vnode;
	int err;
	size_t len;
	err = copyinstr(filename, kfilename, __PATH_MAX + __NAME_MAX + 1, &len);
	if(err)
		return err;
	err = vfs_open((char*)filename, flags, 0, &file_vnode);
	if(err)
		return err;
	//No error we got a vnode now create a filediscriptor for it.
	//TBD: we may not need to create a file discriptor
	struct thread* cthread = (struct thread*)curthread;
	*fd = createfd(cthread);
	if(*fd < 0)
		return ENFILE;

	//only create this if its not already there.
	struct filehandle *fh = kmalloc(sizeof(struct filehandle));
	if(fh==NULL)
		panic("Memory allocation for file handle failed");
	fh->fileobject = file_vnode;
	fh->offset = 0;
	fh->open_mode = flags;
	fh->lk_fileaccess = lock_create("filelock");
	fh->refcount = 1;

	// *fd = addtofiletable(fh);	we'll set fd once we implement filetable;
	cthread->filetable[*fd] = fh;
	//panic("fail open failed");
	return 0;
}

/*
Description
read reads up to buflen bytes from the file specified by fd, at the location in the file specified by the current seek position of the file, and stores them in the space pointed to by buf. The file must be open for reading.

The current seek position of the file is advanced by the number of bytes read.

Each read (or write) operation is atomic relative to other I/O to the same file.

Return Values
The count of bytes read is returned. This count should be positive. A return value of 0 should be construed as signifying end-of-file. On error, read returns -1 and sets errno to a suitable error code for the error condition encountered.

Note that in some cases, particularly on devices, fewer than buflen (but greater than zero) bytes may be returned. This depends on circumstances and does not necessarily signify end-of-file.

Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    EBADF		fd is not a valid file descriptor, or was not opened for reading.
    EFAULT		Part or all of the address space pointed to by buf is invalid.
    EIO		A hardware I/O error occurred reading the data.
 */
int sys_read(int fd, userptr_t buf, size_t buflen, int32_t *bytesread)
{
	struct filehandle* fh;
	struct thread *cur = (struct thread*)curthread;
	fh = cur->filetable[fd];
	if(fh == NULL || (fh->open_mode & 1)!=0 )
		return EBADF;

	struct iovec iov;
	struct uio ku;
	char *readbuf = (char*)kmalloc(buflen);
	lock_acquire(fh->lk_fileaccess);
	uio_kinit(&iov, &ku, readbuf, buflen, fh->offset, UIO_READ);

	int err = vfs_read(fh->fileobject, &ku);
	if(err)
		return err;
	*bytesread = buflen - ku.uio_resid;
	fh->offset += *bytesread;

	lock_release(fh->lk_fileaccess);
	err = copyout(readbuf, buf, *bytesread);
	if(err)
		return err;

	kfree(readbuf);

	return 0;
}

/*
Description
write writes up to buflen bytes to the file specified by fd, at the location in the file specified by the current seek position of the file, taking the data from the space pointed to by buf. The file must be open for writing.

The current seek position of the file is advanced by the number of bytes written.

Each write (or read) operation is atomic relative to other I/O to the same file.

Return Values
The count of bytes written is returned. This count should be positive. A return value of 0 means that nothing could be written, but that no error occurred; this only occurs at end-of-file on fixed-size objects. On error, write returns -1 and sets errno to a suitable error code for the error condition encountered.

Note that in some cases, particularly on devices, fewer than buflen (but greater than zero) bytes may be written. This depends on circumstances and does not necessarily signify end-of-file. In most cases, one should loop to make sure that all output has actually been written.

Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    EBADF		fd is not a valid file descriptor, or was not opened for writing.
    EFAULT		Part or all of the address space pointed to by buf is invalid.
    ENOSPC		There is no free space remaining on the filesystem containing the file.
    EIO		A hardware I/O error occurred writing the data.
 */
int sys_write(int fd, userptr_t buf, size_t nbytes, int32_t *byteswritten)
{
	struct filehandle* fh;
	struct thread *cur = (struct thread*)curthread;
	fh = cur->filetable[fd];
	if(fh == NULL || (fh->open_mode & 3)==0 )
		return EBADF;
	char *writebuf = (char* )kmalloc(nbytes+1);
	int result = copyin(buf, writebuf, nbytes);
	if(result)
		return result;
	struct iovec iov;
	struct uio ku;
	lock_acquire(fh->lk_fileaccess);
	uio_kinit(&iov, &ku, writebuf, nbytes, fh->offset, UIO_WRITE);
	//ku.uio_space = cur->t_addrspace;
	int err = vfs_write(fh->fileobject, &ku);
	if(err)
		return err;
	*byteswritten = ku.uio_offset - fh->offset;
	fh->offset += *byteswritten;
	lock_release(fh->lk_fileaccess);
	kfree(writebuf);
	return 0;
}

/*
Description
lseek alters the current seek position of the file handle filehandle, seeking to a new position based on pos and whence.

If whence is

    SEEK_SET, the new position is pos.
    SEEK_CUR, the new position is the current position plus pos.
    SEEK_END, the new position is the position of end-of-file plus pos.
    anything else, lseek fails.

Note that pos is a signed quantity.

It is not meaningful to seek on certain objects (such as the console device). All seeks on these objects fail.

Seek positions less than zero are invalid. Seek positions beyond EOF are legal.

Note that each distinct open of a file should have an independent seek pointer.

Return Values
On success, lseek returns the new position. On error, -1 is returned, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    EBADF		fd is not a valid file handle.
    ESPIPE		fd refers to an object which does not support seeking.
    EINVAL		whence is invalid.
    EINVAL		The resulting seek position would be negative.
 */
int sys_lseek(int fd, off_t pos, int sp, int32_t *offsethigh, int32_t *offsetlow)
{
	if(fd<3)
		return ESPIPE;
	int whence;
	int err = copyin((userptr_t)sp+16, &whence, sizeof(int32_t));

	if(whence <0 || whence >2)
		return EINVAL;
	struct filehandle* fh;
	struct thread *cur = (struct thread*)curthread;
	fh = cur->filetable[fd];
	if(fh == NULL )
		return EBADF;
	lock_acquire(fh->lk_fileaccess);
	off_t newpos;
	if(whence == SEEK_SET)
		newpos = 0;
	else if(whence == SEEK_END)
	{
		struct stat filestat;
		VOP_STAT(fh->fileobject, &filestat);
		newpos = filestat.st_size;
	}
	else
	{
		newpos = fh->offset;
	}
	err =vfs_lseek(fh->fileobject, newpos + pos);
	if(err)
		return err;
	fh->offset = newpos + pos;
	// else whence is SEEK_CUR

	off_t ofst = fh->offset;
	*offsetlow = (int32_t)ofst;
	ofst = ofst >> 32;
	*offsethigh = (int32_t)ofst;

	lock_release(fh->lk_fileaccess);
	return 0;
}

/*
Description
The file handle fd is closed. The same file handle may then be returned again from open, dup2, pipe, or similar calls.

Other file handles are not affected in any way, even if they are attached to the same file.

Return Values
On success, close returns 0. On error, -1 is returned, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    EBADF		fd is not a valid file handle.
    EIO		A hard I/O error occurred.
 */
int sys_close(int fd)
{
	struct filehandle* fh;
	struct thread *cur = (struct thread*)curthread;
	fh = cur->filetable[fd];
	if(fh == NULL)
		return EBADF;
	fh->refcount --;
	if(fh->refcount == 0)
	{
		vfs_close(fh->fileobject);
		kfree(fh);
	}
	cur->filetable[fd] = NULL;
//	while(1);
	return 0;
}

/*
 Description
dup2 clones the file handle oldfd onto the file handle newfd. If newfd names an open file, that file is closed.

The two handles refer to the same "open" of the file - that is, they are references to the same object and share the same seek pointer. Note that this is different from opening the same file twice.

dup2 is most commonly used to relocate opened files onto STDIN_FILENO, STDOUT_FILENO, and/or STDERR_FILENO.

Both filehandles must be non-negative.

Using dup2 to clone a file handle onto itself has no effect.

(The "2" in "dup2" arises from the existence of an older and less powerful Unix system call "dup".)
Return Values
dup2 returns newfd. On error, -1 is returned, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    EBADF		oldfd is not a valid file handle, or newfd is a value that cannot be a valid file handle.
    EMFILE		The process's file table was full, or a process-specific limit on open files was reached.
 */
int sys_dup2(int oldfd, int newfd)
{
	(void)oldfd, (void)newfd;
	return 0;
}

/*
 * Description
The current directory of the current process is set to the directory named by pathname.

Return Values
On success, chdir returns 0. On error, -1 is returned, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    ENODEV		The device prefix of pathname did not exist.
    ENOTDIR		A non-final component of pathname was not a directory.
    ENOTDIR		pathname did not refer to a directory.
    ENOENT		pathname did not exist.
    EIO		A hard I/O error occurred.
    EFAULT		pathname was an invalid pointer.
 */
int sys_chdir(userptr_t pathname)
{
	(void)pathname;
	return 0;
}


/*
Description
The name of the current directory is computed and stored in buf, an area of size buflen. The length of data actually stored, which must be non-negative, is returned.

Note: this call behaves like read - the name stored in buf is not 0-terminated.

This function is not meant to be called except by the C library; application programmers should use getcwd instead.
Return Values
On success, __getcwd returns the length of the data returned. On error, -1 is returned, and errno is set according to the error encountered.
Errors
The following error codes should be returned under the conditions given. Other error codes may be returned for other errors not mentioned here.


    ENOENT		A component of the pathname no longer exists.
    EIO		A hard I/O error occurred.
    EFAULT		buf points to an invalid address.
 */
int sys___getcwd(userptr_t buf, size_t buflen, int32_t *ret)
{
	(void)buf, (void)buflen, (void)ret;
	return 0;
}
