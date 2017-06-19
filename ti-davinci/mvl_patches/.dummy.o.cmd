cmd_mvl_patches/dummy.o := /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/arm_v5t_le-gcc -Wp,-MD,mvl_patches/.dummy.o.d  -nostdinc -isystem /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/../lib/gcc/armv5tl-montavista-linux-gnueabi/4.2.0/include -D__KERNEL__ -Iinclude  -include include/linux/autoconf.h -include linux/marker.h -mlittle-endian -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Os -marm -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi  -msoft-float -Uarm -fno-omit-frame-pointer -fno-optimize-sibling-calls  -fno-stack-protector -Wdeclaration-after-statement -Wno-pointer-sign    -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(dummy)"  -c -o mvl_patches/.tmp_dummy.o mvl_patches/dummy.c

deps_mvl_patches/dummy.o := \
  mvl_patches/dummy.c \
  include/linux/marker.h \
    $(wildcard include/config/markers/disable/optimization.h) \
    $(wildcard include/config/markers.h) \
  include/asm/marker.h \
  include/asm-generic/marker.h \

mvl_patches/dummy.o: $(deps_mvl_patches/dummy.o)

$(deps_mvl_patches/dummy.o):
