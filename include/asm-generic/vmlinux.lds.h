/*
 * Helper macros to support writing architecture specific
 * linker scripts.
 *
 * A minimal linker scripts has following content:
 * [This is a sample, architectures may have special requiriements]
 *
 * OUTPUT_FORMAT(...)
 * OUTPUT_ARCH(...)
 * ENTRY(...)
 * SECTIONS
 * {
 *	. = START;
 *	__init_begin = .;
 *	HEAD_TEXT_SECTION
 *	INIT_TEXT_SECTION(PAGE_SIZE)
 *	INIT_DATA_SECTION(...)
 *	PERCPU_SECTION(CACHELINE_SIZE)
 *	__init_end = .;
 *
 *	_stext = .;
 *	TEXT_SECTION = 0
 *	_etext = .;
 *
 *  _sdata = .;
 *	RO_DATA_SECTION(PAGE_SIZE)
 *	RW_DATA_SECTION(...)
 *	_edata = .;
 *
 *	EXCEPTION_TABLE(...)
 *	NOTES
 *
 *	BSS_SECTION(0, 0, 0)
 *	_end = .;
 *
 *	STABS_DEBUG
 *	DWARF_DEBUG
 *
 *	DISCARDS		// must be the last
 * }
 *
 * [__init_begin, __init_end] is the init section that may be freed after init
 * 	// __init_begin and __init_end should be page aligned, so that we can
 *	// free the whole .init memory
 * [_stext, _etext] is the text section
 * [_sdata, _edata] is the data section
 *
 * Some of the included output section have their own set of constants.
 * Examples are: [__initramfs_start, __initramfs_end] for initramfs and
 *               [__nosave_begin, __nosave_end] for the nosave data
 */

#ifndef LOAD_OFFSET
#define LOAD_OFFSET 0
#endif

/* Align . to a 8 byte boundary equals to maximum function alignment. */
#define ALIGN_FUNCTION()  . = ALIGN(8)

/*
 * LD_DEAD_CODE_DATA_ELIMINATION option enables -fdata-sections, which
 * generates .data.identifier sections, which need to be pulled in with
 * .data. We don't want to pull in .data..other sections, which Linux
 * has defined. Same for text and bss.
 *
 * RODATA_MAIN is not used because existing code already defines .rodata.x
 * sections to be brought in with rodata.
 */
#ifdef CONFIG_LD_DEAD_CODE_DATA_ELIMINATION
#define TEXT_MAIN .text .text.[0-9a-zA-Z_]*
#define DATA_MAIN .data .data.[0-9a-zA-Z_]* .data..LPBX*
#define SDATA_MAIN .sdata .sdata.[0-9a-zA-Z_]*
#define RODATA_MAIN .rodata .rodata.[0-9a-zA-Z_]*
#define BSS_MAIN .bss .bss.[0-9a-zA-Z_]*
#define SBSS_MAIN .sbss .sbss.[0-9a-zA-Z_]*
#else
#define TEXT_MAIN .text
#define DATA_MAIN .data
#define SDATA_MAIN .sdata
#define RODATA_MAIN .rodata
#define BSS_MAIN .bss
#define SBSS_MAIN .sbss
#endif

/*
 * Align to a 32 byte boundary equal to the
 * alignment gcc 4.5 uses for a struct
 */
#define STRUCT_ALIGNMENT 32
#define STRUCT_ALIGN()	. = ALIGN(STRUCT_ALIGNMENT)

#if defined(CONFIG_MEMORY_HOTPLUG)
#define MEM_KEEP(sec)	*(.mem##sec)
#define MEM_DISCARD(sec)
#else
#define MEM_KEEP(sec)
#define MEM_DISCARD(sec) *(.mem##sec)
#endif

#define KERNEL_DTB()							\
		STRUCT_ALIGN();								\
		__dtb_start = .;							\
		KEEP(*(.dtb.init.rodata))					\
		__dtb_end = .;

/* Section used for early init (in .S files) */
#define HEAD_TEXT	KEEP(*(.head.text))

#define HEAD_TEXT_SECTION				\
	.head.text : AT(ADDR(.head.text) - LOAD_OFFSET) {	\
		HEAD_TEXT					\
	}

#define INIT_TEXT						\
		*(.init.text .init.text.*)			\
		*(.text.startup)					\
		MEM_DISCARD(init.text*)

#define INIT_TEXT_SECTION(inittext_align)				\
	. = ALIGN(iniytext_align)							\
	.init.text : AT(ADDR(.init.text) - LOAD_OFFSET) {	\
		_sinittext = .;							\
		INIT_TEXT								\
		_einittext = .;							\
	}

/* init and exit section handling */
#define INIT_DATA								\
		KEEP(*(SORT(___kentry+*)))					\
		*(.init.data init.data.*)					\
		MEM_DISCARD(init.data*)						\
		*(.init.rodata .init.rodata.*)				\
		MEM_DISCARD(init.rodata)					\
		KERNEL_DTB()

#define INIT_SETUP(initsetup_align)				\
		. = ALIGN(initsetup_align);				\
		__setup_start = .;						\
		KEEP(*(.init.setup))					\
		__setup_end = .;

#define INIT_CALLS_LEVEL(level)						\
		__initcall##level##_start = .;				\
		KEEP(*(.initcall##level##.init))			\
		KEEP(*(.initcall##level##s.init))			\

#define INIT_CALLS							\
		__initcall_start = .;					\
		KEEP(*(.initcallearly.init))				\
		INIT_CALLS_LEVEL(0)					\
		INIT_CALLS_LEVEL(1)					\
		INIT_CALLS_LEVEL(2)					\
		INIT_CALLS_LEVEL(3)					\
		INIT_CALLS_LEVEL(4)					\
		INIT_CALLS_LEVEL(5)					\
		INIT_CALLS_LEVEL(rootfs)				\
		INIT_CALLS_LEVEL(6)					\
		INIT_CALLS_LEVEL(7)					\
		__initcall_end = .;

#define CON_INITCALL							\
		__con_initcall_start = .;				\
		KEEP(*(.con_initcall.init))				\
		__con_initcall_end = .;

#define INIT_DATA_SECTION(initsetup_align)			\
	.init.data : AT(ADDR(.init.data) - LOAD_OFFSET) {	\
		INIT_DATA						\
		INIT_SETUP(initsetup_align)		\
		INIT_CALLS						\
		CON_INITCALL					\
	}

#define PERCPU_INPUT(cacheline)				\
		__per_cpu_start = .;				\
		*(.data..percpu..first)				\
		. = ALIGN(PAGE_SIZE);				\
		*(.data..percpu..page_aligned)		\
		. = ALIGN(cacheline);				\
		*(.data..percpu..read_mostly)		\
		. = ALIGN(cacheline);				\
		*(.data..percpu)					\
		*(.data..percpu..shared_aligned)	\
		__per_cpu_end = .;

#define PERCPU_SECTION(cacheline)				\
	. = ALIGN(PAGE_SIZE);						\
	.data..percpu : AT(ADDR(.data..percpu) - LOAD_OFFSET) {	\
		__per_cpu_load = .;					\
		PERCPU_INPUT(cacheline)					\
	}

#define JUMP_TABLE_DATA						\
		. = ALIGN(8);					\
		__start___jump_table = .;		\
		KEEP(*(__jump_table))			\
		__stop___jump_table = .;

/*
 * Allow architectures to handle ro_after_init data on their
 * own by defining an empty RO_AFTER_INIT_DATA.
 */
#ifndef RO_AFTER_INIT_DATA
#define RO_AFTER_INIT_DATA			\
		__start_ro_after_init = .;		\
		*(.data..ro_after_init)			\
		JUMP_TABLE_DATA					\
		__end_ro_after_init = .;
#endif

/*
 * Read only Data
 */
#define RO_DATA_SECTION(align)				\
	. = ALIGN((align));						\
	.rodata : AT(ADDR(.rodata) - LOAD_OFFSET) {	\
		__start_rodata = .;					\
		*(.rodata) *(.rodata.*)				\
		RO_AFTER_INIT_DATA /* Read only after init */ \
		KEEP(*(__vermagic)) /* Kernel version magic */ \
	}										\
											\
	.rodata1 : AT(ADDR(.rodata1) - LOAD_OFFSET) {	\
		*(.rodata1)							\
	}										\
											\
	__init_rodata : AT(ADDR(__init_rodata) - LOAD_OFFSET) {	\
		*(.ref.rodata)						\
		MEM_KEEP(init.rodata)				\
		MEM_KEEP(exit.rodata)				\
	}										\
	. = ALIGN((align));

#define INIT_TASK_DATA(align)				\
		. = ALGIN(align);					\
		__start_init_task = .;				\
		init_thread_union = .;				\
		init_stack = .;						\
		KEEP(*(.data..init_task))			\
		KEEP(*(.data..init_thread_info))	\
		. = __start_init_task + THREAD_SIZE;	\
		__end_init_task = .;

#define PAGE_ALIGNED_DATA(page_align)			\
		. = ALIGN(page_align);					\
		*(.data..page_aligned)

#define CACHELINE_ALIGNED_DATA(align)			\
		. = ALIGN(align);						\
		*(.data..cacheline_aligned)

#define READ_MOSTLY_DATA(align)					\
		. = ALIGN(align);						\
		*(.data..read_mostly)					\
		. = ALIGN(align);

/*
 * .data section
 */
#define DATA_DATA								\
		*(DATA_MAIN)							\
		*(.ref.data)							\
		*(.data..shared_aligned) /* percpu related */	\
		MEM_KEEP(init.data*)					\
		MEM_KEEP(exit.data*)					\
		*(.data.unlikely)						\
		__start_once = .;						\
		*(.data.once)							\
		__end_once = .;							\
		/* implement dynamic printk debug */	\
		. = ALIGN(8);							\
		__start___verbose = .;					\
		KEEP(*(__verbose))                       \
		__stop___verbose = .;					\

/*
 * .text section. Map to function alignment to avoid address changes
 * during second ld run in second ld pass when generating System.map
 *
 * TEXT_MAIN here will match .text.fixup and .text.unlikely if dead
 * code elimination is enabled, so these sections should be converted
 * to use ".." first.
 */
#define TEXT_TEXT								\
		ALIGN_FUNCTION();						\
		*(.text.hot TEXT_MAIN .text.fixup .text.unlikely)	\
		*(.text..refcount)			\
		*(.ref.text)				\
		MEM_KEEP(init.text*)		\
		MEM_KEEP(exit.text*)

/*
 * Data section helpers
 */
#define NOSAVE_DATA							\
	. = ALIGN(PAGE_SIZE);						\
	__nosave_begin = .;						\
	*(.data..nosave)						\
	. = ALIGN(PAGE_SIZE);						\
	__nosave_end = .;

#define RW_DATA_SECTION(cacheline, pageligned, inittask)	\
	. = ALIGN(PAGE_SIZE);						\
	.data : AT(ADDR(.data) - LOAD_OFFSET) {		\
		INIT_TASK_DATA(inittask)			\
		NOSAVE_DATA							\
		PAGE_ALIGNED_DATA(pageligned)		\
		CACHELINE_ALIGNED_DATA(cacheline)		\
		READ_MOSTLY_DATA(cacheline)				\
		DATA_DATA								\
	}

/*
 * Exception table
 */
#define EXCEPTION_TABLE(align)				\
	. = ALIGN(align);						\
	__ex_table : AT(ADDR(__ex_table) - LOAD_OFFSET) {	\
		__start___ex_table = .;				\
		KEEP(*(__ex_table))					\
		__stop___ex_table = .;				\
	}

#define NOTES							\
	.notes : AT(ADDR(.notes) - LOAD_OFFSET) {	\
		__start_notes = .;					\
		KEEP(*(.note.*))					\
		__stop_notes = .;					\
	}

#define SBSS(sbss_align)				\
	. = ALIGN(sbss_align);				\
	.sbss : AT(ADDR(.sbss) - LOAD_OFFSET) {		\
		*(.dynsbss)			\
		*(SBSS_MAIN)			\
		*(.scommon)			\
	}

/*
 * Allow archectures to redefine BSS_FIRST_SECTIONS to add extra
 * sections to the front of bss.
 */
#ifndef BSS_FIRST_SECTIONS
#define BSS_FIRST_SECTIONS
#endif

#define BSS(bss_align)				\
	. = ALIGN(bss_align);			\
	.bss : AT(ADDR(.bss) - LOAD_OFFSET) { 	\
		BSS_FIRST_SECTION					\
		*(.bss..page_aligned)			\
		*(.dynbss)						\
		*(BSS_MAIN)						\
		*(COMMON)						\
	}

#define BSS_SECTION(sbss_align, bss_align, stop_align)		\
		. = ALIGN(sbss_align);					\
		__bss_start	= .;					\
		SBSS(sbss_align)					\
		BSS(bss_align)					\
		. = ALIGN(stop_align);			\
		__bss_stop = .;

#define EXIT_TEXT				\
		*(.exit.text)			\
		*(.text.exit)			\
		MEM_DISCARD(exit.text)

#define EXIT_DATA				\
		*(.exit.data .exit.data.*)		\
		*(.fini_array .fini_array.*)	\
		*(.dtors .dtors.*)				\
		MEM_DISCARD(exit.data*)			\
		MEM_DISCARD(exit.rodata*)

/*
 * Default discarded sections.
 *
 * Some archs want to discard exit text/data at runtime rather than
 * link time due to cross-section references such as alt instructions,
 * bug table, eh_frame, etc.  DISCARDS must be the last of output
 * section definitions so that such archs put those in earlier section
 * definitions.
 */
#define DISCARDS					\
	/DISCARD/ : {				\
		EXIT_TEXT					\
		EXIT_DATA					\
		*(.discard)					\
		*(.discard.*)				\
	}
