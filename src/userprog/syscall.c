#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "userprog/process.h"

void s_exit (int status);
static void syscall_handler (struct intr_frame *);
void parse_syscall_args (uint32_t *sp, int num_args, ...);
int s_write (int fd, const void * buffer, unsigned size);
int s_read (int fd, void * buffer, unsigned size);
int s_open (const char * file);
pid_t s_exec (const char *cmd_line);
int s_wait (pid_t pid);
int s_create (const char *file, unsigned initial_size);
bool s_remove (const char *file);
int s_filesize(int fd);
int s_seek(int fd, unsigned position);
unsigned s_tell(int fd);
int s_close(int fd);
void s_halt();

struct lock fl;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&fl);
}

/* Helper function for processing a variable number of
   arguments passed on the stack.  This function demonstrates
   the C99 variable number of arguments feature (not in the
   original C specification).  It can be called like this:

        parse_syscall_args (sp, 1, &arg1);
        parse_syscall_args (sp, 2, &arg1, &arg2);

   The parameters arg1, arg2, etc., must be uint32_t. */
void
parse_syscall_args (uint32_t *sp, int num_args, ...)
{
  va_list arg;
  int i;

  va_start (arg, num_args);
  for (i = 0; i < num_args; i++)
    *(va_arg (arg, uint32_t*)) = *++sp;
  va_end (arg);

  if ((uint32_t)sp >= (uint32_t)PHYS_BASE)
    thread_exit ();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t *sp = f -> esp;
  
  if(!is_user_vaddr(sp))
	s_exit(-1);

  if(!(is_user_vaddr(sp +1) && is_user_vaddr(sp+2) && is_user_vaddr(sp+3)))	
	s_exit(-1);

  if (*sp < SYS_HALT || *sp > SYS_INUMBER)
	s_exit(-1);
  
  uint32_t syscallnum = *sp;
  uint32_t first_arg = 0;
  uint32_t second_arg = 0;
  uint32_t third_arg = 0;

  struct thread * t = thread_current();

  switch(syscallnum)
  {
  case SYS_EXIT:
    parse_syscall_args(sp, 1, &first_arg);
    f->eax = first_arg;
    s_exit((int)first_arg);
    
    break;
  case SYS_READ:
    parse_syscall_args(sp, 3, &first_arg, &second_arg, &third_arg);
    f -> eax = s_read((int)first_arg, (void*)second_arg, third_arg);
    break;
  case SYS_WRITE:
    parse_syscall_args(sp, 3, &first_arg, &second_arg, &third_arg);
    f -> eax = s_write((int)first_arg, (const void *)second_arg, third_arg);
    break;
  
  case SYS_OPEN:
	parse_syscall_args(sp, 1, &first_arg);
	f-> eax = s_open((const char *) first_arg);
    break;

  case SYS_HALT:
	s_halt();
    break;

  case SYS_EXEC:
	parse_syscall_args(sp, 1, &first_arg);
	f -> eax = s_exec((const char *) first_arg);
    break;

  case SYS_WAIT:
	parse_syscall_args(sp, 1, &first_arg);
	f -> eax = s_wait((pid_t) first_arg);
    break;

  case SYS_CREATE:
	parse_syscall_args(sp, 2, &first_arg, &second_arg);
	f -> eax = s_create ((const char *) first_arg, (unsigned) second_arg);
    break;

  case SYS_REMOVE:
	parse_syscall_args(sp, 1, &first_arg);
	f -> eax = s_remove((const char *) first_arg);
    break;

  case SYS_FILESIZE:
	parse_syscall_args(sp, 1, &first_arg);
	f -> eax = s_filesize((int) first_arg);
    break;

  case SYS_SEEK:
	parse_syscall_args(sp, 2, &first_arg, &second_arg);
	f -> eax = s_seek((int) first_arg, (unsigned) second_arg); 
    break;

  case SYS_TELL:
	parse_syscall_args(sp, 1, &first_arg);
	f -> eax = s_tell((int) first_arg);
    break;

  case SYS_CLOSE:
	parse_syscall_args(sp, 1, &first_arg);
	f -> eax = s_close((int) first_arg);
    break;

  default:
    	s_exit((int) first_arg);
    break;
  }
}

void
s_halt()
{
	shutdown_power_off();
}

int
s_open (const char * file)
{
    if (!file) 
    	return -1;
    
    if (!is_user_vaddr(file))
	s_exit(-1);
    else 
    {
	struct thread * t;
    	t = thread_current();
    	t -> files[t->fd] = filesys_open(file);
	
	if (!t -> files[t->fd])
		return -1;
    	
	int tmp = t -> fd;
    	t -> fd ++;
    	return tmp+2; 
    }
}

void
s_exit (int status)
{
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}

int
s_read (int fd, void * buffer, unsigned size)
{
   if (!is_user_vaddr(buffer) || !is_user_vaddr(buffer + size))
  {
      return -1;
  }
  if (fd == 1)
  {
      return -1;
  }
  else if (fd == 0)
  {
      char * new_buffer = (char *)buffer;
      unsigned i = 0; 
      for (i = 0; i < size; i++)
        {
          new_buffer[i] = (uint8_t)input_getc();
        }
      return i;
  } 

  else if (fd > 1)
  {
      struct thread * t = thread_current();
      struct file * f = t -> files[fd -2];
      if(f == NULL)
      {
	return -1;
      }

      return (int)file_read(f, buffer,size);
  }
}

int
s_write (int fd, const void * buffer, unsigned size)
{

  if (!is_user_vaddr(buffer) || !is_user_vaddr(buffer + size))
  {
      return -1;
  }

  if (fd == 1)
  {
      putbuf((const char*)buffer, size);
      return size;
  }
  else if (fd == 0)
  {
      return -1;
  } 

  else if (fd > 1)
  {
      struct thread * t = thread_current();
      struct file * f = t -> files[fd -2];
      if(f == NULL)
      {
	return -1;
      }
      
      return (int)file_write(f, buffer,size);
  }
}

int 
s_create (const char *file, unsigned initial_size)
{
	if (!is_user_vaddr(file) || !file)
		return -1;
	
	return filesys_create(file, initial_size);
}
bool
s_remove (const char *file)
{
	bool result = false;
	struct thread * t = thread_current();
	if (!file) 
		return false;
	
	if(!(is_user_vaddr(file)))
		s_exit(-1);
	else 
	{
		bool result;
		//lock_acquire(&fl);
		result = filesys_remove(file);
		//lock_release(&fl);
		t -> fd -=2;
	}
	return result;
}
int 
s_filesize(int fd)
{
	struct thread * t = thread_current();
	struct file * f = t -> files[fd];
	if (!f)
		return -1;
	return file_length(f);
}
int
s_seek(int fd, unsigned position)
{
	struct thread * t = thread_current();
	struct file * f = t -> files[fd]; 
	if (!f)
		return -1;
	file_seek( f, position);
	return 0;
}
unsigned 
s_tell(int fd)
{
	struct thread * t;
	struct file * f;
        
	t = thread_current();	
	f = t -> files[fd];
	if (!f)
		return -1;
	
	return file_tell(f);
}
int
s_close(int fd)
{
	struct thread * t = thread_current();
	if (!(t -> files[fd]))
		return 0;
	file_close(fd);
	return 1;
}

pid_t 
s_exec (const char *cmd_line)
{
	if(!cmd_line || !is_user_vaddr(cmd_line))
		s_exit(-1);
	pid_t result;
	//lock_acquire(&fl);
	result = process_execute(cmd_line);
	//lock_release(&fl);	
	return result;
}
int 
s_wait (pid_t pid)
{
	int result;
	result = process_wait(pid);
	return result;
}





















