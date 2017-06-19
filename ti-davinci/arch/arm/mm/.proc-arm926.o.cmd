cmd_arch/arm/mm/proc-arm926.o := /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/arm_v5t_le-gcc -Wp,-MD,arch/arm/mm/.proc-arm926.o.d  -nostdinc -isystem /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/../lib/gcc/armv5tl-montavista-linux-gnueabi/4.2.0/include -D__KERNEL__ -Iinclude  -include include/linux/autoconf.h -include linux/marker.h -mlittle-endian -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float    -c -o arch/arm/mm/proc-arm926.o arch/arm/mm/proc-arm926.S

deps_arch/arm/mm/proc-arm926.o := \
  arch/arm/mm/proc-arm926.S \
    $(wildcard include/config/cpu/arm926/cpu/idle.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/cpu/dcache/writethrough.h) \
    $(wildcard include/config/cpu/cache/round/robin.h) \
  include/linux/marker.h \
    $(wildcard include/config/markers/disable/optimization.h) \
    $(wildcard include/config/markers.h) \
  include/linux/linkage.h \
  include/asm/linkage.h \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/memory/hotplug.h) \
    $(wildcard include/config/acpi/hotplug/memory.h) \
    $(wildcard include/config/acpi/hotplug/memory/module.h) \
  include/linux/compiler.h \
  include/asm/assembler.h \
  include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/smp.h) \
  include/asm/pgtable-hwdef.h \
  include/asm/pgtable.h \
  include/asm-generic/4level-fixup.h \
  include/asm/proc-fns.h \
    $(wildcard include/config/cpu/32.h) \
    $(wildcard include/config/cpu/arm610.h) \
    $(wildcard include/config/cpu/arm7tdmi.h) \
    $(wildcard include/config/cpu/arm710.h) \
    $(wildcard include/config/cpu/arm720t.h) \
    $(wildcard include/config/cpu/arm740t.h) \
    $(wildcard include/config/cpu/arm9tdmi.h) \
    $(wildcard include/config/cpu/arm920t.h) \
    $(wildcard include/config/cpu/arm922t.h) \
    $(wildcard include/config/cpu/arm925t.h) \
    $(wildcard include/config/cpu/arm926t.h) \
    $(wildcard include/config/cpu/arm940t.h) \
    $(wildcard include/config/cpu/arm946e.h) \
    $(wildcard include/config/cpu/sa110.h) \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/arm1020.h) \
    $(wildcard include/config/cpu/arm1020e.h) \
    $(wildcard include/config/cpu/arm1022.h) \
    $(wildcard include/config/cpu/arm1026.h) \
    $(wildcard include/config/cpu/xscale.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/v6.h) \
  include/asm/memory.h \
    $(wildcard include/config/runtime/phys/offset.h) \
    $(wildcard include/config/dram/size.h) \
    $(wildcard include/config/dram/base.h) \
    $(wildcard include/config/discontigmem.h) \
  include/asm/arch/memory.h \
    $(wildcard include/config/davinci/kernel/sdram/offset.h) \
    $(wildcard include/config/arch/da8xx.h) \
  include/asm/page.h \
    $(wildcard include/config/cpu/copy/v3.h) \
    $(wildcard include/config/cpu/copy/v4wt.h) \
    $(wildcard include/config/cpu/copy/v4wb.h) \
    $(wildcard include/config/cpu/copy/v6.h) \
    $(wildcard include/config/aeabi.h) \
  include/asm-generic/page.h \
  include/asm/sizes.h \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem.h) \
    $(wildcard include/config/out/of/line/pfn/to/page.h) \
  include/asm/arch/vmalloc.h \
  include/asm/arch/io.h \
  include/asm/procinfo.h \
  arch/arm/mm/proc-macros.S \
  include/asm/asm-offsets.h \
  include/asm/thread_info.h \
    $(wildcard include/config/debug/stack/usage.h) \
  include/asm/fpstate.h \
    $(wildcard include/config/iwmmxt.h) \

arch/arm/mm/proc-arm926.o: $(deps_arch/arm/mm/proc-arm926.o)

$(deps_arch/arm/mm/proc-arm926.o):
