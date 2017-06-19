cmd_arch/arm/boot/compressed/head.o := /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/arm_v5t_le-gcc -Wp,-MD,arch/arm/boot/compressed/.head.o.d  -nostdinc -isystem /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/../lib/gcc/armv5tl-montavista-linux-gnueabi/4.2.0/include -D__KERNEL__ -Iinclude  -include include/linux/autoconf.h -include linux/marker.h -mlittle-endian -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float    -c -o arch/arm/boot/compressed/head.o arch/arm/boot/compressed/head.S

deps_arch/arm/boot/compressed/head.o := \
  arch/arm/boot/compressed/head.S \
    $(wildcard include/config/debug/icedcc.h) \
    $(wildcard include/config/cpu/v6.h) \
    $(wildcard include/config/arch/sa1100.h) \
    $(wildcard include/config/debug/ll/ser3.h) \
    $(wildcard include/config/arch/omap2.h) \
    $(wildcard include/config/omap/ll/debug/uart2.h) \
    $(wildcard include/config/omap/ll/debug/uart3.h) \
    $(wildcard include/config/arch/iop331.h) \
    $(wildcard include/config/arch/s3c2410.h) \
    $(wildcard include/config/s3c2410/lowlevel/uart/port.h) \
    $(wildcard include/config/cpu/cp15.h) \
    $(wildcard include/config/runtime/phys/offset.h) \
    $(wildcard include/config/zboot/rom.h) \
    $(wildcard include/config/arch/rpc.h) \
    $(wildcard include/config/cpu/dcache/writethrough.h) \
    $(wildcard include/config/processor/id.h) \
    $(wildcard include/config/mcount.h) \
  include/linux/marker.h \
    $(wildcard include/config/markers/disable/optimization.h) \
    $(wildcard include/config/markers.h) \
  include/linux/linkage.h \
  include/asm/linkage.h \

arch/arm/boot/compressed/head.o: $(deps_arch/arm/boot/compressed/head.o)

$(deps_arch/arm/boot/compressed/head.o):
