cmd_arch/arm/lib/setbit.o := /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/arm_v5t_le-gcc -Wp,-MD,arch/arm/lib/.setbit.o.d  -nostdinc -isystem /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/../lib/gcc/armv5tl-montavista-linux-gnueabi/4.2.0/include -D__KERNEL__ -Iinclude  -include include/linux/autoconf.h -include linux/marker.h -mlittle-endian -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float    -c -o arch/arm/lib/setbit.o arch/arm/lib/setbit.S

deps_arch/arm/lib/setbit.o := \
  arch/arm/lib/setbit.S \
  include/linux/marker.h \
    $(wildcard include/config/markers/disable/optimization.h) \
    $(wildcard include/config/markers.h) \
  include/linux/linkage.h \
  include/asm/linkage.h \
  include/asm/assembler.h \
  include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/smp.h) \
  arch/arm/lib/bitops.h \
    $(wildcard include/config/cpu/32v6k.h) \

arch/arm/lib/setbit.o: $(deps_arch/arm/lib/setbit.o)

$(deps_arch/arm/lib/setbit.o):
