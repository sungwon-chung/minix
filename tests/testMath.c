#include <hwsim/minix.h>
#include <hwsim/hardware.h>

int main()
{

    TracePrintf(0, "Starting testMath test - should trigger TrapMathHandler\n");

    int t1 = 1;
    t1--;
    int trigger = 0 / t1;
    trigger = trigger + 1;
   return 0;
}