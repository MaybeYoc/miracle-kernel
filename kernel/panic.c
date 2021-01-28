
#include <linux/compiler.h>

/**
 *	panic - halt the system
 *	@fmt: The text string to print
 *
 *	Display a message, then perform cleanups.
 *
 *	This function never returns.
 */
void panic(const char *fmt, ...)
{
	
}

/*
 * Called when gcc's -fstack-protector feature is used, and
 * gcc detects corruption of the on-stack canary value
 */
__visible void __stack_chk_fail(void)
{
}

void warn_slowpath_fmt(const char *file, const int line,
		       const char *fmt, ...)
{

}

void warn_slowpath_fmt_taint(const char *file, const int line, unsigned taint,
			     const char *fmt, ...)
{

}

void warn_slowpath_null(const char *file, const int line)
{

}
