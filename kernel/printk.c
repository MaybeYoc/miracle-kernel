#include <stdarg.h>
#include <linux/compiler.h>
#include <linux/printk.h>

int console_printk[4] = {
	CONSOLE_LOGLEVEL_DEFAULT,	/* console_loglevel */
	MESSAGE_LOGLEVEL_DEFAULT,	/* default_message_loglevel */
	CONSOLE_LOGLEVEL_MIN,		/* minimum_console_loglevel */
	CONSOLE_LOGLEVEL_DEFAULT,	/* default_console_loglevel */
};

asmlinkage __printf(1, 2) __cold
int printk(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = 1;
	va_end(args);

	return r;
}
