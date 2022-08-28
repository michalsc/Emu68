# MMU support in Emu68

### NOTE

The document is a rough draft used by me for development purporses. The document is subject of further changes without a notice.

## Current state

At the moment Emu68 offers direct, linear 1:1 access to ARM memory and, in case of Pistorm variant, 1:1 mapping to ARM memory and Amiga chipset access through a page fault handler. This approach allows to use a mapping-free translation of m68k code to aarch64, i.e. loads and stores are applied without any global base offset and without a need to subroutine jump. This, of course incorporates MMU of aarch64 cpu already and because of that it does not allow Emu68 to use zero-cost (or in general cost effective) MMU support for m68k side.

## Incoming changes

In order to combine the current state with MMU support in the future several changes have to be applied to internals of Emu68. Most of them are not invasive, however. The larest one is transition from aarch64 supervisor mode to the hypervisor, the smaller ones are related to MMU table crawling, supervisor protection, write protection and access bits.

### Hypervisor mode

In the first part of MMU support, Emu68 and corresponding page fault handling will be transfered from supervisor to hypervisor mode. There, the 1:1 access to ARM memory and access to PiStorm throuhg page fault mechanism will be provided. After setting everything up, Emu68 will prepare 1:1 memory mapping for supervisor mode and will drop there. The 1:1 mapping will, in this case, not cover the PiStorm interfacing, as this part will be postponed exclusively to the hypervisor. The supervisor memory mapping will also include a 1:1 shadow at higher addresses (e.g. at 16GB) which will be used for instruction fetching, in order to distinguish between MMU map for instructions and MMU map for data.

### MMU activation/deactivation

Once the MMU translation is activated or deactivated by the m68k side, MMU tables for supervisor mode will be altered accordingly. If MMU is disabled, the default table for the lowest 4GB will be used (and corresponding one for the instruction MMU). Once m68k enables MMU, the 4GB area will be replaced with a custom, uninitialized root pointer. The root pointer will be changed every time the S bit is changed, in order to distinguis between user and supervisor-only access rights.

| S-bit | MMU disabled | MMU enabled |
| ---------- | :----: | :----: |
| User       | Code: default table<br />Data: default table | Code: user_i_table<br />Data: user_d_table |
| Supervisor | Code: default table<br />Data: default table | Code: super_i_table<br />Data: super_d_table |

### Necessity of CODE/DATA tables

By default M68k MMU does not recognize between code and data in its MMU table, but in order to support the MMU, one has to provide space for DTT0/1 and ITT0/1 - Data and Instruction transparent translation registers. These are used to introduce two regions each for data in instruction fetches which are 1:1 translated to physical addresses.

As a consequence the MMU crawler (corresponding to ATC cache) has to update up to four aarch64 MMU tables.

**NOTE**: DTT/ITT works even if MMU is disabled!



