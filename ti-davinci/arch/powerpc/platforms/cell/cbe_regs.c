/*
 * cbe_regs.c
 *
 * Accessor routines for the various MMIO register blocks of the CBE
 *
 * (c) 2006 Benjamin Herrenschmidt <benh@kernel.crashing.org>, IBM Corp.
 */


#include <linux/config.h>
#include <linux/percpu.h>
#include <linux/types.h>
#include <linux/module.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/ptrace.h>

#include "cbe_regs.h"

/*
 * Current implementation uses "cpu" nodes. We build our own mapping
 * array of cpu numbers to cpu nodes locally for now to allow interrupt
 * time code to have a fast path rather than call of_get_cpu_node(). If
 * we implement cpu hotplug, we'll have to install an appropriate norifier
 * in order to release references to the cpu going away
 */
static struct cbe_regs_map
{
	struct device_node *cpu_node;
	struct cbe_pmd_regs __iomem *pmd_regs;
	struct cbe_iic_regs __iomem *iic_regs;
	struct cbe_mic_tm_regs __iomem *mic_tm_regs;
	struct cbe_pmd_shadow_regs pmd_shadow_regs;
} cbe_regs_maps[MAX_CBE];
static int cbe_regs_map_count;

static struct cbe_thread_map
{
	struct device_node *cpu_node;
	struct cbe_regs_map *regs;
} cbe_thread_map[NR_CPUS];

static struct cbe_regs_map *cbe_find_map(struct device_node *np)
{
	int i;
	struct device_node *tmp_np;

	if (strcasecmp(np->type, "spe") == 0) {
		if (np->data == NULL) {
			/* walk up path until cpu node was found */
			tmp_np = np->parent;
			while (tmp_np != NULL && strcasecmp(tmp_np->type, "cpu") != 0)
				tmp_np = tmp_np->parent;

			np->data = cbe_find_map(tmp_np);
		}
		return np->data;
	}

	for (i = 0; i < cbe_regs_map_count; i++)
		if (cbe_regs_maps[i].cpu_node == np)
			return &cbe_regs_maps[i];
	return NULL;
}

struct cbe_pmd_regs __iomem *cbe_get_pmd_regs(struct device_node *np)
{
	struct cbe_regs_map *map = cbe_find_map(np);
	if (map == NULL)
		return NULL;
	return map->pmd_regs;
}
EXPORT_SYMBOL_GPL(cbe_get_pmd_regs);

struct cbe_pmd_regs __iomem *cbe_get_cpu_pmd_regs(int cpu)
{
	struct cbe_regs_map *map = cbe_thread_map[cpu].regs;
	if (map == NULL)
		return NULL;
	return map->pmd_regs;
}
EXPORT_SYMBOL_GPL(cbe_get_cpu_pmd_regs);

struct cbe_pmd_shadow_regs *cbe_get_pmd_shadow_regs(struct device_node *np)
{
	struct cbe_regs_map *map = cbe_find_map(np);
	if (map == NULL)
		return NULL;
	return &map->pmd_shadow_regs;
}

struct cbe_pmd_shadow_regs *cbe_get_cpu_pmd_shadow_regs(int cpu)
{
	struct cbe_regs_map *map = cbe_thread_map[cpu].regs;
	if (map == NULL)
		return NULL;
	return &map->pmd_shadow_regs;
}

struct cbe_iic_regs __iomem *cbe_get_iic_regs(struct device_node *np)
{
	struct cbe_regs_map *map = cbe_find_map(np);
	if (map == NULL)
		return NULL;
	return map->iic_regs;
}

struct cbe_iic_regs __iomem *cbe_get_cpu_iic_regs(int cpu)
{
	struct cbe_regs_map *map = cbe_thread_map[cpu].regs;
	if (map == NULL)
		return NULL;
	return map->iic_regs;
}

struct cbe_mic_tm_regs __iomem *cbe_get_mic_tm_regs(struct device_node *np)
{
	struct cbe_regs_map *map = cbe_find_map(np);
	if (map == NULL)
		return NULL;
	return map->mic_tm_regs;
}

struct cbe_mic_tm_regs __iomem *cbe_get_cpu_mic_tm_regs(int cpu)
{
	struct cbe_regs_map *map = cbe_thread_map[cpu].regs;
	if (map == NULL)
		return NULL;
	return map->mic_tm_regs;
}
EXPORT_SYMBOL_GPL(cbe_get_cpu_mic_tm_regs);

/* FIXME
 * This is little more than a stub at the moment.  It should be
 * fleshed out so that it works for both SMT and non-SMT, no
 * matter if the passed cpu is odd or even.
 * For SMT enabled, returns 0 for even-numbered cpu; otherwise 1.
 * For SMT disabled, returns 0 for all cpus.
 */
u32 cbe_get_hw_thread_id(int cpu)
{
	return (cpu & 1);
}
EXPORT_SYMBOL_GPL(cbe_get_hw_thread_id);

void __init cbe_regs_init(void)
{
	int i;
	struct device_node *cpu;

	/* Build local fast map of CPUs */
	for_each_possible_cpu(i)
		cbe_thread_map[i].cpu_node = of_get_cpu_node(i, NULL);

	/* Find maps for each device tree CPU */
	for_each_node_by_type(cpu, "cpu") {
		struct cbe_regs_map *map = &cbe_regs_maps[cbe_regs_map_count++];

		/* That hack must die die die ! */
		const struct address_prop {
			unsigned long address;
			unsigned int len;
		} __attribute__((packed)) *prop;


		if (cbe_regs_map_count > MAX_CBE) {
			printk(KERN_ERR "cbe_regs: More BE chips than supported"
			       "!\n");
			cbe_regs_map_count--;
			return;
		}
		map->cpu_node = cpu;
		for_each_possible_cpu(i)
			if (cbe_thread_map[i].cpu_node == cpu)
				cbe_thread_map[i].regs = map;

		prop = get_property(cpu, "pervasive", NULL);
		if (prop != NULL)
			map->pmd_regs = ioremap(prop->address, prop->len);

		prop = get_property(cpu, "iic", NULL);
		if (prop != NULL)
			map->iic_regs = ioremap(prop->address, prop->len);

		prop = (struct address_prop *)get_property(cpu, "mic-tm",
							   NULL);
		if (prop != NULL)
			map->mic_tm_regs = ioremap(prop->address, prop->len);
	}
}

