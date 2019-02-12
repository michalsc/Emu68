# Status of M68K Opcode implementation

Group | Opcode | Mode          |X|N|Z|V|C| Implemented | Tested
--|------------|---------------|-|-|-|-|-|-------------|-------
0 | ORI to CCR | x             |x|x|x|x|x| NO          | NO
0 | ORI to SR  | x             | | | | | | NO          | NO
0 | ORI        | #imm, _Ea_    |-|*|*|0|0| YES         | YES
0 | ANDI to CCR| #imm          |x|x|x|x|x| NO          | NO
0 | ANDI to SR | #imm          | | | | | | NO          | NO
0 | ANDI       | #imm, _Ea_    |-|*|*|0|0| YES         | YES
0 | SUBI       | #imm, _Ea_    |*|*|*|*|*| YES         | YES
0 | ADDI       | #imm, _Ea_    |*|*|*|*|*| YES         | YES
0 | CMP2       |               |-|*|*|*|*| NO          | NO
0 | CHK2       |               |-|*|*|*|*| NO          | NO
0 | EORI to CCR| #imm          |x|x|x|x|x| NO          | NO
0 | EORI to SR | #imm          | | | | | | NO          | NO
0 | EORI       | #imm, _Ea_    |-|*|*|0|0| YES         | YES
0 | CMPI       |               |-|*|*|*|*| NO          | NO
0 | BTST       | #imm, _Ea_    |-|-|*|-|-| YES         | NO
0 | BCHG       | #imm, _Ea_    |-|-|*|-|-| YES         | NO
0 | BCLR       | #imm, _Ea_    |-|-|*|-|-| YES         | NO
0 | BSET       | #imm, _Ea_    |-|-|*|-|-| YES         | NO
0 | MOVES      |               | | | | | | NO          | NO
0 | CAS2       |               |-|*|*|*|*| NO          | NO
0 | CAS        |               |-|*|*|*|*| NO          | NO
0 | BTST       | Dn, _Ea_      |-|-|*|-|-| YES         | NO
0 | BCHG       | Dn, _Ea_      |-|-|*|-|-| YES         | NO
0 | BCLR       | Dn, _Ea_      |-|-|*|-|-| YES         | NO
0 | BSET       | Dn, _Ea_      |-|-|*|-|-| YES         | NO
0 | MOVEP      |               | | | | | | NO          | NO

to be continued...