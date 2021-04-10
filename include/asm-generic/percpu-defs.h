/*
 * linux/percpu-defs.h - basic definitions for percpu areas
 *
 * DO NOT INCLUDE DIRECTLY OUTSIDE PERCPU IMPLEMENTATION PROPER.
 *
 * This file is separate from linux/percpu.h to avoid cyclic inclusion
 * dependency from arch header files.  Only to be included from
 * asm/percpu.h.
 *
 * This file includes macros necessary to declare percpu sections and
 * variables, and definitions of percpu accessors and operations.  It
 * should provide enough percpu features to arch header files even when
 * they can only include asm/percpu.h to avoid cyclic inclusion dependency.
 */

#ifndef __ASM_GENERIC_PERCPU_DEFS_H_
#define __ASM_GENERIC_PERCPU_DEFS_H_

#ifndef __ASM_PERCPU_H_
#error "please don't include this file directly"
#endif


#ifndef PER_CPU_BASE_SECTION
#ifdef CONFIG_SMP
#define PER_CPU_BASE_SECTION ".data..percpu"
#else
#define PER_CPU_BASE_SECTION ".data"
#endif
#endif

#ifndef PER_CPU_ATTRIBUTES
#define PER_CPU_ATTRIBUTES
#endif

#ifdef CONFIG_SMP
#define PER_CPU_SHARED_ALIGNED_SECTION "..shared_aligned"
#define PER_CPU_ALIGNED_SECTION "..shared_aligned"
#define PER_CPU_FIRST_SECTION "..first"
#else
#define PER_CPU_SHARED_ALIGNED_SECTION ""
#define PER_CPU_ALIGNED_SECTION "..shared_aligned"
#define PER_CPU_FIRST_SECTION ""
#endif

#define __PCPU_ATTRS(sec)						\
	__percpu __attribute__((section(PER_CPU_BASE_SECTION sec)))	\
	PER_CPU_ATTRIBUTES


#define DECLARE_PER_CPU_SECTION(type, name, sec)			\
	extern __PCPU_ATTRS(sec) __typeof__(type) name
#define DEFINE_PER_CPU_SECTION(type, name, sec)				\
	__PCPU_ATTRS(sec) __typeof__(type) name


#define DECLARE_PER_CPU(type, name)					\
	DECLARE_PER_CPU_SECTION(type, name, "")
#define DEFINE_PER_CPU(type, name)					\
	DEFINE_PER_CPU_SECTION(type, name, "")

#define DECLARE_PER_CPU_FIRST(type, name)				\
	DECLARE_PER_CPU_SECTION(type, name, PER_CPU_FIRST_SECTION)
#define DEFINE_PER_CPU_FIRST(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, PER_CPU_FIRST_SECTION)

#define DECLARE_PER_CPU_SHARED_ALIGNED(type, name)			\
	DECLARE_PER_CPU_SECTION(type, name, PER_CPU_SHARED_ALIGNED_SECTION) \
	____cacheline_aligned_in_smp
#define DEFINE_PER_CPU_SHARED_ALIGNED(type, name)			\
	DEFINE_PER_CPU_SECTION(type, name, PER_CPU_SHARED_ALIGNED_SECTION) \
	____cacheline_aligned_in_smp

#define DECLARE_PER_CPU_ALIGNED(type, name)				\
	DECLARE_PER_CPU_SECTION(type, name, PER_CPU_ALIGNED_SECTION)	\
	____cacheline_aligned
#define DEFINE_PER_CPU_ALIGNED(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, PER_CPU_ALIGNED_SECTION)	\
	____cacheline_aligned

#define DECLARE_PER_CPU_PAGE_ALIGNED(type, name)			\
	DECLARE_PER_CPU_SECTION(type, name, "..page_aligned")		\
	__aligned(PAGE_SIZE)
#define DEFINE_PER_CPU_PAGE_ALIGNED(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, "..page_aligned")		\
	__aligned(PAGE_SIZE)

#define DECLARE_PER_CPU_READ_MOSTLY(type, name)			\
	DECLARE_PER_CPU_SECTION(type, name, "..read_mostly")
#define DEFINE_PER_CPU_READ_MOSTLY(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, "..read_mostly")

#ifndef __ASSEMBLY__

#define __verify_pcpu_ptr(ptr)						\
do {									\
	const void __percpu *__vpp_verify = (typeof((ptr) + 0))NULL;	\
	(void)__vpp_verify;						\
} while (0)

#ifdef CONFIG_SMP
#ifndef __per_cpu_offset
extern unsigned long __per_cpu_offset[NR_CPUS];
#define per_cpu_offset(x) (__per_cpu_offset[x])
#endif

#ifndef my_cpu_offset
#define my_cpu_offset __my_cpu_offset
#endif

#ifndef this_cpu_offset
#define this_cpu_offset per_cpu_offset(__my_cpu_offset)
#endif

#endif

#ifdef CONFIG_SMP

#define SHIFT_PERCPU_PTR(__p, __offset)					\
	RELOC_HIDE((typeof(*(__p)) __kernel __force *)(__p), (__offset))

#define per_cpu_ptr(ptr, cpu)						\
({									\
	__verify_pcpu_ptr(ptr);						\
	SHIFT_PERCPU_PTR((ptr), per_cpu_offset((cpu)));			\
})

#define this_cpu_ptr(ptr)						\
({									\
	__verify_pcpu_ptr(ptr);						\
	SHIFT_PERCPU_PTR(ptr, this_cpu_offset);		\
})

#else	/* CONFIG_SMP */

#define VERIFY_PERCPU_PTR(__p)						\
({									\
	__verify_pcpu_ptr(__p);						\
	(typeof(*(__p)) __kernel __force *)(__p);			\
})

#define per_cpu_ptr(ptr, cpu)	({ (void)(cpu); VERIFY_PERCPU_PTR(ptr); })
#define this_cpu_ptr(ptr)	per_cpu_ptr(ptr, 0)

#endif	/* CONFIG_SMP */

#define per_cpu(var, cpu)	(*per_cpu_ptr(&(var), cpu))

#define this_cpu_generic_to_op(pcp, val, op)		\
do {					\
	*this_cpu_ptr(&(pcp)) op val;						\
} while (0)

#define this_cpu_generic_add_return(pcp, val)				\
({									\
	this_cpu_add(pcp, val);						\
	this_cpu_read(pcp);						\
})

#define this_cpu_generic_xchg(pcp, nval)					\
({									\
	typeof(pcp) __ret;						\
	__ret = this_cpu_read(pcp);					\
	this_cpu_write(pcp, nval);					\
	__ret;								\
})

#define this_cpu_generic_cmpxchg(pcp, oval, nval)			\
({									\
	typeof(pcp) __ret;						\
	__ret = this_cpu_read(pcp);					\
	if (__ret == (oval))						\
		this_cpu_write(pcp, nval);				\
	__ret;								\
})

#define this_cpu_generic_cmpxchg_double(pcp1, pcp2, oval1, oval2, nval1, nval2) \
({									\
	int __ret = 0;							\
	if (this_cpu_read(pcp1) == (oval1) &&				\
			 this_cpu_read(pcp2)  == (oval2)) {		\
		this_cpu_write(pcp1, nval1);				\
		this_cpu_write(pcp2, nval2);				\
		__ret = 1;						\
	}								\
	(__ret);							\
})

#ifndef this_cpu_read_1
#define this_cpu_read_1(pcp)		(*this_cpu_ptr(&(pcp)))
#endif
#ifndef this_cpu_read_2
#define this_cpu_read_2(pcp)		(*this_cpu_ptr(&(pcp)))
#endif
#ifndef this_cpu_read_4
#define this_cpu_read_4(pcp)		(*this_cpu_ptr(&(pcp)))
#endif
#ifndef this_cpu_read_8
#define this_cpu_read_8(pcp)		(*this_cpu_ptr(&(pcp)))
#endif

#ifndef this_cpu_write_1
#define this_cpu_write_1(pcp, val)	this_cpu_generic_to_op(pcp, val, =)
#endif
#ifndef this_cpu_write_2
#define this_cpu_write_2(pcp, val)	this_cpu_generic_to_op(pcp, val, =)
#endif
#ifndef this_cpu_write_4
#define this_cpu_write_4(pcp, val)	this_cpu_generic_to_op(pcp, val, =)
#endif
#ifndef this_cpu_write_8
#define this_cpu_write_8(pcp, val)	this_cpu_generic_to_op(pcp, val, =)
#endif

#ifndef this_cpu_add_1
#define this_cpu_add_1(pcp, val)		this_cpu_generic_to_op(pcp, val, +=)
#endif
#ifndef this_cpu_add_2
#define this_cpu_add_2(pcp, val)		this_cpu_generic_to_op(pcp, val, +=)
#endif
#ifndef this_cpu_add_4
#define this_cpu_add_4(pcp, val)		this_cpu_generic_to_op(pcp, val, +=)
#endif
#ifndef this_cpu_add_8
#define this_cpu_add_8(pcp, val)		this_cpu_generic_to_op(pcp, val, +=)
#endif

#ifndef this_cpu_add_return_1
#define this_cpu_add_return_1(pcp, val)	this_cpu_generic_add_return(pcp, val)
#endif
#ifndef this_cpu_add_return_2
#define this_cpu_add_return_2(pcp, val)	this_cpu_generic_add_return(pcp, val)
#endif
#ifndef this_cpu_add_return_4
#define this_cpu_add_return_4(pcp, val)	this_cpu_generic_add_return(pcp, val)
#endif
#ifndef this_cpu_add_return_8
#define this_cpu_add_return_8(pcp, val)	this_cpu_generic_add_return(pcp, val)
#endif

#ifndef this_cpu_xchg_1
#define this_cpu_xchg_1(pcp, nval)	this_cpu_generic_xchg(pcp, nval)
#endif
#ifndef this_cpu_xchg_2
#define this_cpu_xchg_2(pcp, nval)	this_cpu_generic_xchg(pcp, nval)
#endif
#ifndef this_cpu_xchg_4
#define this_cpu_xchg_4(pcp, nval)	this_cpu_generic_xchg(pcp, nval)
#endif
#ifndef this_cpu_xchg_8
#define this_cpu_xchg_8(pcp, nval)	this_cpu_generic_xchg(pcp, nval)
#endif

#ifndef this_cpu_cmpxchg_1
#define this_cpu_cmpxchg_1(pcp, oval, nval) \
	this_cpu_generic_cmpxchg(pcp, oval, nval)
#endif
#ifndef this_cpu_cmpxchg_2
#define this_cpu_cmpxchg_2(pcp, oval, nval) \
	this_cpu_generic_cmpxchg(pcp, oval, nval)
#endif
#ifndef this_cpu_cmpxchg_4
#define this_cpu_cmpxchg_4(pcp, oval, nval) \
	this_cpu_generic_cmpxchg(pcp, oval, nval)
#endif
#ifndef this_cpu_cmpxchg_8
#define this_cpu_cmpxchg_8(pcp, oval, nval) \
	this_cpu_generic_cmpxchg(pcp, oval, nval)
#endif

#ifndef this_cpu_cmpxchg_double_1
#define this_cpu_cmpxchg_double_1(pcp1, pcp2, oval1, oval2, nval1, nval2) \
	this_cpu_generic_cmpxchg_double(pcp1, pcp2, oval1, oval2, nval1, nval2)
#endif
#ifndef this_cpu_cmpxchg_double_2
#define this_cpu_cmpxchg_double_2(pcp1, pcp2, oval1, oval2, nval1, nval2) \
	this_cpu_generic_cmpxchg_double(pcp1, pcp2, oval1, oval2, nval1, nval2)
#endif
#ifndef this_cpu_cmpxchg_double_4
#define this_cpu_cmpxchg_double_4(pcp1, pcp2, oval1, oval2, nval1, nval2) \
	this_cpu_generic_cmpxchg_double(pcp1, pcp2, oval1, oval2, nval1, nval2)
#endif
#ifndef this_cpu_cmpxchg_double_8
#define this_cpu_cmpxchg_double_8(pcp1, pcp2, oval1, oval2, nval1, nval2) \
	this_cpu_generic_cmpxchg_double(pcp1, pcp2, oval1, oval2, nval1, nval2)
#endif

#ifndef this_cpu_and_1
#define this_cpu_and_1(pcp, val)		this_cpu_generic_to_op(pcp, val, &=)
#endif
#ifndef this_cpu_and_2
#define this_cpu_and_2(pcp, val)		this_cpu_generic_to_op(pcp, val, &=)
#endif
#ifndef this_cpu_and_4
#define this_cpu_and_4(pcp, val)		this_cpu_generic_to_op(pcp, val, &=)
#endif
#ifndef this_cpu_and_8
#define this_cpu_and_8(pcp, val)		this_cpu_generic_to_op(pcp, val, &=)
#endif

#ifndef this_cpu_or_1
#define this_cpu_or_1(pcp, val)		this_cpu_generic_to_op(pcp, val, |=)
#endif
#ifndef this_cpu_or_2
#define this_cpu_or_2(pcp, val)		this_cpu_generic_to_op(pcp, val, |=)
#endif
#ifndef this_cpu_or_4
#define this_cpu_or_4(pcp, val)		this_cpu_generic_to_op(pcp, val, |=)
#endif
#ifndef this_cpu_or_8
#define this_cpu_or_8(pcp, val)		this_cpu_generic_to_op(pcp, val, |=)
#endif

/*
 * Branching function to split up a function into a set of functions that
 * are called for different scalar sizes of the objects handled.
 */

extern void __bad_size_call_parameter(void);

#define __pcpu_size_call_return(stem, variable)				\
({									\
	typeof(variable) pscr_ret__;					\
	__verify_pcpu_ptr(&(variable));					\
	switch(sizeof(variable)) {					\
	case 1: pscr_ret__ = stem##1(variable); break;			\
	case 2: pscr_ret__ = stem##2(variable); break;			\
	case 4: pscr_ret__ = stem##4(variable); break;			\
	case 8: pscr_ret__ = stem##8(variable); break;			\
	default:							\
		__bad_size_call_parameter(); break;			\
	}								\
	pscr_ret__;							\
})

#define __pcpu_size_call_return2(stem, variable, ...)			\
({									\
	typeof(variable) pscr2_ret__;					\
	__verify_pcpu_ptr(&(variable));					\
	switch(sizeof(variable)) {					\
	case 1: pscr2_ret__ = stem##1(variable, __VA_ARGS__); break;	\
	case 2: pscr2_ret__ = stem##2(variable, __VA_ARGS__); break;	\
	case 4: pscr2_ret__ = stem##4(variable, __VA_ARGS__); break;	\
	case 8: pscr2_ret__ = stem##8(variable, __VA_ARGS__); break;	\
	default:							\
		__bad_size_call_parameter(); break;			\
	}								\
	pscr2_ret__;							\
})

#define __pcpu_double_call_return_bool(stem, pcp1, pcp2, ...)		\
({									\
	bool pdcrb_ret__;						\
	__verify_pcpu_ptr(&(pcp1));					\
	BUILD_BUG_ON(sizeof(pcp1) != sizeof(pcp2));			\
	VM_BUG_ON((unsigned long)(&(pcp1)) % (2 * sizeof(pcp1)));	\
	VM_BUG_ON((unsigned long)(&(pcp2)) !=				\
		  (unsigned long)(&(pcp1)) + sizeof(pcp1));		\
	switch(sizeof(pcp1)) {						\
	case 1: pdcrb_ret__ = stem##1(pcp1, pcp2, __VA_ARGS__); break;	\
	case 2: pdcrb_ret__ = stem##2(pcp1, pcp2, __VA_ARGS__); break;	\
	case 4: pdcrb_ret__ = stem##4(pcp1, pcp2, __VA_ARGS__); break;	\
	case 8: pdcrb_ret__ = stem##8(pcp1, pcp2, __VA_ARGS__); break;	\
	default:							\
		__bad_size_call_parameter(); break;			\
	}								\
	pdcrb_ret__;							\
})

#define __pcpu_size_call(stem, variable, ...)				\
do {									\
	__verify_pcpu_ptr(&(variable));					\
	switch(sizeof(variable)) {					\
		case 1: stem##1(variable, __VA_ARGS__);break;		\
		case 2: stem##2(variable, __VA_ARGS__);break;		\
		case 4: stem##4(variable, __VA_ARGS__);break;		\
		case 8: stem##8(variable, __VA_ARGS__);break;		\
		default: 						\
			__bad_size_call_parameter();break;		\
	}								\
} while (0)

#define this_cpu_read(pcp)		__pcpu_size_call_return(this_cpu_read_, pcp)
#define this_cpu_write(pcp, val)		__pcpu_size_call(this_cpu_write_, pcp, val)
#define this_cpu_add(pcp, val)		__pcpu_size_call(this_cpu_add_, pcp, val)
#define this_cpu_and(pcp, val)		__pcpu_size_call(this_cpu_and_, pcp, val)
#define this_cpu_or(pcp, val)		__pcpu_size_call(this_cpu_or_, pcp, val)
#define this_cpu_add_return(pcp, val)	__pcpu_size_call_return2(this_cpu_add_return_, pcp, val)
#define this_cpu_xchg(pcp, nval)		__pcpu_size_call_return2(this_cpu_xchg_, pcp, nval)
#define this_cpu_cmpxchg(pcp, oval, nval) \
	__pcpu_size_call_return2(this_cpu_cmpxchg_, pcp, oval, nval)
#define this_cpu_cmpxchg_double(pcp1, pcp2, oval1, oval2, nval1, nval2) \
	__pcpu_double_call_return_bool(this_cpu_cmpxchg_double_, pcp1, pcp2, oval1, oval2, nval1, nval2)

#define this_cpu_sub(pcp, val)		this_cpu_add(pcp, -(val))
#define this_cpu_inc(pcp)		this_cpu_add(pcp, 1)
#define this_cpu_dec(pcp)		this_cpu_sub(pcp, 1)
#define this_cpu_sub_return(pcp, val)	this_cpu_add_return(pcp, -(typeof(pcp))(val))
#define this_cpu_inc_return(pcp)		this_cpu_add_return(pcp, 1)
#define this_cpu_dec_return(pcp)		this_cpu_add_return(pcp, -1)

#endif /* !__ASSEMBLY__ */
#endif /* __ASM_GENERIC_PERCPU_DEFS_H_ */
