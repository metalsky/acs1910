cmd_arch/arm/kernel/vmlinux.lds := /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/arm_v5t_le-gcc -E -Wp,-MD,arch/arm/kernel/.vmlinux.lds.d  -nostdinc -isystem /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/../lib/gcc/armv5tl-montavista-linux-gnueabi/4.2.0/include -D__KERNEL__ -Iinclude  -include include/linux/autoconf.h -include linux/marker.h -mlittle-endian  -DTEXT_OFFSET=0x00008000 -P -C -Uarm -D__ASSEMBLY__ -o arch/arm/kernel/vmlinux.lds arch/arm/kernel/vmlinux.lds.S

deps_arch/arm/kernel/vmlinux.lds := \
  arch/arm/kernel/vmlinux.lds.S \
    $(wildcard include/config/xip/kernel.h) \
    $(wildcard include/config/xip/phys/addr.h) \
    $(wildcard include/config/mmu.h) \
  include/linux/marker.h \
    $(wildcard include/config/markers/disable/optimization.h) \
    $(wildcard include/config/markers.h) \
  include/asm-generic/vmlinux.lds.h \
  include/asm/thread_info.h \
    $(wildcard include/config/debug/stack/usage.h) \
  include/linux/compiler.h \
  include/asm/fpstate.h \
    $(wildcard include/config/iwmmxt.h) \
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
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/xscale.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/copy/v6.h) \
    $(wildcard include/config/aeabi.h) \
  include/asm-generic/page.h \
  include/asm/sizes.h \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem.h) \
    $(wildcard include/config/out/of/line/pfn/to/page.h) \

arch/arm/kernel/vmlinux.lds: $(deps_arch/arm/kernel/vmlinux.lds)

$(deps_arch/arm/kernel/vmlinux.lds):
