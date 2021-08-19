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

