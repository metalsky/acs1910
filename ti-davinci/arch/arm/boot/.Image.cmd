cmd_arch/arm/boot/Image := /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le/bin/arm_v5t_le-objcopy -O binary -R .note -R .comment -S  vmlinux arch/arm/boot/Image
