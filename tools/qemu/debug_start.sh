#!/bin/sh

qemu-system-aarch64 -cpu cortex-a57 -machine type=virt,gic-version=2 -smp 8 -m 1024M -kernel ../../arch/arm64/boot/Image -device virtio-scsi-device -serial stdio -s -S
