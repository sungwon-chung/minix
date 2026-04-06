#include <hwsim/minix.h>
#include <hwsim/hardware.h>

void
termRead(int tty_id) {
    int size = TERMINAL_MAX_LINE;
    size = 3;
    char buf[size];

    // Full line write
    TtyPrintf(tty_id, "This is a TEXT line written __ to the terminal!!\n");
    int amnt = TtyRead(tty_id, buf, size);
    TtyPrintf(tty_id, "Read 1: %s\n", buf);

    for (int i = 0; i < size; i++) {
        buf[i] = 0;
        amnt = amnt;
    }

    // Partial write (no newline)
    TtyPrintf(tty_id, "Some more text\n");
    TtyPrintf(tty_id, "Partial write issued. TtyRead should now block...\n");

    

    amnt = TtyRead(tty_id, buf, size); // This should block
    TtyPrintf(tty_id, "Read 2 returned: %s\n", buf); // Shouldn’t run yet

    for (int i = 0; i < size; i++) {
        buf[i] = 0;
        amnt = amnt;
    }

    TtyRead(tty_id, buf, size); // This should now complete
    TtyPrintf(tty_id, "Read 3 : %s\n", buf);

    for (int i = 0; i < size; i++) {
        buf[i] = 0;
        amnt = amnt;
    }
}

int
main() {

    while (1) {
        TracePrintf(0, "Starting terminal read test on all 4 terminals...\n");
        for (int i = 0; i < 4; i++) {
            termRead(i);
        }
    }
    return 0;
}