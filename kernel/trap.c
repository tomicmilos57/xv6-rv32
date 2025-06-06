#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint32)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint32)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->tf->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->tf->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();
    //printf("Process: %d JOJ\n", p->tf->a7);
    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void print_trapframe(struct trapframe *tf) {
    printf("trapframe at %p:\n", tf);
    printf(" kernel_satp   = 0x%x\n", tf->kernel_satp);
    printf(" kernel_sp     = 0x%x\n", tf->kernel_sp);
    printf(" kernel_trap   = 0x%x\n", tf->kernel_trap);
    printf(" epc           = 0x%x\n", tf->epc);
    printf(" kernel_hartid = 0x%x\n", tf->kernel_hartid);
    printf(" ra            = 0x%x\n", tf->ra);
    printf(" sp            = 0x%x\n", tf->sp);
    printf(" gp            = 0x%x\n", tf->gp);
    printf(" tp            = 0x%x\n", tf->tp);
    printf(" t0            = 0x%x\n", tf->t0);
    printf(" t1            = 0x%x\n", tf->t1);
    printf(" t2            = 0x%x\n", tf->t2);
    printf(" s0            = 0x%x\n", tf->s0);
    printf(" s1            = 0x%x\n", tf->s1);
    printf(" a0            = 0x%x\n", tf->a0);
    printf(" a1            = 0x%x\n", tf->a1);
    printf(" a2            = 0x%x\n", tf->a2);
    printf(" a3            = 0x%x\n", tf->a3);
    printf(" a4            = 0x%x\n", tf->a4);
    printf(" a5            = 0x%x\n", tf->a5);
    printf(" a6            = 0x%x\n", tf->a6);
    printf(" a7            = 0x%x\n", tf->a7);
    printf(" s2            = 0x%x\n", tf->s2);
    printf(" s3            = 0x%x\n", tf->s3);
    printf(" s4            = 0x%x\n", tf->s4);
    printf(" s5            = 0x%x\n", tf->s5);
    printf(" s6            = 0x%x\n", tf->s6);
    printf(" s7            = 0x%x\n", tf->s7);
    printf(" s8            = 0x%x\n", tf->s8);
    printf(" s9            = 0x%x\n", tf->s9);
    printf(" s10           = 0x%x\n", tf->s10);
    printf(" s11           = 0x%x\n", tf->s11);
    printf(" t3            = 0x%x\n", tf->t3);
    printf(" t4            = 0x%x\n", tf->t4);
    printf(" t5            = 0x%x\n", tf->t5);
    printf(" t6            = 0x%x\n", tf->t6);
}
void
usertrapret(void)
{
  struct proc *p = myproc();

  // turn off interrupts, since we're switching
  // now from kerneltrap() to usertrap().
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->tf->kernel_satp = r_satp();         // kernel page table
  p->tf->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->tf->kernel_trap = (uint32)usertrap;
  p->tf->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->tf->epc);

  // tell trampoline.S the user page table to switch to.
  uint32 satp = MAKE_SATP(p->pagetable);
  //print_trapframe(p->tf);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint32 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint32,uint32))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
// must be 4-byte aligned to fit in stvec.
void 
kerneltrap()
{
  int which_dev = 0;
  uint32 sepc = r_sepc();
  uint32 sstatus = r_sstatus();
  uint32 scause = r_scause();
  

  //if((sstatus & SSTATUS_SPP) == 0)
  //  panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint32 scause = r_scause();

  if((scause & 0x80000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else {
    }

    plic_complete(irq);
    return 1;
  } else if(scause == 0x80000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

