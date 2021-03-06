/*
 *  U-Boot - x86 Startup Code
 *
 * (C) Copyright 2008-2011
 * Graeme Russ, <graeme.russ@gmail.com>
 *
 * (C) Copyright 2002
 * Daniel Engström, Omicron Ceti AB, <daniel@omicron.se>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <asm/global_data.h>
#include <asm/post.h>
#include <asm/processor.h>
#include <asm/processor-flags.h>
#include <generated/generic-asm-offsets.h>
#include <generated/asm-offsets.h>

.section .text
.code32
.globl _start
.type _start, @function
.globl _x86boot_start
_x86boot_start:
	/*
	 * This is the fail-safe 32-bit bootstrap entry point.
	 *
	 * This code is used when booting from another boot loader like
	 * coreboot or EFI. So we repeat some of the same init found in
	 * start16.
	 */
	cli
	cld

	/* Turn off cache (this might require a 486-class CPU) */
	movl	%cr0, %eax
	orl	$(X86_CR0_NW | X86_CR0_CD), %eax
	movl	%eax, %cr0
	wbinvd

	/* Tell 32-bit code it is being entered from an in-RAM copy */
	movl	$GD_FLG_WARM_BOOT, %ebx

	/*
	 * Zero the BIST (Built-In Self Test) value since we don't have it.
	 * It must be 0 or the previous loader would have reported an error.
	 */
	movl	$0, %ebp

	jmp	1f

	/* Add a way for tools to discover the _start entry point */
	.align	4
	.long	0x12345678
_start:
	/*
	 * This is the 32-bit cold-reset entry point, coming from start16.
	 * Set %ebx to GD_FLG_COLD_BOOT to indicate this.
	 */
	movl	$GD_FLG_COLD_BOOT, %ebx

	/* Save BIST */
	movl	%eax, %ebp
1:

	/* Save table pointer */
	movl	%ecx, %esi

	/* Load the segement registers to match the GDT loaded in start16.S */
#ifndef CONFIG_INHERIT_GDT
	movl	$(X86_GDT_ENTRY_32BIT_DS * X86_GDT_ENTRY_SIZE), %eax
	movw	%ax, %fs
	movw	%ax, %ds
	movw	%ax, %gs
	movw	%ax, %es
	movw	%ax, %ss
#endif

	/* Clear the interrupt vectors */
	lidt	blank_idt_ptr

	/*
	 * Critical early platform init - generally not used, we prefer init
	 * to happen later when we have a console, in case something goes
	 * wrong.
	 */
	jmp	early_board_init
.globl early_board_init_ret
early_board_init_ret:
	post_code(POST_START)

	/* Initialise Cache-As-RAM */
	jmp	car_init
.globl car_init_ret
car_init_ret:
#ifndef CONFIG_HAVE_FSP
	/*
	 * We now have CONFIG_SYS_CAR_SIZE bytes of Cache-As-RAM (or SRAM,
	 * or fully initialised SDRAM - we really don't care which)
	 * starting at CONFIG_SYS_CAR_ADDR to be used as a temporary stack
	 * and early malloc() area. The MRC requires some space at the top.
	 *
	 * Stack grows down from top of CAR. We have:
	 *
	 * top-> CONFIG_SYS_CAR_ADDR + CONFIG_SYS_CAR_SIZE
	 *	MRC area
	 *	global_data with x86 global descriptor table
	 *	early malloc area
	 *	stack
	 * bottom-> CONFIG_SYS_CAR_ADDR
	 */
	movl	$(CONFIG_SYS_CAR_ADDR + CONFIG_SYS_CAR_SIZE - 4), %esp
#ifdef CONFIG_DCACHE_RAM_MRC_VAR_SIZE
	subl	$CONFIG_DCACHE_RAM_MRC_VAR_SIZE, %esp
#endif
#else
	/*
	 * U-Boot enters here twice. For the first time it comes from
	 * car_init_done() with esp points to a temporary stack and esi
	 * set to zero. For the second time it comes from fsp_init_done()
	 * with esi holding the HOB list address returned by the FSP.
	 */
#endif
	/* Set up global data */
	mov	%esp, %eax
	call	board_init_f_mem
	mov	%eax, %esp

	/*
	 * Debug UART is available here although it may not be plumbed out
	 * to pins depending on the board. To use it:
	 *
	 * call  debug_uart_init
	 * mov   $'a', %eax
	 * call  printch
	 */

	/* Get address of global_data */
	mov	%fs:0, %edx
#ifdef CONFIG_HAVE_FSP
	/* Store the HOB list if we have one */
	test	%esi, %esi
	jz	skip_hob
	movl	%esi, GD_HOB_LIST(%edx)

	/*
	 * After fsp_init() returns, the stack has already been switched to a
	 * place within system memory as defined by CONFIG_FSP_TEMP_RAM_ADDR.
	 * Enlarge the size of malloc() pool before relocation since we have
	 * plenty of memory now.
	 */
	subl	$CONFIG_FSP_SYS_MALLOC_F_LEN, %esp
	movl	%esp, GD_MALLOC_BASE(%edx)
skip_hob:
#else
	/* Store table pointer */
	movl	%esi, GD_TABLE(%edx)
#endif
	/* Store BIST */
	movl	%ebp, GD_BIST(%edx)

	/* Set parameter to board_init_f() to boot flags */
	post_code(POST_START_DONE)
	xorl	%eax, %eax

	/* Enter, U-Boot! */
	call	board_init_f

	/* indicate (lack of) progress */
	movw	$0x85, %ax
	jmp	die

.globl board_init_f_r_trampoline
.type board_init_f_r_trampoline, @function
board_init_f_r_trampoline:
	/*
	 * SDRAM has been initialised, U-Boot code has been copied into
	 * RAM, BSS has been cleared and relocation adjustments have been
	 * made. It is now time to jump into the in-RAM copy of U-Boot
	 *
	 * %eax = Address of top of new stack
	 */

	/* Stack grows down from top of SDRAM */
	movl	%eax, %esp

	/* See if we need to disable CAR */
.weak	car_uninit
	movl	$car_uninit, %eax
	cmpl	$0, %eax
	jz	1f

	call	car_uninit
1:
	/* Re-enter U-Boot by calling board_init_f_r() */
	call	board_init_f_r

die:
	hlt
	jmp	die
	hlt

blank_idt_ptr:
	.word	0		/* limit */
	.long	0		/* base */

	.p2align	2	/* force 4-byte alignment */

	/* Add a multiboot header so U-Boot can be loaded by GRUB2 */
multiboot_header:
	/* magic */
	.long	0x1badb002
	/* flags */
	.long	(1 << 16)
	/* checksum */
	.long	-0x1BADB002 - (1 << 16)
	/* header addr */
	.long	multiboot_header - _x86boot_start + CONFIG_SYS_TEXT_BASE
	/* load addr */
	.long	CONFIG_SYS_TEXT_BASE
	/* load end addr */
	.long	0
	/* bss end addr */
	.long	0
	/* entry addr */
	.long	CONFIG_SYS_TEXT_BASE
