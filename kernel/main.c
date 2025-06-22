#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;
// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    //int checksum = virtio_disk_test(1);
    //printf("CHECKSUM %d\n", checksum);
    printf("kinit()\n");
    kinit();         // physical page allocator
    //for (int i = 0; i < 256; i ++) {
    //  printf("KALLOC TEST %d: %d\n", i, kalloc());
    //}
    //printf("KALLOC TEST1: %x\n", (uint32)kalloc());
    printf("kvminit()\n");
    kvminit();       // create kernel page table
    printf("kvminithart()\n");
    kvminithart();   // turn on paging
    printf("procinit()\n");
    procinit();      // process table
    printf("trapinit()\n");
    trapinit();      // trap vectors
    printf("trapinithart()\n");
    trapinithart();  // install kernel trap vector
    printf("plicinit()\n");
    plicinit();      // set up interrupt controller
    printf("plicinithart()\n");
    plicinithart();  // ask PLIC for device interrupts
    printf("binit()\n");
    binit();         // buffer cache
    printf("iinit()\n");
    iinit();         // inode cache
    printf("fileinit()\n");
    fileinit();      // file table
    printf("virtio()\n");
    virtio_disk_init(); // emulated hard disk
    printf("userinit()\n");
    userinit();      // first user process
    printf("all done?\n");
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
