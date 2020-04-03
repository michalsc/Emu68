# Register Allocator

In order to utilize general purpose registers of the host CPU in most efficient way, Emu68 does not provide any direct mapping between registers of host and of emulated CPU. Instead, the host registers are managed by an allocator and can be used to either map m68k register to a host register for certain while, or can be allocated for temporary reasons. 

The mapping between m68k and host register remains active within the translation unit until it is removed by the RA_AllocARMRegister function or the translation unit ends. Every time given mapping is requested, it is moved upward in the LRU queue, so that the least recently used map will be the first to be released. Additionaly, a mapped m68k register can be marked as dirty, so that it's value gets updated as soon as its assignment to arm register is released.

There are following functions intended for general use available:

```
  uint8_t RA_AllocARMRegister(uint32_t **stream)
  uint8_t RA_MapM68kRegister(uint32_t **stream, uint8_t m68k_reg)
  uint8_t RA_MapM68kRegisterForWrite(uint32_t **stream, uint8_t m68k_reg)
  void RA_SetDirtyM68kRegister(uint32_t **stream, uint8_t m68k_reg);
  void RA_FreeARMRegister(uint32_t **stream, uint8_t arm_reg)
```

The first function allocates a register for temporary use. If there are no free registers available, the function will release least recently used M68k-host mapping if necessary. Care must be taken when using it, since in worst case the programmer may exhoust the register pool removing all m68k mappings at the same time.

The second function checks if there is already an active assignment of m68k-arm register pair. If this is the case, the function returns the corresponding ARM register number. If there was no active assignment, the function reserves the host register, assigns it to the corresponding m68k register and loads its value from the cpu context. 

The third function has similar functionality, i.e. it searches for active assignment of m68k register to the host and returns it, marking the register "dirty". If there was no active mapping it will be created but no value from m68k context will be pulled into ARM register. 

The function RA_SetDirtyM68kRegister marks that there was a change in value of given mapped m68k register. It means, the value will be updated in the m68k context once the mapping is removed.

The last function, RA_FreeARMRegister, releases host register allocated as temporary one. In case of mapped m68k registers the function does nothing.


Once allocated, the host register is locked within given translation unit until the 