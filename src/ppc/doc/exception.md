# Register usage on exception entry

sprg3 = r0
sprg2 = lr
sprg1 = srr1
sprg0 = srr0

spr 944 = PowerPC Base 

There is some stack space reserved on the exception entry, defined as STACK_ALLOC_SIZE. This
one consists of RED_ZONE_SIZE of 256 bytes and additional 32 bytes giving enough room for link
frame. On this stack temporary variables (if any) are placed:

0(r1) - the register frame where cpu context is stored to
4(r1) - CR register
8(r1) - r4
