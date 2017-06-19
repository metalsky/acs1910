/*
 * This code largely moved from arch/i386/kernel/timer/timer_tsc.c
 * which was originally moved from arch/i386/kernel/time.c.
 * See comments there for proper credits.
 */

#include <linux/clocksource.h>
#include <linux/workqueue.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/dmi.h>

#include <asm/vsyscall-gtod.h>
#include <asm/delay.h>
#include <asm/tsc.h>
#include <asm/delay.h>
#include <asm/io.h>

#include "mach_timer.h"

/*
 * On some systems the TSC frequency does not
 * change with the cpu frequency. So we need
 * an extra value to store the TSC freq
 */
unsigned int tsc_khz;

int tsc_disable __cpuinitdata = 0;

#ifdef CONFIG_X86_TSC
static int __init tsc_setup(char *str)
{
	printk(KERN_WARNING "notsc: Kernel compiled with CONFIG_X86_TSC, "
				"cannot disable TSC.\n");
	return 1;
}
#else
/*
 * disable flag for tsc. Takes effect by clearing the TSC cpu flag
 * in cpu/common.c
 */
static int __init tsc_setup(char *str)
{
	tsc_disable = 1;

	return 1;
}
#endif

__setup("notsc", tsc_setup);

/*
 * code to mark and check if the TSC is unstable
 * due to cpufreq or due to unsynced TSCs
 */
static int tsc_unstable;

static inline int check_tsc_unstable(void)
{
	return tsc_unstable;
}

void mark_tsc_unstable(void)
{
	tsc_unstable = 1;
}
EXPORT_SYMBOL_GPL(mark_tsc_unstable);

/* Accellerators for sched_clock()
 * convert from cycles(64bits) => nanoseconds (64bits)
 *  basic equation:
 *		ns = cycles / (freq / ns_per_sec)
 *		ns = cycles * (ns_per_sec / freq)
 *		ns = cycles * (10^9 / (cpu_khz * 10^3))
 *		ns = cycles * (10^6 / cpu_khz)
 *
 *	Then we use scaling math (suggested by george@mvista.com) to get:
 *		ns = cycles * (10^6 * SC / cpu_khz) / SC
 *		ns = cycles * cyc2ns_scale / SC
 *
 *	And since SC is a constant power of two, we can convert the div
 *  into a shift.
 *
 *  We can use khz divisor instead of mhz to keep a better percision, since
 *  cyc2ns_scale is limited to 10^6 * 2^10, which fits in 32 bits.
 *  (mathieu.desnoyers@polymtl.ca)
 *
 *			-johnstul@us.ibm.com "math is hard, lets go shopping!"
 */
static unsigned long cyc2ns_scale __read_mostly;

#define CYC2NS_SCALE_FACTOR 10 /* 2^10, carefully chosen */

static inline void set_cyc2ns_scale(unsigned long cpu_khz)
{
	cyc2ns_scale = (1000000 << CYC2NS_SCALE_FACTOR)/cpu_khz;
}

static inline unsigned long long cycles_2_ns(unsigned long long cyc)
{
	return (cyc * cyc2ns_scale) >> CYC2NS_SCALE_FACTOR;
}

/*
 * Scheduler clock - returns current time in nanosec units.
 */
unsigned long long sched_clock(void)
{
	unsigned long long this_offset;

	/*
	 * in the NUMA case we dont use the TSC as they are not
	 * synchronized across all CPUs.
	 */
#ifndef CONFIG_NUMA
	if (!cpu_khz || check_tsc_unstable())
#endif
		/* no locking but a rare wrong value is not a big deal */
		return (jiffies_64 - INITIAL_JIFFIES) * (1000000000 / HZ);

	/* read the Time Stamp Counter: */
	rdtscll(this_offset);

	/* return the value in ns */
	return cycles_2_ns(this_offset);
}

static unsigned long calculate_cpu_khz(void)
{
	unsigned long long start, end;
	unsigned long count;
	u64 delta64;
	int i;
	unsigned long flags;

	local_irq_save(flags);

	/* run 3 times to ensure the cache is warm */
	for (i = 0; i < 3; i++) {
		mach_prepare_counter();
		rdtscll(start);
		mach_countup(&count);
		rdtscll(end);
	}
	/*
	 * Error: ECTCNEVERSET
	 * The CTC wasn't reliable: we got a hit on the very first read,
	 * or the CPU was so fast/slow that the quotient wouldn't fit in
	 * 32 bits..
	 */
	if (count <= 1)
		goto err;

	delta64 = end - start;

	/* cpu freq too fast: */
	if (delta64 > (1ULL<<32))
		goto err;

	/* cpu freq too slow: */
	if (delta64 <= CALIBRATE_TIME_MSEC)
		goto err;

	delta64 += CALIBRATE_TIME_MSEC/2; /* round for do_div */
	do_div(delta64,CALIBRATE_TIME_MSEC);

	local_irq_restore(flags);
	return (unsigned long)delta64;
err:
	local_irq_restore(flags);
	return 0;
}

int recalibrate_cpu_khz(void)
{
#ifndef CONFIG_SMP
	unsigned long cpu_khz_old = cpu_khz;

	if (cpu_has_tsc) {
		cpu_khz = calculate_cpu_khz();
		tsc_khz = cpu_khz;
		cpu_data[0].loops_per_jiffy =
			cpufreq_scale(cpu_data[0].loops_per_jiffy,
					cpu_khz_old, cpu_khz);
		return 0;
	} else
		return -ENODEV;
#else
	return -ENODEV;
#endif
}

EXPORT_SYMBOL(recalibrate_cpu_khz);

void tsc_init(void)
{
	if (!cpu_has_tsc || tsc_disable)
		return;

	cpu_khz = calculate_cpu_khz();
	tsc_khz = cpu_khz;

	if (!cpu_khz)
		return;

	printk("Detected %lu.%03lu MHz processor.\n",
				(unsigned long)cpu_khz / 1000,
				(unsigned long)cpu_khz % 1000);

	set_cyc2ns_scale(cpu_khz);
	use_tsc_delay();
}

#ifdef CONFIG_CPU_FREQ

static unsigned int cpufreq_delayed_issched = 0;
static unsigned int cpufreq_init = 0;
static struct work_struct cpufreq_delayed_get_work;

static void handle_cpufreq_delayed_get(void *v)
{
	unsigned int cpu;

	for_each_online_cpu(cpu)
		cpufreq_get(cpu);

	cpufreq_delayed_issched = 0;
}

/*
 * if we notice cpufreq oddness, schedule a call to cpufreq_get() as it tries
 * to verify the CPU frequency the timing core thinks the CPU is running
 * at is still correct.
 */
static inline void cpufreq_delayed_get(void)
{
	if (cpufreq_init && !cpufreq_delayed_issched) {
		cpufreq_delayed_issched = 1;
		printk(KERN_DEBUG "Checking if CPU frequency changed.\n");
		schedule_work(&cpufreq_delayed_get_work);
	}
}

/*
 * if the CPU frequency is scaled, TSC-based delays will need a different
 * loops_per_jiffy value to function properly.
 */
static unsigned int ref_freq = 0;
static unsigned long loops_per_jiffy_ref = 0;
static unsigned long cpu_khz_ref = 0;

static int
time_cpufreq_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;

	if (val != CPUFREQ_RESUMECHANGE && val != CPUFREQ_SUSPENDCHANGE)
		write_seqlock_irq(&xtime_lock);

	if (!ref_freq) {
		if (!freq->old){
			ref_freq = freq->new;
			goto end;
		}
		ref_freq = freq->old;
		loops_per_jiffy_ref = cpu_data[freq->cpu].loops_per_jiffy;
		cpu_khz_ref = cpu_khz;
	}

	if ((val == CPUFREQ_PRECHANGE  && freq->old < freq->new) ||
	    (val == CPUFREQ_POSTCHANGE && freq->old > freq->new) ||
	    (val == CPUFREQ_RESUMECHANGE)) {
		if (!(freq->flags & CPUFREQ_CONST_LOOPS))
			cpu_data[freq->cpu].loops_per_jiffy =
				cpufreq_scale(loops_per_jiffy_ref,
						ref_freq, freq->new);

		if (cpu_khz) {

			if (num_online_cpus() == 1)
				cpu_khz = cpufreq_scale(cpu_khz_ref,
						ref_freq, freq->new);
			if (!(freq->flags & CPUFREQ_CONST_LOOPS)) {
				tsc_khz = cpu_khz;
				set_cyc2ns_scale(cpu_khz);
				/*
				 * TSC based sched_clock turns
				 * to junk w/ cpufreq
				 */
				mark_tsc_unstable();
			}
		}
	}
end:
	if (val != CPUFREQ_RESUMECHANGE && val != CPUFREQ_SUSPENDCHANGE)
		write_sequnlock_irq(&xtime_lock);

	return 0;
}

static struct notifier_block time_cpufreq_notifier_block = {
	.notifier_call	= time_cpufreq_notifier
};

static int __init cpufreq_tsc(void)
{
	int ret;

	INIT_WORK(&cpufreq_delayed_get_work, handle_cpufreq_delayed_get, NULL);
	ret = cpufreq_register_notifier(&time_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (!ret)
		cpufreq_init = 1;

	return ret;
}

core_initcall(cpufreq_tsc);

#endif

/* clock source code */

static unsigned long current_tsc_khz = 0;
static int tsc_update_callback(void);

static cycle_t read_tsc(void)
{
	cycle_t ret;

	rdtscll(ret);

	return ret;
}


static cycle_t __vsyscall_fn vread_tsc(void)
{
	cycle_t ret;

	rdtscll(ret);

	return ret;
}

static struct clocksource clocksource_tsc = {
	.name			= "tsc",
	.rating			= 300,
	.read			= read_tsc,
	.mask			= CLOCKSOURCE_MASK(64),
	.mult			= 0, /* to be set */
	.shift			= 22,
	.update_callback	= tsc_update_callback,
	.is_continuous		= 1,
	.vread			= vread_tsc,
};

static int tsc_update_callback(void)
{
	int change = 0;

	/* check to see if we should switch to the safe clocksource: */
	if (clocksource_tsc.rating != 50 && check_tsc_unstable()) {
		clocksource_tsc.rating = 50;
		clocksource_reselect();
		change = 1;
	}

	/* only update if tsc_khz has changed: */
	if (current_tsc_khz != tsc_khz) {
		current_tsc_khz = tsc_khz;
		clocksource_tsc.mult = clocksource_khz2mult(current_tsc_khz,
							clocksource_tsc.shift);
		change = 1;
	}

	return change;
}

static int __init dmi_mark_tsc_unstable(struct dmi_system_id *d)
{
	printk(KERN_NOTICE "%s detected: marking TSC unstable.\n",
		       d->ident);
	mark_tsc_unstable();
	return 0;
}

/* List of systems that have known TSC problems */
static struct dmi_system_id __initdata bad_tsc_dmi_table[] = {
	{
	 .callback = dmi_mark_tsc_unstable,
	 .ident = "IBM Thinkpad 380XD",
	 .matches = {
		     DMI_MATCH(DMI_BOARD_VENDOR, "IBM"),
		     DMI_MATCH(DMI_BOARD_NAME, "2635FA0"),
		     },
	 },
	 {}
};

#define TSC_FREQ_CHECK_INTERVAL (10*MSEC_PER_SEC) /* 10sec in MS */
static struct timer_list verify_tsc_freq_timer;

/* XXX - Probably should add locking */
static void verify_tsc_freq(unsigned long unused)
{
	static u64 last_tsc;
	static unsigned long last_jiffies;

	u64 now_tsc, interval_tsc;
	unsigned long now_jiffies, interval_jiffies;


	if (check_tsc_unstable())
		return;

	rdtscll(now_tsc);
	now_jiffies = jiffies;

	if (!last_jiffies) {
		goto out;
	}

	interval_jiffies = now_jiffies - last_jiffies;
	interval_tsc = now_tsc - last_tsc;
	interval_tsc *= HZ;
	do_div(interval_tsc, cpu_khz*1000);

	if (interval_tsc < (interval_jiffies * 3 / 4)) {
		printk("TSC appears to be running slowly. "
			"Marking it as unstable\n");
		mark_tsc_unstable();
		return;
	}

out:
	last_tsc = now_tsc;
	last_jiffies = now_jiffies;
	/* set us up to go off on the next interval: */
	mod_timer(&verify_tsc_freq_timer,
		jiffies + msecs_to_jiffies(TSC_FREQ_CHECK_INTERVAL));
}

/*
 * Make an educated guess if the TSC is trustworthy and synchronized
 * over all CPUs.
 */
static __init int unsynchronized_tsc(void)
{
	/*
	 * Intel systems are normally all synchronized.
	 * Exceptions must mark TSC as unstable:
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL)
 		return 0;

	/* assume multi socket systems are not synchronized: */
 	return num_possible_cpus() > 1;
}

static int __init init_tsc_clocksource(void)
{

	if (cpu_has_tsc && tsc_khz && !tsc_disable) {
		/* check blacklist */
		dmi_check_system(bad_tsc_dmi_table);

		if (unsynchronized_tsc()) /* mark unstable if unsynced */
			mark_tsc_unstable();
		current_tsc_khz = tsc_khz;
		clocksource_tsc.mult = clocksource_khz2mult(current_tsc_khz,
							clocksource_tsc.shift);
#ifndef CONFIG_HIGH_RES_TIMERS
		/* lower the rating if we already know its unstable: */
		if (check_tsc_unstable())
			clocksource_tsc.rating = 50;
#else
		/*
		 * Mark TSC unsuitable for high resolution timers. TSC has so
		 * many pitfalls: frequency changes, stop in idle ...  When we
		 * switch to high resolution mode we can not longer detect a
		 * firmware caused frequency change, as the emulated tick uses
		 * TSC as reference. This results in a circular dependency.
		 * Switch only to high resolution mode, if pm_timer or such
		 * is available.
		 */
		clocksource_tsc.rating = 50;
		clocksource_tsc.is_continuous = 0;
#endif
		init_timer(&verify_tsc_freq_timer);
		verify_tsc_freq_timer.function = verify_tsc_freq;
		verify_tsc_freq_timer.expires =
			jiffies + msecs_to_jiffies(TSC_FREQ_CHECK_INTERVAL);
		add_timer(&verify_tsc_freq_timer);

		return clocksource_register(&clocksource_tsc);
	}

	return 0;
}

module_init(init_tsc_clocksource);
