/*
 * MPC85xx setup and early boot code plus other random bits.
 *
 * Maintained by Kumar Gala (see MAINTAINERS for contact information)
 *
 * Copyright 2005 Freescale Semiconductor Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/config.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/reboot.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/initrd.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/fsl_devices.h>

#include <asm/of_device.h>
#include <asm/of_platform.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/atomic.h>
#include <asm/time.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/ipic.h>
#include <asm/bootinfo.h>
#include <asm/pci-bridge.h>
#include <asm/mpc85xx.h>
#include <asm/irq.h>
#include <mm/mmu_decl.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <asm/mpic.h>
#include <asm/i8259.h>

#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_pci.h>

static int cds_pci_slot = 2;
static volatile u8 *cadmus;

#ifdef CONFIG_PCI

static void mpc85xx_cds_restart(char *cmd)
{
	struct pci_dev *dev;
	u_char tmp;

	if ((dev = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C686,
					NULL))) {

		/* Use the VIA Super Southbridge to force a PCI reset */
		pci_read_config_byte(dev, 0x47, &tmp);
		pci_write_config_byte(dev, 0x47, tmp | 1);

		pci_dev_put(dev);
	}

	/*
	 *  We shouldn't get here, since the above should have done a full
	 *  reset, but just in case...
	 */
	fsl_rstcr_restart(NULL);
}

static int mpc85xx_exclude_device(struct pci_controller *hose,
				  u_char bus, u_char devfn)
{
	return PCIBIOS_SUCCESSFUL;
}

void __init
mpc85xx_cds_pci_irq_fixup(struct pci_dev *dev)
{
	u_char		c;
	if ((dev->vendor == PCI_VENDOR_ID_VIA) &&
			(dev->device == PCI_DEVICE_ID_VIA_82C586_1)) {

		/*
		 * U-Boot does not set the enable bits
		 * for the IDE device. Force them on here.
		 */
		pci_read_config_byte(dev, 0x40, &c);
		c |= 0x03; /* IDE: Chip Enable Bits */
		pci_write_config_byte(dev, 0x40, c);

		/*
		 * Since only primary interface works, force the
		 * IDE function to standard primary IDE interrupt
		 * w/ 8259 offset
		 */
		dev->irq = 14;
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}

	/*
	 * Force legacy USB interrupt routing
	 */
	if ((dev->vendor == PCI_VENDOR_ID_VIA) &&
			(dev->device == PCI_DEVICE_ID_VIA_82C586_2)) {
		if (PCI_FUNC(dev->devfn) == 2) {
			dev->irq = 10;
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 10);
		} else if (PCI_FUNC(dev->devfn) == 3) {
			dev->irq = 11;
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 11);
		}
	}
}

#ifdef CONFIG_PPC_I8259
static void mpc85xx_8259_cascade_handler(unsigned int irq,
	       				 struct irq_desc *desc,
					 struct pt_regs *regs)
{
	unsigned int cascade_irq = i8259_irq(regs);

	if (cascade_irq != NO_IRQ)
		/* handle an interrupt from the 8259 */
		generic_handle_irq(cascade_irq, regs);

	/* check for any interrupts from the shared IRQ line */
	handle_fasteoi_irq(irq, desc, regs);
}

static irqreturn_t mpc85xx_8259_cascade_action(int irq, void *dev_id,
					       struct pt_regs *regs)
{
	return IRQ_HANDLED;
}

static struct irqaction mpc85xxcds_8259_irqaction = {
	.handler = mpc85xx_8259_cascade_action,
	.flags = IRQF_SHARED,
	.mask = CPU_MASK_NONE,
	.name = "8259 cascade",
};
#endif /* PPC_I8259 */
#endif /* CONFIG_PCI */

static void __init mpc85xx_cds_pic_init(void)
{
	struct mpic *mpic;
	struct resource r;
	struct device_node *np = NULL;

	np = of_find_node_by_type(np, "open-pic");

	if (np == NULL) {
		printk(KERN_ERR "Could not find open-pic node\n");
		return;
	}

	if (of_address_to_resource(np, 0, &r)) {
		printk(KERN_ERR "Failed to map mpic register space\n");
		of_node_put(np);
		return;
	}

	mpic = mpic_alloc(np, r.start,
			MPIC_PRIMARY | MPIC_WANTS_RESET | MPIC_BIG_ENDIAN,
			0, 256, " OpenPIC  ");
	BUG_ON(mpic == NULL);

	/* Return the mpic node */
	of_node_put(np);

	mpic_init(mpic);
}

#if defined(CONFIG_PPC_I8259) && defined(CONFIG_PCI)
static int mpc85xx_cds_8259_attach(void)
{
	int ret;
	struct device_node *np = NULL;
	struct device_node *cascade_node = NULL;
	int cascade_irq;

	/* Initialize the i8259 controller */
	for_each_node_by_type(np, "interrupt-controller")
		if (device_is_compatible(np, "chrp,iic")) {
			cascade_node = np;
			break;
		}

	if (cascade_node == NULL) {
		printk(KERN_DEBUG "Could not find i8259 PIC\n");
		return -ENODEV;
	}

	cascade_irq = irq_of_parse_and_map(cascade_node, 0);
	if (cascade_irq == NO_IRQ) {
		printk(KERN_ERR "Failed to map cascade interrupt\n");
		return -ENXIO;
	}

	i8259_init(cascade_node, 0);
	of_node_put(cascade_node);

	/*
	 *  Hook the interrupt to make sure desc->action is never NULL.
	 *  This is required to ensure that the interrupt does not get
	 *  disabled when the last user of the shared IRQ line frees their
	 *  interrupt.
	 */
	if ((ret = setup_irq(cascade_irq, &mpc85xxcds_8259_irqaction))) {
		printk(KERN_ERR "Failed to setup cascade interrupt\n");
		return ret;
	}

	/* Success. Connect our low-level cascade handler. */
	set_irq_handler(cascade_irq, mpc85xx_8259_cascade_handler);

	return 0;
}

device_initcall(mpc85xx_cds_8259_attach);

#endif /* CONFIG_PPC_I8259 */

/*
 * Setup the architecture
 */
static void __init mpc85xx_cds_setup_arch(void)
{
	struct device_node *cpu;
#ifdef CONFIG_PCI
	struct device_node *np;
#endif

	if (ppc_md.progress)
		ppc_md.progress("mpc85xx_cds_setup_arch()", 0);

	cpu = of_find_node_by_type(NULL, "cpu");
	if (cpu != 0) {
		const unsigned int *fp;

		fp = get_property(cpu, "clock-frequency", NULL);
		if (fp != 0)
			loops_per_jiffy = *fp / HZ;
		else
			loops_per_jiffy = 500000000 / HZ;
		of_node_put(cpu);
	}

	cadmus = ioremap(CADMUS_BASE, CADMUS_SIZE);
	cds_pci_slot = ((cadmus[CM_CSR] >> 6) & 0x3) + 1;

	if (ppc_md.progress) {
		char buf[40];
		snprintf(buf, 40, "CDS Version = 0x%x in slot %d\n",
				cadmus[CM_VER], cds_pci_slot);
		ppc_md.progress(buf, 0);
	}

#ifdef CONFIG_PCI
	for_each_node_by_type(np, "pci") {
		if (of_device_is_compatible(np, "fsl,mpc8540-pci") ||
		    of_device_is_compatible(np, "fsl,mpc8548-pcie")) {
			struct resource rsrc;
			of_address_to_resource(np, 0, &rsrc);
			if ((rsrc.start & 0xfffff) == 0x8000)
				fsl_add_bridge(np, 1);
			else
				fsl_add_bridge(np, 0);
		}
	}

	ppc_md.pci_irq_fixup = mpc85xx_cds_pci_irq_fixup;
	ppc_md.pci_exclude_device = mpc85xx_exclude_device;
#endif
}

static void mpc85xx_cds_show_cpuinfo(struct seq_file *m)
{
	uint pvid, svid, phid1;
	uint memsize = total_memory;

	pvid = mfspr(SPRN_PVR);
	svid = mfspr(SPRN_SVR);

	seq_printf(m, "Vendor\t\t: Freescale Semiconductor\n");
	seq_printf(m, "Machine\t\t: MPC85xx CDS (0x%x)\n", cadmus[CM_VER]);
	seq_printf(m, "PVR\t\t: 0x%x\n", pvid);
	seq_printf(m, "SVR\t\t: 0x%x\n", svid);

	/* Display cpu Pll setting */
	phid1 = mfspr(SPRN_HID1);
	seq_printf(m, "PLL setting\t: 0x%x\n", ((phid1 >> 24) & 0x3f));

	/* Display the amount of memory */
	seq_printf(m, "Memory\t\t: %d MB\n", memsize / (1024 * 1024));
}


/*
 * Called very early, device-tree isn't unflattened
 */
static int __init mpc85xx_cds_probe(void)
{
        unsigned long root = of_get_flat_dt_root();

        return of_flat_dt_is_compatible(root, "MPC85xxCDS");
}

static struct of_device_id mpc85xx_ids[] = {
	{ .type = "soc", },
	{ .compatible = "soc", },
	{ .compatible = "fsl,rapidio-delta", },
	{ .compatible = "cfi-flash", },
	{},
};

static int __init mpc85xx_publish_devices(void)
{
	if (!machine_is(mpc85xx_cds))
		return 0;

	of_platform_bus_probe(NULL, mpc85xx_ids, NULL);

	return 0;
}
device_initcall(mpc85xx_publish_devices);

define_machine(mpc85xx_cds) {
	.name		= "MPC85xx CDS",
	.probe		= mpc85xx_cds_probe,
	.setup_arch	= mpc85xx_cds_setup_arch,
	.init_IRQ	= mpc85xx_cds_pic_init,
	.show_cpuinfo	= mpc85xx_cds_show_cpuinfo,
	.get_irq	= mpic_get_irq,
#ifdef CONFIG_PCI
	.restart	= mpc85xx_cds_restart,
	.pcibios_fixup_bus	= fsl_pcibios_fixup_bus,
#else
	.restart	= fsl_rstcr_restart,
#endif
	.calibrate_decr = generic_calibrate_decr,
	.progress	= udbg_progress,
};
