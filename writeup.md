# pre
实现目标：
* 一种比较简单的做法是不考虑父进程和子进程，只要出现写，就复制一份。

## 一开始的想法
* `uvmcopy`的时候把父进程页和子进程页都改成read only。建树
* 当尝试写一个read only的页的时候触发页写例外。
	* 向上找到第一个非`COW`的祖先A。
	* 遍历祖先A的子节点，如果节点对应的PTE也是`COW`的，那么递归遍历。
	* 对于每个`COW`节点，清除`COW`标记分配页。修改引用计数。
* 释放物理页
	* 引用计数使用物理地址作为标签
	* 当引用计数为0的时候释放物理页。

物理页大致要分配多少呢？即使物理地址除以4096后这个数还是很大。可以像堆一样动态分配内存，一次分配10个ref页的位置；或者取巧一下，直接计算出大约

```
A
|
|---|
B   C
|   |  
D   E
	|
|---|---|
F   G   H
```

```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  // char *mem;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    *pte &= ~PTE_W; //移除原来的写标记
    *pte |= PTE_COW; // 子进程添加COW标记
    flags = PTE_FLAGS(*pte);
    // if((mem = kalloc()) == 0)
    //   goto err;
    // memmove(mem, (char*)pa, PGSIZE);
      // kfree(mem);
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      goto err;
    }
    inc_ref(pa);
    printf("copy: %p map to %p flags:%p\n",i,pa,PTE_FLAGS(*pte));
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 0); 
  return -1;
}


int
alloccow(pagetable_t pagetable,uint64 va)
{
  pte_t *pte;
  uint flags;
  uint64 pa;
  char *mem;

  va = PGROUNDDOWN(va);
  if((pte = walk(pagetable, va, 0)) == 0)
      panic("alloccow: pte should exist");
  if((*pte & PTE_V) == 0)
    panic("alloccow: page not present");
  if((*pte & PTE_COW) == 0)
    panic("alloccow: not a COW");
  flags = (PTE_FLAGS(*pte) & (~PTE_COW)) | PTE_W;
  if((mem = kalloc()) == 0)return -1;
  pa = PTE2PA(*pte);
  memmove(mem, (char*)pa, PGSIZE);
  *pte = (uint64)PA2PTE(mem) |flags;
  kfree((void*)pa);
  printf("va:%p\n",va);
  return 0;
}


void print_pagetable(pagetable_t table,int level,int vpn[2]){
  int i;
  pte_t pte;
  uint64 va,pa;
  for(i=0;i<512;i++){
    pte = table[i];
    if (pte & PTE_V){
      pa = PTE2PA(pte);
      if(level==2){
        va = (uint64)vpn[0] << 30 | (uint64)vpn[1] << 21 | (uint64)i << 12;
        printf("%p map to %p;flags:%x\n",va,pa,PTE_FLAGS(pte));
      }else{
        vpn[level] = i;
        print_pagetable((pagetable_t)pa,level + 1,vpn);
      }
    }
  }
}
void
debug_pagetable(pagetable_t table)
{
  int vpn[2];
  print_pagetable(table,0,vpn);
}

    if((alloccow(p->pagetable,r_stval()))<0)
      p->killed = 1;
    printf("handle cow,pid=%d\n",p->pid);
```