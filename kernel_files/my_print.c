// syscall #341
#include <linux/linkage.h>
#include <linux/kernel.h>

asmlinkage void sys_print_info(int pid, long start_int, long start_deci, long end_int, long end_deci)
{
    printk(KERN_INFO "[Project1] %d %ld.%09ld %ld.%09ld\n", pid, start_int, start_deci, end_int, end_deci);
}

