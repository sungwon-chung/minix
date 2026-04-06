#include <stdio.h>
#include <stdlib.h>
#include <hwsim/minix.h>
#include <hwsim/hardware.h>
int main(int argc, char** argv) 
{
    TtyPrintf(0, "InitChild Process START :\n");
    argc = argc;
    argv = argv;
    int pid = GetPid();
    while(1) {
        TtyPrintf(0, "InitChild Process : %d\n", pid);
        // Delay for 2 clock ticks.
        Delay(5);
    }

}