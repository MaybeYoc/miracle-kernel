#ifndef __KERNEL_PRINTK__
#define __KERNEL_PRINTK__

#include <stdarg.h>
#include <linux/init.h>
#include <linux/kern_levels.h>
#include <linux/linkage.h>
#include <linux/cache.h>

extern const char linux_banner[];
extern const char linux_proc_banner[];

static inline int printk_get_level(const char *buffer)
{
	if (buffer[0] == KERN_SOH_ASCII && buffer[1]) {
		switch (buffer[1]) {
		case '0' ... '7':
		case 'd':	/* KERN_DEFAULT */
			return buffer[1];
		}
	}
	return 0;
}

static inline const char *printk_skip_level(const char *buffer)
{
	if (printk_get_level(buffer))
		return buffer + 2;

	return buffer;
}

static inline const char *printk_skip_headers(const char *buffer)
{
	while (printk_get_level(buffer))
		buffer = printk_skip_level(buffer);

	return buffer;
}

/* printk's without a loglevel use this.. */
#define MESSAGE_LOGLEVEL_DEFAULT CONFIG_MESSAGE_LOGLEVEL_DEFAULT

/* We show everything that is MORE important than this.. */
#define CONSOLE_LOGLEVEL_SILENT  0 /* Mum's the word */
#define CONSOLE_LOGLEVEL_MIN	 1 /* Minimum loglevel we let people use */
#define CONSOLE_LOGLEVEL_QUIET	 4 /* Shhh ..., when booted with "quiet" */
#define CONSOLE_LOGLEVEL_DEFAULT 7 /* anything MORE serious than KERN_DEBUG */
#define CONSOLE_LOGLEVEL_DEBUG	10 /* issue debug messages */
#define CONSOLE_LOGLEVEL_MOTORMOUTH 15	/* You can't shut this one up */

extern int console_printk[];

#define console_loglevel (console_printk[0])
#define default_message_loglevel (console_printk[1])
#define minimum_console_loglevel (console_printk[2])
#define default_console_loglevel (console_printk[3])

static inline void console_silent(void)
{
	console_loglevel = CONSOLE_LOGLEVEL_SILENT;
}

static inline void console_verbose(void)
{
	if (console_loglevel)
		console_loglevel = CONSOLE_LOGLEVEL_MOTORMOUTH;
}

struct va_format {
	const char *fmt;
	va_list *va;
};

/*
 * FW_BUG
 * Add this to a message where you are sure the firmware is buggy or behaves
 * really stupid or out of spec. Be aware that the responsible BIOS developer
 * should be able to fix this issue or at least get a concrete idea of the
 * problem by reading your message without the need of looking at the kernel
 * code.
 *
 * Use it for definite and high priority BIOS bugs.
 *
 * FW_WARN
 * Use it for not that clear (e.g. could the kernel messed up things already?)
 * and medium priority BIOS bugs.
 *
 * FW_INFO
 * Use this one if you want to tell the user or vendor about something
 * suspicious, but generally harmless related to the firmware.
 *
 * Use it for information or very low priority BIOS bugs.
 */
#define FW_BUG		"[Firmware Bug]: "
#define FW_WARN		"[Firmware Warn]: "
#define FW_INFO		"[Firmware Info]: "

/*
 * HW_ERR
 * Add this to a message for hardware errors, so that user can report
 * it to hardware vendor instead of LKML or software vendor.
 */
#define HW_ERR		"[Hardware Error]: "

/*
 * DEPRECATED
 * Add this to a message whenever you want to warn user space about the use
 * of a deprecated aspect of an API so they can stop using it
 */
#define DEPRECATED	"[Deprecated]: "

/*
 * Dummy printk for disabled debugging statements to use whilst maintaining
 * gcc's format checking.
 */
#define no_printk(fmt, ...)				\
({							\
	if (0)						\
		printk(fmt, ##__VA_ARGS__);		\
	0;						\
})

#ifdef CONFIG_PRINTK
asmlinkage __printf(5, 0)
int vprintk_emit(int facility, int level,
		 const char *dict, size_t dictlen,
		 const char *fmt, va_list args);

asmlinkage __printf(1, 0)
int vprintk(const char *fmt, va_list args);

asmlinkage __printf(5, 6) __cold
int printk_emit(int facility, int level,
		const char *dict, size_t dictlen,
		const char *fmt, ...);

asmlinkage __printf(1, 2) __cold
int printk(const char *fmt, ...);

__printf(1, 2) void dump_stack_set_arch_desc(const char *fmt, ...);

extern asmlinkage void dump_stack(void) __cold;
#else
static inline __printf(1, 0)
int vprintk(const char *s, va_list args)
{
	return 0;
}

static inline __printf(1, 2) __cold
int printk(const char *s, ...)
{
	return 0;
}

static inline __printf(1, 2) void dump_stack_set_arch_desc(const char *fmt, ...)
{
}

static inline void dump_stack(void)
{
}

#endif

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

/*
 * These can be used to print at the various log levels.
 * All of these will print unconditionally, although note that pr_debug()
 * and other debug macros are compiled out unless either DEBUG is defined
 * or CONFIG_DYNAMIC_DEBUG is set.
 */
#define pr_emerg(fmt, ...) \
	printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...) \
	printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...) \
	printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...) \
	printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...) \
	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...) \
	printk(KERN_CONT fmt, ##__VA_ARGS__)

/* pr_devel() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define pr_devel(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_devel(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

#if defined(DEBUG)
#define pr_debug(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

/*
 * Print a one-time message (analogous to WARN_ONCE() et al):
 */

#ifdef CONFIG_PRINTK
#define printk_once(fmt, ...)					\
({								\
	static bool __print_once __read_mostly;			\
								\
	if (!__print_once) {					\
		__print_once = true;				\
		printk(fmt, ##__VA_ARGS__);			\
	}							\
})
#else
#define printk_once(fmt, ...)					\
	no_printk(fmt, ##__VA_ARGS__)
#endif

#define pr_emerg_once(fmt, ...)					\
	printk_once(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert_once(fmt, ...)					\
	printk_once(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit_once(fmt, ...)					\
	printk_once(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err_once(fmt, ...)					\
	printk_once(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn_once(fmt, ...)					\
	printk_once(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice_once(fmt, ...)				\
	printk_once(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info_once(fmt, ...)					\
	printk_once(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont_once(fmt, ...)					\
	printk_once(KERN_CONT pr_fmt(fmt), ##__VA_ARGS__)

#if defined(DEBUG)
#define pr_devel_once(fmt, ...)					\
	printk_once(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_devel_once(fmt, ...)					\
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

/* If you are writing a driver, please use dev_dbg instead */
#if defined(DEBUG)
#define pr_debug_once(fmt, ...)					\
	printk_once(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_debug_once(fmt, ...)					\
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};
extern void hex_dump_to_buffer(const void *buf, size_t len,
			       int rowsize, int groupsize,
			       char *linebuf, size_t linebuflen, bool ascii);
#ifdef CONFIG_PRINTK
extern void print_hex_dump(const char *level, const char *prefix_str,
			   int prefix_type, int rowsize, int groupsize,
			   const void *buf, size_t len, bool ascii);
extern void print_hex_dump_bytes(const char *prefix_str, int prefix_type,
				 const void *buf, size_t len);
#else
static inline void print_hex_dump(const char *level, const char *prefix_str,
				  int prefix_type, int rowsize, int groupsize,
				  const void *buf, size_t len, bool ascii)
{
}
static inline void print_hex_dump_bytes(const char *prefix_str, int prefix_type,
					const void *buf, size_t len)
{
}
#endif

#define print_hex_dump_debug(prefix_str, prefix_type, rowsize,		\
			     groupsize, buf, len, ascii)		\
	print_hex_dump(KERN_DEBUG, prefix_str, prefix_type, rowsize,	\
		       groupsize, buf, len, ascii)

#endif
