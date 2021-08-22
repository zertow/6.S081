

## 遇到的部分问题
这里记录一下一些bug解决的过程。  
### sbrk的问题
sbrk我至少犯了两个错误。
### sbrk的返回值
第一个问题是返回值。  
首先看原来的sbrk：
```c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}
```
可以看到这里addr返回要么是**没变过**的大小，要么是-1。并且这里addr作为一个地址是32位大小的，可能有错误的隐患（[这篇blog](https://blog.csdn.net/passenger12234/article/details/117869436)指出了这个问题，但是似乎int也能通过测试)。  
我一开始将`addr`变过的值传回去，结果一直出现访问一个越界地址的页错误，我一直以为是缺页处理有问题：
```c
// bash
$ echo hi
exec echo failed
panic: freewalk: leaf 
// 不应该出现这个问题
```
打印一下r_stval()可以发现是一直在访问0x14008(但是只分配到0x14000)。
```c
uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;
  struct proc *p = myproc();
  addr =  p->sz;
  if(argint(0, &n) < 0)
    return -1;
  if(n >= 0){
    p->sz = addr+n; 
  }else{
    p->sz = uvmdealloc(p->pagetable, addr, addr + n);
  }
  return addr;// 返回原来的地址才对
}
```
### sbrk的size应该设置为多少
一开始我想虽然是`lazy allocation`,但是也应该一页一页的分配才对，因此就在`sys_sbrk`里给`p->sz`赋值为这样：
```c
  if(n >= 0){
    p->sz = PGROUNDUP(addr+n); 
  }else{
    p->sz = uvmdealloc(p->pagetable, addr, addr + n);
  }
```
这样写`usertests`里的`sbrkbasic`可能会出现这个问题：
```bash
test sbrkbasic: �"�: sbrk test failed 69633 12000 FFFFFFFF
```
再仔细想想看就不对了。这样每次都会分配一页，结果就是多分配了很多空间：比如调用`sbrk(1);sbrk(1);sbrk(1);`，不是`lazy_allocation`的话也只会分配到`size=3`，我这样写就直接分配到0xc000上去了。

```bash
test kernmem: : oops could read 0 = 0
panic: freewalk: leaf
```
这个搜一下`oops could read`，发现这个测试是测能否读到内核数据的。结果发现读到了。说明缺页处理有问题。
```c
//....
} else if ( (r_scause()== 13 || r_scause() ==15) && (r_stval() < p->sz || r_stval() > p->trapframe->sp)){
    mem = kalloc();
    if(mem != 0){
      va = PGROUNDDOWN(r_stval());
      memset(mem, 0, PGSIZE);
      if(mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
        uvmdealloc(p->pagetable, va, va -PGSIZE);
        kfree(mem);
        p->killed = 1;
      }
    }else{
      p->killed = 1;
    }

  } else {
//....
```
我一开始感觉没问题，后面找了个别人写的比对一下，发现判断条件写错了。我理解错进程的模型了。  
![](https://raw.githubusercontent.com/ruiqurm/image_host/master/blog/20210822222507.png)
我以为用户栈是在`p->sz`之上...  所以写错了。正确的写法是这样的：
```
if(va >= p->sz || va < p->trapframe->sp)return 0;
```
之前的理解确实错的离谱，一部分是看得太快的原因，现在补一下课；  
进程初始化的时候，会分配一个固定的空间。也就是分配到上图中的STACK那里。然后，stack处作为栈基址，向下做一个guard page。而`sbrk`的时候，伸长出来的内存就是堆内存。   

不过这个改完以后还是不行，仔细看一下hint和其他的blog，发现少做了supervisor模式的页错误。这个页错误主要是`copyin`函数内的`walkaddr`产生的。因此修改`walkaddr`即可：
```c
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;
  struct proc * p = myproc();
  char * mem;
  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0 || ((*pte & PTE_V) == 0) ){
    if(va >= p->sz || va < p->trapframe->sp)return 0;
    mem = kalloc();
    if(mem == 0)return 0;
    va = PGROUNDDOWN(va);
    memset(mem, 0, PGSIZE);
    if(mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      return 0;
    }
    return (uint64)mem;
  }
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}
```




参考：  
* （有完整代码）https://blog.csdn.net/u012419550/article/details/115415194
* （比较详细分析）https://blog.csdn.net/RedemptionC/article/details/107464400
* https://blog.csdn.net/passenger12234/article/details/117869436