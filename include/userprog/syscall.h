#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);
/* project2 */
struct page * check_address(void *addr);
struct lock filesys_lock;
/* project2 */

struct file *process_get_file(int fd);
void process_close_file(int fd);
#endif /* userprog/syscall.h */
