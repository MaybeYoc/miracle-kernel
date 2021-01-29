#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <linux/types.h>
#include <linux/llist.h>

struct path {
	struct dentry *dentry;
};

struct file {
	union {
		struct llist_node	fu_llist;
	} f_u;
	struct path		f_path;
};

#endif /* _LINUX_FS_H */
