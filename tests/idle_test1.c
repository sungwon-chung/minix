#include <stdio.h>
#include <stdlib.h>
#include <hwsim/minix.h>
#include <hwsim/hardware.h>
int main() 
{
    Fork();
    while(1) {
        Pause();
    }
}
