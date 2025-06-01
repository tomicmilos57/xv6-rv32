#!/bin/bash

riscv32-unknown-elf-objcopy -O binary kernel/kernel kernel.bin
riscv32-unknown-elf-objdump -D kernel/kernel >objdump.txt
python bin2mif.py kernel.bin xv6.mif 8
cp xv6.mif ../RISCVerilog/misc
cp kernel.bin ../remu/misc
cp objdump.txt ../remu/misc
cp fs.img ../remu/misc
