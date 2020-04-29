// syscall #342
#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/timer.h>

asmlinkage long long sys_get_time(void) {
    struct timespec t;
    getnstimeofday(&t);
    return t.tv_sec * 1000000000 + t.tv_nsec;
}
