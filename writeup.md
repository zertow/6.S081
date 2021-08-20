# RISC-V assembly
1. Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?   
a0,a1,a2寄存器

2. Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)  
被编译器直接算出了一个12了。
```asm
  22:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  24:	4635                	li	a2,13
  26:	45b1                	li	a1,12
```
3. At what address is the function printf located?  
0x630  

4. What value is in the register ra just after the jalr to printf in main?
ra是返回地址寄存器。可以翻一下`printf`函数，会发现返回地址没变过。因此看这main就行：
```asm
  30:	00000097          	auipc	ra,0x0
  34:	600080e7          	jalr	1536(ra) # 630 <printf>
```
这里`auipc`是把当前pc加上0放进了ra。和x86跳转不同，这里的pc就是当前指令的地址。接下来`jalr`跳转到了0x630(1536+48=0x630)。但是注意这里ra寄存器也变化了，变化为下一条指令的地址，即0x38。因此ra寄存器里是0x38

5. Run the following code.
```c
unsigned int i = 0x00646c72;
printf("H%x Wo%s", 57616, &i);
```
这个题目是小端法和大端法的问题。xv6是小端的。   
同时第一个打印的16进制，不需要0x；第二个打印的是字母，根据ascii值打印。  
打印的结果是:HE110 World

6. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?   
```
printf("x=%d y=%d", 3);
```
y打印出来的值是1，这个值和a2寄存器之前存的值有关系，我查了一会，估计是在printint里面修改的，但也没有仔细再看了。

# Backtrace


这个lab也比较简单，但是我写的时候犯了一个很大的错误：  
这是我写的一个错误的版本
```c
void 
backtrace(void)
{
  uint64* fp = (uint64*)r_fp(); // 栈基址的指针
  uint64* page_up = (uint64*)PGROUNDUP((uint64)(fp));
  printf("backtrace:\n");
  while(fp < page_up ) {
    printf("%p\n",*(fp - 8)); 
    fp = (uint64*)*(fp - 16); 
  }
}
```
问题在哪里呢？注意fp是一个指针，对指针进行加减运算，实际上是`+n := +sizeof(type)*n`。  
然后在`fp = (uint64*)*(fp - 16)`的地方，-16就变成了-128了。所以就进了死循环了。  
改成这样就对了：
```c
void 
backtrace(void)
{
  uint64* fp = (uint64*)r_fp(); // 栈基址的指针
  uint64* page_up = (uint64*)PGROUNDUP((uint64)(fp));
  printf("backtrace:\n");
  while(fp < page_up ) {
    printf("%p\n",*(fp - 1)); // fp-8是返回地址
    fp = (uint64*)*(fp - 2); 
  }
}
```
当然，也可以把fp声明成uint64，这样就不会有这个问题了。

# Alarm

这个lab分为了两部分。

## test0

首先看第一部分。

第一部分需要添加两个`syscall`，使得lab能够运行起来。这部分和`lab syscall`类似，这里就不再赘述。添加完后，需要在`sys_sigalarm`中保存tick的信息和函数指针。

```c
/**
 *  添加系统调用
 **/

//kernel/syscall.c
extern uint64 sys_sigalarm(void);
extern uint64 sys_sigreturn(void);

static uint64 (*syscalls[])(void) = {
    //....
    [SYS_sigalarm]   sys_sigalarm,
	[SYS_sigreturn]   sys_sigreturn,
}

//kernel/syscall.h
#define SYS_sigalarm  22
#define SYS_sigreturn  23

//user/user.h
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);

//user/usys.pl
entry("sigalarm");
entry("sigreturn");

//Makefile
UPROGS=\
    //...
    $U/_alarmtest\ 

//kernel/proc.h
struct proc{
   //....
  int alarm_tick;
  int alarm_total_tick;
  void* alarm_func;
}
//kernel/proc.c
static struct proc*
allocproc(void){
    //...
    p->context.ra = (uint64)forkret;
	p->context.sp = p->kstack + PGSIZE;
    p->alarm_total_tick = 0; //add
    //...
}

//kernel/sysproc.c
uint64 sys_sigalarm(void){
  struct proc *p = myproc();
  if(argint(0, &p->alarm_total_tick) || argaddr(1, (uint64*)&p->alarm_func))
    return -1;
  p->alarm_tick = p->alarm_total_tick;
}

```

然后在`trap.c的usertrap`中，要添加相应的响应。一开始我因为注释上写`send interrupts and exceptions to kerneltrap()` ，我以为是在`systrap`里面。后面发现只要在`usertrap`里改就行了。

读一下代码可以发现`which_dev=2`的时候，就是有时钟中断的时候。因此可以修改if循环。主要这里不要忘了处理`which_dev!=0`的其他情况：

```c
if(r_scause() == 8){
    //...
}else if((which_dev = devintr()) != 0){//之前忘了把这里改成==2了，usertest就出错了
	if(which_dev==2&& p->alarm_total_tick !=0){
    p->alarm_tick-=1;
    if(p->alarm_tick==0){
        p->alarm_tick = p->alarm_total_tick;
        p->trapframe->epc = (uint64)p->alarm_func;
      }
    }
}else{
    // ....
}
```

为什么这样改可以呢？因为进入用户态调用的是`userret()`。但是`userret()`存的pc是之前ecall的pc+4,而不是我们想要的函数地址，看一下`userret`的代码可以发现，它读取的pc是`p->trapframe->epc`，因此我们只要修改这个就可以了。

## test1/2

接下来是返回。函数返回时，会调用`sys_sigreturn`。我们只需要在这里复原寄存器和pc就可以了。要复原的话，就得提前存起来。因此需要分配一个`trapframe`来保存信息。另外，提示中说如果当前正在执行alarm函数，而且没返回，就不允许再执行alarm函数。

```c
//kernel/proc.h
struct proc{
   //....
  int alarm_state; // 当前alarm函数状态
  struct trapframe * alarm_trapframe; // alarm专用trapframe
}
//kernel/proc.c
static struct proc*
allocproc(void){
  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }
  // 分配alarm_trapframe
  if((p->alarm_trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }
  p->alarm_state=0;
  //....
}

// user/trap.c
usertrap(void){
    //....
 else if((which_dev = devintr()) ==2){
    // ok
    if(p->alarm_total_tick !=0){
    p->alarm_tick-=1;
    if(p->alarm_tick==0 && p->alarm_state == 0){
        p->alarm_tick = p->alarm_total_tick;
        // 备份p->alarm_trapframe
        memmove(p->alarm_trapframe,p->trapframe,sizeof(struct trapframe));
        p->trapframe->epc = (uint64)p->alarm_func;
        p->alarm_state = 1;
      }
    }
  } else {
    //....
}
// kernel/sysproc.c
uint64 sys_sigreturn(void){
  struct proc *p = myproc();
  if(p->alarm_state == 1){
    p->alarm_state = 0;
  }
  // 重新覆写p->alarm_trapframe
  memmove(p->trapframe,p->alarm_trapframe,sizeof(struct trapframe));
  return 0;
}

```





## 遇到的问题

### 看错提示了

提示上说函数的地址可以是0，我则看成了函数的地址不应该为0，然后以为是参数获取错误了，折腾了好久

### 改错地方没有复原

这个lab的代码量不大，主要是要知道要改哪里。一开始的时候比较混乱，改错了很多地方，而且也没有复原，后面`alarmtest`过了，但是`usertests`一直过不了。于是我通过git checkout跳转到旧版本看是否能运行`usertests`，再用git commit逐一比对错误的地方，最后解决了问题。当然这个主要是写代码习惯不好的问题，应该每通过一个lab，或者每一次commit都要检查是否通过usertests。后面可以考虑用CI工具来检查这个。

