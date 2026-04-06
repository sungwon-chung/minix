#include <hwsim/minix.h>
#include <hwsim/hardware.h>
#include "../src/kernel.h"

int main(int argc, char** argv) 
{

    // int largestack[4096*100];

    argc = argc;
    argv = argv;

    int wfork = Fork();
    if (wfork == 0) {
        TtyPrintf(0, "Init Process - Child proces PID %d. SDFSDF \n", wfork);
        while (wfork < 3) {
            Delay(3);
            wfork++;
        }
        TtyPrintf(0, "Init Process - Child proces PID %d. RETURNING\n", wfork);
        return 0;
    } else {
        int ref2 = Fork();
        if (ref2 == 0) {
            while (ref2 < 3) {
                Delay(5);
                ref2++;
            }
            TtyPrintf(0, "Init Process - Child proces PID %d. REF \n", ref2);
            return 0;
        } else {
            TtyPrintf(0, "waiting \n");
            Wait(&wfork);
            TtyPrintf(0, "waiting 2 \n");
            Wait(&wfork);
            TtyPrintf(0, "waiting COMPLETE \n");
            Fork();
            // return 0 ; // uncomment to proceed further
        }
    }



    /////////// TEST MATH BELOW///////

    // int z = 0;
    // int y = 6;

    // int f = y / z;

    // f = f;

    ///////////// TEST MATH ABOVE///////

    int *b = malloc(4096 * 2);
    b = b;
    int my_pid = GetPid();
    TtyPrintf(0, "Init Process with PID %d. Starting\n", my_pid);

    Fork();

    my_pid = GetPid();
    
    int AF = 0;
    while (AF < 5) {
        TtyPrintf(0, "Init Process with PID %d. PAUSED\n", my_pid);
        Pause();
        AF++;
    }

    
    // The user process allocates 4 pages past the brk. 
    TtyPrintf(0, "Init Process with PID %d. PRE Allocating 4 pages heap \n", my_pid);
    int *e = malloc((4*PAGESIZE));

    TtyPrintf(0, "Init Process with PID %d. FREE 4 pages\n", my_pid);

    free(e);

    TtyPrintf(0, "Init Process with PID %d. PRE GROW STACK\n", my_pid);
    TtyPrintf(0, "LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLOOOOOOOOOOOOOOOOONGGGGGGGGGGGGGGGGGGGGG\n");

    TtyPrintf(0, "LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLOOOOOOOOOOOOOOOOONGGGGGGGGGGGGGGGGGGGGG\n");

    TtyPrintf(0, "LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLOOOOOOOOOOOOOOOOONGGGGGGGGGGGGGGGGGGGGG\n");

    int c[4094 * 2];
    c[0] = 1;
    TtyPrintf(0, "%d\n", c[0]);

    while(c[0] < 4) {
        TtyPrintf(0, "Init Process : %d\n", my_pid);
        // Delay for 2 clock ticks.
        Delay(5);
        c[0]++;
    }

    // Write another small program that does not do much. Call Fork and Exec from your init proces.

    int fpid = Fork();
    my_pid = GetPid();
    if (fpid == 0) { 
        
        char* args= {""};
        // Test Exec failure.
        int isError3 = Exec("doesNotExist.c", &args);
        TtyPrintf(0, "Init Process - Child proces PID %d. Exec failure: expected = -1, actual = %d\n", fpid, isError3);

        // Test Exec success.
        int isError4 = Exec("initChild", argv);
        if (isError4 != ERROR) {
            TtyPrintf(0, "Init Process - Child proces PID %d. - Exec success; no return: actual = %d\n", fpid, isError4);
        }
    } else {
        while(1) {
            TtyPrintf(0, "Init Process - PARENT : %d\n", my_pid);
            Delay(1);
        }
    }
    return 0;
}