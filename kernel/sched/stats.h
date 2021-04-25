
# define   schedstat_enabled()		0
# define __schedstat_inc(var)		do { } while (0)
# define   schedstat_inc(var)		do { } while (0)
# define __schedstat_add(var, amt)	do { } while (0)
# define   schedstat_add(var, amt)	do { } while (0)
# define __schedstat_set(var, val)	do { } while (0)
# define   schedstat_set(var, val)	do { } while (0)
# define   schedstat_val(var)		0
# define   schedstat_val_or_zero(var)	0

# define sched_info_queued(rq, t)	do { } while (0)
# define sched_info_reset_dequeued(t)	do { } while (0)
# define sched_info_dequeued(rq, t)	do { } while (0)
# define sched_info_depart(rq, t)	do { } while (0)
# define sched_info_arrive(rq, next)	do { } while (0)
# define sched_info_switch(rq, t, next)	do { } while (0)

struct rq;
static inline void psi_enqueue(struct task_struct *p, bool wakeup) {}
static inline void psi_dequeue(struct task_struct *p, bool sleep) {}
static inline void psi_ttwu_dequeue(struct task_struct *p) {}
static inline void psi_task_tick(struct rq *rq) {}
