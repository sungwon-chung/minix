#ifndef _CALLS_H
#define _CALLS_H

extern int KernelFork(void);
extern int KernelExec(char *filename, char **argvec);
extern void KernelExit(int status);
extern unsigned int KernelWait(int *status_ptr);
extern unsigned int KernelGetPid();
extern int KernelBrk(void *addr);
extern int KernelDelay(int clock_ticks);
extern int KernelTTYRead(int tty_id, void *buf, int len);
extern int KernelTTYWrite(int tty_id, void *buf, int len);
extern void PrintPageTable(uintptr_t pt_physical_address);
extern int VerifyInputString(char *str);
extern int VerifyArrayOfPointers(char **argvec);

#endif