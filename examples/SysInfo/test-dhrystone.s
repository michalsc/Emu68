/*
    Perform BusTest - read 128 bytes in a row from memory
*/

	.globl _SI_BusTest
_SI_BusTest:
    movem.l d1/d2/a0,-(sp)
    movea.l #0, A0
Loop:
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  move.l (A0)+,D1
  move.l (A0)+,D2
  subq.l #1,d0
 bne Loop
    movem.l (sp)+, d1/d2/a0
    rts


/*
    This is an unofficial code snipped from SysInfo 3.x - it performs Dhrystone-like test 
    loop run 100 times (or adjustable, in our case) and calculates time per loop as well as
    number of loops per second (SysInfo Dhrystones).
    
    The SysInfo MIPS is calculated by dividing SysInfo Dhrystones by 958
*/
	.text
	.globl _SI_Start

_SI_Start:
	move.l	#100,D2
	.globl _SI_Start_Nr
_SI_Start_Nr:
	movem.l d7,-(a7)
u64a4:
	addq.l 	#1,d7
	 bsr 	testmul	
	 bsr 	testdiv	
 	 bsr 	testshift	
 	 bsr 	testshift	
	 bsr testlogic		
	 bsr testlogic		
	  bsr testlogic	
	 bsr testlogic	
	 bsr testlogic	
	 bsr testwork	
	 bsr testwork	
	 bsr testwork	
	 bsr testwork	
	 bsr testwork	
 	subq.l #1,d2	
 	bne.s u64a4
 	movem.l (a7)+, d7
 
	rts

	.globl testmul
testmul:			
 move.l #0x7fff,d1		
 move.l #0x7fff,d0		
 muls d1,d0			
 move.l #0x7fff,d1		
 moveq #-1,d0			
 mulu d1,d0			
 rts				

	.globl testdiv
testdiv: 			
 move.l #0xffff,d0		
 move.l #0x7fff,d1		
 divs d1,d0			
 move.l #0xffff,d0		
 move.l #0x7fff,d1		
 divu d1,d0			
 rts				

 nop
 nop
	.globl testshift
testshift: 			
 asl.l #8,d0			
 asr.l #8,d0			
 rol.l #8,d0			
 ror.l #8,d0			
 rts				
			
 nop
 nop
	.globl testwork
testwork:			

 add.l d0,d0			
 add.l d0,d0			
 add.l d0,d0			
 add.l d0,d0			
 addi.l #0x3039,d0		
 subi.l #0x3039,d0		
 lea u79b0,a0			
 move.l 4(a0),d0		
 move.l 8(a0),d0		
 move.l 0xc(a0),d0		
 bsr u673e			
 ext.w d0			
 ext.l d0  			
 move.l #0x3039,d0		
 swap d0			
 exg d0,d1			
 clr.l d1			
 neg.l d0			

 add.l d0,d0			
 add.l d0,d0			
 add.l d0,d0			
 add.l d0,d0			
 addi.l #0x3039,d0		
 subi.l #0x3039,d0		
 movea.l #u79b0,a0		
 move.l 4(a0),d0
 move.l 8(a0),d0
 move.l 0xc(a0),d0
 bsr u673e
 ext.w d0
 ext.l d0
 move.l #0x3039,d0
 swap d0
 exg d0,d1
 clr.l d1
 neg.l d0			

 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 addi.l #0x3039,d0
 subi.l #0x3039,d0
 movea.l #u79b0,a0
 move.l 4(a0),d0
 move.l 8(a0),d0
 move.l 0xc(a0),d0
 bsr u673e
 ext.w d0
 ext.l d0
 move.l #0x3039,d0
 swap d0
 exg d0,d1
 clr.l d1
 neg.l d0

 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 addi.l #0x3039,d0
 subi.l #0x3039,d0
 movea.l #u79b0,a0
 move.l 4(a0),d0
 move.l 8(a0),d0
 move.l 0xc(a0),d0
 bsr u673e
 ext.w d0
 ext.l d0
 move.l #0x3039,d0
 swap d0
 exg d0,d1
 clr.l d1
 neg.l d0

 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 addi.l #0x3039,d0
 subi.l #0x3039,d0
 movea.l #u79b0,a0
 move.l 4(a0),d0
 move.l 8(a0),d0
 move.l 0xc(a0),d0
 bsr u673e
 ext.w d0
 ext.l d0
 move.l #0x3039,d0
 swap d0
 exg d0,d1
 clr.l d1
 neg.l d0

 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 addi.l #0x3039,d0
 subi.l #0x3039,d0
 movea.l #u79b0,a0
 move.l 4(a0),d0
 move.l 8(a0),d0
 move.l 0xc(a0),d0
 bsr u673e
 ext.w d0
 ext.l d0
 move.l #0x3039,d0
 swap d0
 exg d0,d1
 clr.l d1
 neg.l d0

 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 addi.l #0x3039,d0
 subi.l #0x3039,d0
 movea.l #u79b0,a0
 move.l 4(a0),d0
 move.l 8(a0),d0
 move.l 0xc(a0),d0
 bsr u673e
 ext.w d0
 ext.l d0
 move.l #0x3039,d0
 swap d0
 exg d0,d1
 clr.l d1
 neg.l d0

 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 add.l d0,d0
 addi.l #0x3039,d0
 subi.l #0x3039,d0
 movea.l #u79b0,a0
 move.l 4(a0),d0
 move.l 8(a0),d0
 move.l 0xc(a0),d0
 bsr u673e
 ext.w d0
 ext.l d0
 move.l #0x3039,d0
 swap d0
 exg d0,d1
 clr.l d1
 neg.l d0

 bra u673a		
 nop
u673a:
 move.w #0x7b,d1		
u673e:
 rts			

 nop
 nop

	.globl testlogic
testlogic:		
 andi.l #0xffff,d0	
 ori.l #0xffff,d0	
 eori.l #0xffff,d0	
 and.l d1,d0		
 not.l d0		
 rts			

u79b0: dc.l 10,100,1000,10000

