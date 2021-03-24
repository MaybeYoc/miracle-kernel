/* CPU control.
 * (C) 2001, 2002, 2003, 2004 Rusty Russell
 *
 * This code is licenced under the GPL.
 */
#include <linux/smp.h>
#include <linux/init.h>

struct cpumask __cpu_possible_mask __read_mostly;
struct cpumask __cpu_online_mask __read_mostly;
struct cpumask __cpu_present_mask __read_mostly;
struct cpumask __cpu_active_mask __read_mostly;
