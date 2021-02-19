/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MODULE_PARAMS_H
#define _LINUX_MODULE_PARAMS_H
/* (C) Copyright 2001, 2002 Rusty Russell IBM Corporation */
#include <linux/init.h>
#include <linux/stringify.h>
#include <linux/kernel.h>

/* You can override this manually, but generally this should match the
   module name. */
#define MODULE_PARAM_PREFIX KBUILD_MODNAME "."

/* Chosen so that structs with an unsigned long line up. */
#define MAX_PARAM_PREFIX_LEN (64 - sizeof(unsigned long))

/* This struct is here for syntactic coherency, it is not used */
#define __MODULE_INFO(tag, name, info)					  \
  struct __UNIQUE_ID(name) {}

#define __MODULE_PARM_TYPE(name, _type)					  \
  __MODULE_INFO(parmtype, name##type, #name ":" _type)

/* One for each parameter, describing how to use it.  Some files do
   multiple of these per line, so can't just use MODULE_INFO. */
#define MODULE_PARM_DESC(_parm, desc) \
	__MODULE_INFO(parm, _parm, #_parm ":" desc)

struct kernel_param;

/*
 * Flags available for kernel_param_ops
 *
 * NOARG - the parameter allows for no argument (foo instead of foo=1)
 */
enum {
	KERNEL_PARAM_OPS_FL_NOARG = (1 << 0)
};

struct kernel_param_ops {
	/* How the ops should behave */
	unsigned int flags;
	/* Returns 0, or -errno.  arg is in kp->arg. */
	int (*set)(const char *val, const struct kernel_param *kp);
	/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
	int (*get)(char *buffer, const struct kernel_param *kp);
	/* Optional function to free kp->arg when module unloaded. */
	void (*free)(void *arg);
};

/*
 * Flags available for kernel_param
 *
 * UNSAFE - the parameter is dangerous and setting it will taint the kernel
 * HWPARAM - Hardware param not permitted in lockdown mode
 */
enum {
	KERNEL_PARAM_FL_UNSAFE	= (1 << 0),
	KERNEL_PARAM_FL_HWPARAM	= (1 << 1),
};

struct kernel_param {
	const char *name;
	struct module *mod;
	const struct kernel_param_ops *ops;
	const u16 perm;
	s8 level;
	u8 flags;
	union {
		void *arg;
		const struct kparam_string *str;
		const struct kparam_array *arr;
	};
};

extern const struct kernel_param __start___param[], __stop___param[];

/* Special one for strings we want to copy into */
struct kparam_string {
	unsigned int maxlen;
	char *string;
};

/* Special one for arrays */
struct kparam_array
{
	unsigned int max;
	unsigned int elemsize;
	unsigned int *num;
	const struct kernel_param_ops *ops;
	void *elem;
};

static inline void kernel_param_lock(struct module *mod)
{
}

static inline void kernel_param_unlock(struct module *mod)
{
}

/**
 * parameq - checks if two parameter names match
 * @name1: parameter name 1
 * @name2: parameter name 2
 *
 * Returns true if the two parameter names are equal.
 * Dashes (-) are considered equal to underscores (_).
 */
extern bool parameq(const char *name1, const char *name2);

/**
 * parameqn - checks if two parameter names match
 * @name1: parameter name 1
 * @name2: parameter name 2
 * @n: the length to compare
 *
 * Similar to parameq(), except it compares @n characters.
 */
extern bool parameqn(const char *name1, const char *name2, size_t n);

/* Called on module insert or kernel boot */
extern char *parse_args(const char *name,
		      char *args,
		      const struct kernel_param *params,
		      unsigned num,
		      s16 level_min,
		      s16 level_max,
		      void *arg,
		      int (*unknown)(char *param, char *val,
				     const char *doing, void *arg));

#endif /* _LINUX_MODULE_PARAMS_H */
