#include <hwsim/minix.h>
#include <hwsim/hardware.h>


int main(int argc, char **argv) 
 
{
    argc = argc;
    argv = argv;
    int pid = GetPid();
    int status;
    int childPID;
    int statusCopy = status;

    TracePrintf(0, "testExitWait - ORIGINAL Process PID %d will wait; it has no children and no exited children\n", pid);
    childPID = Wait(&status);
    TracePrintf(0, "testExitWait - ORIGINAL Process PID %d finished waiting: expected childPID = -1, actual childPID = %d. Expected status = %d, actual status = %d\n", pid, childPID, statusCopy, status);

    int fpid = Fork();

    if (fpid == 0) {
        TracePrintf(0, "testExitWait - Child process : %d\n", fpid);
        Delay(6);
        TracePrintf(0, "testExitWait - Child process PID %d will now exit\n", fpid);
        Exit(20);
        
    } else {
         TracePrintf(0, "testExitWait - Parent Process PID %d will wait. It has one delayed child but no exited children\n", fpid);
         childPID = Wait(&status);
         TracePrintf(0, "testExitWait - Parent Process PID %d finished waiting: expected childPID = 0, actual childPID = %d. Expected status = 20, actual status = %d\n", fpid, childPID, status);
    }

    // Now test multiple fork calls from the same parent to test statusQ.
    TracePrintf(0, "testExitWait - Parent Process PID %d will fork multiple children processes\n");
    int fork_calls = 3;
    int i;
    for (i = 1; i <= fork_calls; i++) {

        fpid = Fork();
        if (fpid == 0) {
            TracePrintf(0, "testExitWait - Child process PID %d will exit\n", fpid);
            Exit(i);
        }
    }

    TracePrintf(0, "testExitWait - Parent Process PID %d will wait on its chilren");
    for (i = 1; i <= fork_calls; i++) {
        childPID = Wait(&status);
        TracePrintf(0,"testExitWait - Parent Process PID %d finished waiting: childPid = %d, status = %d\n", childPID, status);
    }
    
    return 0;
    
}