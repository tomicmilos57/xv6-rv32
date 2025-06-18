#!/bin/bash

make clean
make --eval='SDCARD_EN=1'
if [ ! $? -eq 0 ]; then
  echo "EXITING: Objcopy failed"
  exit -1
fi
make fs.img
if [ ! $? -eq 0 ]; then
  echo "EXITING: Objcopy failed"
  exit -1
fi

if [[ ! -f kernel/kernel ]]; then
  echo "Can not find kernel binary"
  exit -1
fi

riscv32-unknown-elf-objcopy -O binary kernel/kernel kernel.bin
if [ ! $? -eq 0 ]; then
  echo "EXITING: Objcopy failed"
  exit -1
fi

riscv32-unknown-elf-objdump -D kernel/kernel >objdump.txt
if [ ! $? -eq 0 ]; then
  echo "EXITING: Objdump failed"
  exit -1
fi

if [[ ! -f bin2mif.py ]]; then
  echo "EXITING: Can not find bin2mif.py"
  exit -1
else
  python bin2mif.py kernel.bin xv6.mif 8
  dd if=kernel.bin of=kernel_32kB.bin bs=1 count=32768
  dd if=kernel.bin of=kernel_16kB.bin bs=1 skip=32768 count=16384
  python bin2mif.py kernel_32kB.bin kernel_32kB.mif 8
  python bin2mif.py kernel_16kB.bin kernel_16kB.mif 8
fi

if [[ ! -d ../RISCVerilog/misc ]]; then
  echo "EXITING: Can not find RISCVerilog"
else
  cp xv6.mif ../RISCVerilog/misc
  cp kernel_32kB.mif ../RISCVerilog/misc
  cp kernel_16kB.mif ../RISCVerilog/misc
  cp fs.img ../RISCVerilog/misc
fi

if [[ ! -d ../remu/misc ]]; then
  echo "EXITING: Can not find REMU"
else
  cp kernel.bin ../remu/misc
  cp objdump.txt ../remu/misc
  cp fs.img ../remu/misc
fi
