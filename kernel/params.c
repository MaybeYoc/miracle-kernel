/* Helpers for initial module or kernel cmdline parsing
   Copyright (C) 2001 Rusty Russell.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/irqflags.h>

static char dash2underscore(char c)
{
	if (c == '-')
		return '_';
	return c;
}

bool parameqn(const char *a, const char *b, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		if (dash2underscore(a[i]) != dash2underscore(b[i]))
			return false;
	}
	return true;
}

bool parameq(const char *a, const char *b)
{
	return parameqn(a, b, strlen(a)+1);
}

static void param_check_unsafe(const struct kernel_param *kp)
{
	if (kp->flags & KERNEL_PARAM_FL_UNSAFE) {
		pr_notice("Setting dangerous option %s - tainting kernel\n",
			  kp->name);
		//add_taint(TAINT_USER, LOCKDEP_STILL_OK); /* TODO */
	}
}

static int parse_one(char *param,
		     char *val,
		     const char *doing,
		     const struct kernel_param *params,
		     unsigned num_params,
		     s16 min_level,
		     s16 max_level,
		     void *arg,
		     int (*handle_unknown)(char *param, char *val,
				     const char *doing, void *arg))
{
	unsigned int i;
	int err;

	/* Find parameter */
	for (i = 0; i < num_params; i++) {
		if (parameq(param, params[i].name)) {
			if (params[i].level < min_level
			    || params[i].level > max_level)
				return 0;
			/* No one handled NULL, so do it here. */
			if (!val &&
			    !(params[i].ops->flags & KERNEL_PARAM_OPS_FL_NOARG))
				return -EINVAL;
			pr_debug("handling %s with %p\n", param,
				params[i].ops->set);
			kernel_param_lock(params[i].mod);
			param_check_unsafe(&params[i]);
			err = params[i].ops->set(val, &params[i]);
			kernel_param_unlock(params[i].mod);
			return err;
		}
	}

	if (handle_unknown) {
		pr_debug("doing %s: %s='%s'\n", doing, param, val);
		return handle_unknown(param, val, doing, arg);
	}

	pr_debug("Unknown argument '%s'\n", param);
	return -ENOENT;
}

/* Args looks like "foo=bar,bar2 baz=fuz wiz". */
char *parse_args(const char *doing,
		 char *args,
		 const struct kernel_param *params,
		 unsigned num,
		 s16 min_level,
		 s16 max_level,
		 void *arg,
		 int (*unknown)(char *param, char *val,
				const char *doing, void *arg))
{
	char *param, *val, *err = NULL;

	/* Chew leading spaces */
	args = skip_spaces(args);

	if (*args)
		pr_debug("doing %s, parsing ARGS: '%s'\n", doing, args);

	while (*args) {
		int ret;
		int irq_was_disabled;

		args = next_arg(args, &param, &val);
		/* Stop at -- */
		if (!val && strcmp(param, "--") == 0)
			return err ?: args;
		irq_was_disabled = irqs_disabled();
		ret = parse_one(param, val, doing, params, num,
				min_level, max_level, arg, unknown);
		if (irq_was_disabled && !irqs_disabled())
			pr_warn("%s: option '%s' enabled irq's!\n",
				doing, param);

		switch (ret) {
		case 0:
			continue;
		case -ENOENT:
			pr_err("%s: Unknown parameter `%s'\n", doing, param);
			break;
		case -ENOSPC:
			pr_err("%s: `%s' too large for parameter `%s'\n",
			       doing, val ?: "", param);
			break;
		default:
			pr_err("%s: `%s' invalid for parameter `%s'\n",
			       doing, val ?: "", param);
			break;
		}

		err = ERR_PTR(ret);
	}

	return err;
}
