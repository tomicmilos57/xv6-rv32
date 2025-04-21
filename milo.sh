#!/bin/bash

riscv32-unknown-elf-objcopy -O binary kernel/kernel kernel.bin
python bin2mif.py kernel.bin xv6.mif 8
cp xv6.mif ../RISCVerilog/misc
