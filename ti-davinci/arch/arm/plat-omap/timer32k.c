/*
 * linux/arch/arm/plat-omap/timer32k.c
 *
 * OMAP 32K Timer
 *
 * Copyright (C) 2004 - 2005 Nokia Corporation
 * Partial timer rewrite and additional dynamic tick timer support by
 * Tony Lindgen <tony@atomide.com> and
 * Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 * OMAP Dual-mode timer framework support by Timo Teras
 * Clocksource infrastructure added by Daniel Walker <dwalker@mvista.com>
 *
 * MPU timer code based on the older MPU timer code for OMAP
 * Copyright (C) 2000 RidgeRun, Inc.
 * Author: Greg Lonnon <glonnon@ridgerun.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/leds.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>
#include <asm/arch/dmtimer.h>

struct sys_timer omap_timer;

/*
 * ---------------------------------------------------------------------------
 * 32KHz OS timer
 *
 * This currently works only on 16xx, as 1510 does not have the continuous
 * 32KHz synchronous timer. The 32KHz synchronous timer is used to keep track
 * of time in addition to the 32KHz OS timer. Using only the 32KHz OS timer
 * on 1510 would be possible, but the timer would not be as accurate as
 * with the 32KHz synchronized timer.
 * ---------------------------------------------------------------------------
 */

#if defined(CONFIG_ARCH_OMAP16XX)
#define TIMER_32K_SYNCHRONIZED		0xfffbc410
#elif defined(CONFIG_ARCH_OMAP24XX)
#define TIMER_32K_SYNCHRONIZED		0x48004010
#else
#error OMAP 32KHz timer does not currently work on 15XX!
#endif

/* 16xx specific defines */
#define OMAP1_32K_TIMER_BASE		0xfffb9000
#define OMAP1_32K_TIMER_CR		0x08
#define OMAP1_32K_TIMER_TVR		0x00
#define OMAP1_32K_TIMER_TCR		0x04

#define OMAP_32K_TICKS_PER_HZ		(32768 / HZ)
#define OMAP_32K_TICKS_PER_SEC		(32768)

/*
 * TRM says 1 / HZ = ( TVR + 1) / 32768, so TRV = (32768 / HZ) - 1
 * so with HZ = 128, TVR = 255.
 */
#define OMAP_32K_TIMER_TICK_PERIOD	((32768 / HZ) - 1)

#define JIFFIES_TO_HW_TICKS(nr_jiffies, clock_rate)			\
				(((nr_jiffies) * (clock_rate)) / HZ)

#if defined(CONFIG_ARCH_OMAP1)

static inline void omap_32k_timer_write(int val, int reg)
{
	omap_writew(val, OMAP1_32K_TIMER_BASE + reg);
}

static inline unsigned long omap_32k_timer_read(int reg)
{
	return omap_readl(OMAP1_32K_TIMER_BASE + reg) & 0xffffff;
}

static inline void omap_32k_timer_start(unsigned long load_val)
{
	if (!load_val)
		load_val = 1;
	omap_32k_timer_write(load_val, OMAP1_32K_TIMER_TVR);
	omap_32k_timer_write(0x0f, OMAP1_32K_TIMER_CR);
}

static inline void omap_32k_timer_stop(void)
{
	omap_32k_timer_write(0x0, OMAP1_32K_TIMER_CR);
}

#define omap_32k_timer_ack_irq()

#elif defined(CONFIG_ARCH_OMAP2)

static struct omap_dm_timer *gptimer;

static inline void omap_32k_timer_start(unsigned long load_val)
{
	omap_dm_timer_set_load(gptimer, 1, 0xffffffff - load_val);
	omap_dm_timer_set_int_enable(gptimer, OMAP_TIMER_INT_OVERFLOW);
	omap_dm_timer_start(gptimer);
}

static inline void omap_32k_timer_stop(void)
{
	omap_dm_timer_stop(gptimer);
}

static inline void omap_32k_timer_ack_irq(void)
{
	u32 status = omap_dm_timer_read_status(gptimer);
	omap_dm_timer_write_status(gptimer, status);
}

#endif

/*
 * The 32KHz synchronized timer is an additional timer on 16xx.
 * It is always running.
 */
static inline unsigned long omap_32k_sync_timer_read(void)
{
	return omap_readl(TIMER_32K_SYNCHRONIZED);
}

/*
 * Rounds down to nearest usec. Note that this will overflow for larger values.
 */
static inline unsigned long omap_32k_ticks_to_usecs(unsigned long ticks_32k)
{
	return (ticks_32k * 5*5*5*5*5*5) >> 9;
}

/*
 * Rounds down to nearest nsec.
 */
static inline unsigned long long
omap_32k_ticks_to_nsecs(unsigned long ticks_32k)
{
	return (unsigned long long) ticks_32k * 1000 * 5*5*5*5*5*5 >> 9;
}

static unsigned long omap_32k_last_tick = 0;

/*
 * Returns current time from boot in nsecs. It's OK for this to wrap
 * around for now, as it's just a relative time stamp.
 */
unsigned long long sched_clock(void)
{
	return omap_32k_ticks_to_nsecs(omap_32k_sync_timer_read());
}


static void omap_32k_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
	unsigned long flags;

	local_irq_save(flags);
	omap_32k_timer_stop();
	omap_32k_timer_start(cycles);
	local_irq_restore(flags);
}

static void omap_32k_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
	static int periodic_requests = 0;

	switch (mode) {
	case CLOCK_EVT_ONESHOT:
		/* 32k timer does not have one-shot support in hardware.
		 * instead, wet just to a stop in the next_event hook,
		 * and dont support PERIODIC */
		break;
	case CLOCK_EVT_PERIODIC:
		if (periodic_requests)
			printk(KERN_ERR "32k-timer : CLOCK_EVT_PERIODIC "
			       "is not supported.\n");
		periodic_requests++;
		break;
	case CLOCK_EVT_SHUTDOWN:
		omap_32k_timer_stop();
		break;
	}
}

static struct clock_event_device clockevent_32k = {
	.name		= "32k-timer",
	.capabilities	= CLOCK_CAP_NEXTEVT | CLOCK_CAP_TICK |
			  CLOCK_CAP_UPDATE,
	.shift		= 32,
	.set_next_event	= omap_32k_set_next_event,
	.set_mode	= omap_32k_set_mode,
	.event_handler	= NULL,
};

static cycle_t omap_32k_read(void)
{
	return (cycle_t)omap_32k_sync_timer_read();
}

static struct clocksource clocksource_32k = {
	.name		= "32k-timer",
	.rating		= 250,
	.read		= omap_32k_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.shift		= 10,
	.is_continuous	= 1,
};

/*
 * Timer interrupt for 32KHz timer. When dynamic tick is enabled, this
 * function is also called from other interrupts to remove latency
 * issues with dynamic tick. In the dynamic tick case, we need to lock
 * with irqsave.
 */
static inline irqreturn_t _omap_32k_timer_interrupt(int irq, void *dev_id,
					struct pt_regs *regs)
{
	unsigned long now;

	omap_32k_timer_ack_irq();
	now = omap_32k_sync_timer_read();

	while ((signed long)(now - omap_32k_last_tick)
						>= OMAP_32K_TICKS_PER_HZ) {
		omap_32k_last_tick += OMAP_32K_TICKS_PER_HZ;
		timer_tick(regs);
	}

	/* Restart timer so we don't drift off due to modulo or dynamic tick.
	 * By default we program the next timer to be continuous to avoid
	 * latencies during high system load. During dynamic tick operation the
	 * continuous timer can be overridden from pm_idle to be longer.
	 */
	omap_32k_timer_start(omap_32k_last_tick + OMAP_32K_TICKS_PER_HZ - now);

	return IRQ_HANDLED;
}

static irqreturn_t omap_32k_timer_handler(int irq, void *dev_id,
					struct pt_regs *regs)
{
	return _omap_32k_timer_interrupt(irq, dev_id, regs);
}

static irqreturn_t omap_32k_timer_interrupt(int irq, void *dev_id,
					    struct pt_regs *regs)
{
	unsigned long flags;

	write_seqlock_irqsave(&xtime_lock, flags);
	_omap_32k_timer_interrupt(irq, dev_id, regs);
	write_sequnlock_irqrestore(&xtime_lock, flags);

	return IRQ_HANDLED;
}

#ifdef CONFIG_NO_IDLE_HZ
/*
 * Programs the next timer interrupt needed. Called when dynamic tick is
 * enabled, and to reprogram the ticks to skip from pm_idle. Note that
 * we can keep the timer continuous, and don't need to set it to run in
 * one-shot mode. This is because the timer will get reprogrammed again
 * after next interrupt.
 */
void omap_32k_timer_reprogram(unsigned long next_tick)
{
	unsigned long ticks = JIFFIES_TO_HW_TICKS(next_tick, 32768) + 1;
	unsigned long now = omap_32k_sync_timer_read();
	unsigned long idled = now - omap_32k_last_tick;

	if (idled + 1 < ticks)
		ticks -= idled;
	else
		ticks = 1;
	omap_32k_timer_start(ticks);
}

static struct irqaction omap_32k_timer_irq;
extern struct timer_update_handler timer_update;

static int omap_32k_timer_enable_dyn_tick(void)
{
	/* No need to reprogram timer, just use the next interrupt */
	return 0;
}

static int omap_32k_timer_disable_dyn_tick(void)
{
	omap_32k_timer_start(OMAP_32K_TIMER_TICK_PERIOD);
	return 0;
}

static struct dyn_tick_timer omap_dyn_tick_timer = {
	.enable		= omap_32k_timer_enable_dyn_tick,
	.disable	= omap_32k_timer_disable_dyn_tick,
	.reprogram	= omap_32k_timer_reprogram,
	.handler	= omap_32k_timer_handler,
};
#endif	/* CONFIG_NO_IDLE_HZ */

static struct irqaction omap_32k_timer_irq = {
	.name		= "32KHz timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= omap_32k_timer_interrupt,
};

static __init void omap_init_32k_timer(void)
{
	if (cpu_class_is_omap1())
		setup_irq(INT_OS_TIMER, &omap_32k_timer_irq);
	omap_32k_last_tick = omap_32k_sync_timer_read();

#ifdef CONFIG_ARCH_OMAP2
	/* REVISIT: Check 24xx TIOCP_CFG settings after idle works */
	if (cpu_is_omap24xx()) {
		gptimer = omap_dm_timer_request_specific(1);
		BUG_ON(gptimer == NULL);

		omap_dm_timer_set_source(gptimer, OMAP_TIMER_SRC_32_KHZ);
		setup_irq(omap_dm_timer_get_irq(gptimer), &omap_32k_timer_irq);
		omap_dm_timer_set_int_enable(gptimer,
			OMAP_TIMER_INT_CAPTURE | OMAP_TIMER_INT_OVERFLOW |
			OMAP_TIMER_INT_MATCH);
	}
#endif

	omap_32k_timer_start(OMAP_32K_TIMER_TICK_PERIOD);

	clocksource_32k.mult =
		clocksource_hz2mult(OMAP_32K_TICKS_PER_SEC,
				    clocksource_32k.shift);

	if (clocksource_register(&clocksource_32k))
		printk("omap_init_32k_timer : Error registering"
		       " 32k timer clocksource!\n");

	clockevent_32k.mult = div_sc(OMAP_32K_TICKS_PER_SEC, NSEC_PER_SEC,
				     clockevent_32k.shift);
	clockevent_32k.max_delta_ns =
		clockevent_delta2ns(0xfffffffe, &clockevent_32k);
	clockevent_32k.min_delta_ns =
		clockevent_delta2ns(1, &clockevent_32k);

	register_global_clockevent(&clockevent_32k);
}

/*
 * ---------------------------------------------------------------------------
 * Timer initialization
 * ---------------------------------------------------------------------------
 */
static void __init omap_timer_init(void)
{
#ifdef CONFIG_OMAP_DM_TIMER
	omap_dm_timer_init();
#endif
	omap_init_32k_timer();
}

struct sys_timer omap_timer = {
	.init		= omap_timer_init,
};
