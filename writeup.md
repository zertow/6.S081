这个lab总体来说代码量不是很大，复杂度也不会特别高。但是我还是搞了好几天。主要是对xv6的文件系统不太熟悉，看的课是10多天前看的，因为事前没看代码，听的时候就有些云里雾里的，日志部分倒是听得比较懂。

# Large files
这里我的实现是会超时的，但是功能是能通过的。

这个lab要求实现拓展文件最大长度的功能。xv6的文件格式主要在`file.h`中定义。

```c
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2];
};

// map major device number to device functions.
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};
```
![](https://raw.githubusercontent.com/ruiqurm/image_host/master/blog/20210918011716.png)
借用一下csapp中的图   
其中，`inode`是磁盘中记录文件真正存在的实体，而file是内存中为了方便处理的文件对象。多个文件对象可以指向同一个inode。
![](https://raw.githubusercontent.com/ruiqurm/image_host/master/blog/20210918011857.png)
与控制文件大小的是`inode`addrs的大小。在原版实现中，前12个是直接块，直接映射到磁盘空间上；第13个是一个256的间接块。因此文件数据最多是268个块。在本lab中，要拓展到65803，即一个二级间接块+一级间接块+11个直接块。这里的思想和页表那里类似，都是利用多级链块节省空间。  
下面修改相关的宏(`fs.h`)定义：
```c
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint)) // 一阶间接
#define NINDIRECT_2 NINDIRECT*NINDIRECT // 二阶间接
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT_2)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
};
```
这里稍微改了一下最大空间和一些宏定义，以便修改bmap。  
注意如果修改了`NDIRECT`要记得改`file.h`中的相应宏定义。（这里我一开始忘改了，虽然手册里有提示）  

接着就是修改分配函数了。  
bmap函数返回了是指定inode的第n个块。对于直接块和第一个间接块不需要修改。（当时我修改的时候删了一个地方，后面找了很久也没发现问题）。  
对于第二个间接块，和前面类似的处理，仿写一下就可。这边我是用先除后模的方式做索引，除是一级索引，模是二级索引。这样更满足局部性原理（当然比较直接的想法应该都是这样）。然后先查找或创建一级索引，再查找或创建二级索引。这里`log_write`是`bwrite`的日志版。如果忘记加了，会导致节点没有写回（即内存里有节点，但是磁盘上没有）。导致的症状就是65803都写完了，但是提示写入数据错误。
```c
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  uint index,offset;
  struct buf *bp,*bp2;
  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp); // 忘加了这个
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;
  if (bn < NINDIRECT_2){
    if((addr = ip->addrs[NDIRECT+1]) == 0)
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    index = bn / NINDIRECT;
    offset = bn % NINDIRECT; 
    if((addr = a[index]) == 0){
      a[index] = addr = balloc(ip->dev);
      log_write(bp);
    }
    bp2 = bread(ip->dev, addr);
    a = (uint*)bp2->data;
    if((addr = a[offset]) == 0){
      a[offset] = addr = balloc(ip->dev);
      log_write(bp2);
    }
    brelse(bp2);
    brelse(bp);
    return addr;
  }
  panic("bmap: out of range");
}
```

接下来看`itrunc`函数。这个函数负责清理所有的节点。因此也需要修改一下。重点是`doubly-indirect`，因此我就不再贴上前面的代码：
```c
  //....	
  if(ip->addrs[NDIRECT+1]){
    // 释放doubly-indirect
    bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j]){// 一级节点
        bp2 = bread(ip->dev,a[j]);
        a2 = (uint*)bp2->data;
        for(k=0;k < NINDIRECT; k++){
          if(a2[k]){ //二级节点
            bfree(ip->dev,a2[k]); // 真正的磁盘块
          }
        }
        brelse(bp2);
        bfree(ip->dev, a[j]);
        a[j] = 0; // 可加可不加
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT+1]);
    ip->addrs[NDIRECT+1] = 0;
  }
```
这里要注意`brelse`和`bfree`。前者是释放锁，后者是清理掉一个节点。一级节点，二级节点需要解锁，也需要在处理结束后清理掉。


## 遇到的问题
* 没加`log_write`
* 忘记改`file.h`中`NDIRECT`相关的部分

# Symbolic links
这个lab搞了2天。主要原因是不熟悉文件系统，还看错了指导书。 
* 忘记内核和用户空间有隔离，一开始传参的时候直接用了用户传来的指针（当然也可以，但是不会检查指针指的字符串） 
* 不明白节点的结构，研究了半天文件名到底存在哪，到底怎么通过文件名取节点。
* 我以为`symbol link`添加的时候要保证链接的路径一定存在，后面搞了半天，发现不是这么回事
* 用`ls`做测试的时候发现结果不太对，结果发现是`open`传参数的锅。   
....    
总之一直在原地打转，错误归因。  
最主要的原因就是有点心急了，想快速把lab搞完，代码也没怎么看就上手了。理解题目也没理解好，于是就导致了这样的结果。   

## 思路
做这个lab之前首先要明白什么是软链接；什么是硬链接。  

因为xv6 inode的设计，路径名和inode实际上是分离的。多个路径名可以指向同一个inode。

**硬链接**：把一个路径和inode绑定起来，即上面说的多个路径名可以指向同一个inode情况。也可以看成**挂载**。  
**软链接**：把一个inode指向一个**路径**。即windows中的快捷方式，mac中的`alias`。这种情况下，那个路径可能没有文件。

要做软链接的话，就是新建一个文件，里面存一个路径。当`open`的时候，重定向到它指的位置即可。   

## 实现

首先做一个`syscall`。这一部分忘了可以参考前面的。  
添加`T_SYMLINK`和`O_NOFOLLOW`。  
注意`O_NOFOLLOW`一定要是2的幂并且不能和之前几个冲突。因为它是要取或的。这里我用的是`0x040`。`0x080`之类的也可。  

函数的实现如下：
```c
uint64
sys_symlink(void)
{
  // symlink
  char src[MAXPATH],dst[MAXPATH]; //[1]
  int n;
  struct inode *ip;
  if( (argstr(0,src,MAXPATH) <0) || (argstr(1,dst,MAXPATH) <0) ){
    return -1;
  }

  begin_op();
  // 创建一个新的节点，写入node信息
  if ( (ip = create(dst,T_SYMLINK,0,0))==0){ // [2]
    panic("symlink:create failed");
  }
  n = strlen(src);
  if (writei(ip,0,(uint64)&src,0,n)!=n){ // [3] 
    panic("symlink:write");
  }
  iunlockput(ip); // [4]
  end_op();
  return 0; 
}
```
首先看`[1]`。这里我一开始忘了，设成了文件名最大长度（14），然后`argstr`一直返回负数。  
 
再看`[2]`。这里创建了一个文件，类型为`T_SYMLINK`。一开始我是用`ialloc`慢慢自己写的，后来发现有现成的，就用它了。不过主要要判断返回值。因为这个返回值，我又检查了4个小时。这里返回值是0的情况下，可能是已经存在文件了。但是原版的create里不会判断软链接的类型，因此就会直接返回0.

接下来是`writei`。这里是写入软链接的位置。指导书上建议放在数据块，当然直接存在结构体里面也是可以的。当然代价就是所有inode的结构体都要变长。

再看`iunlockput`函数。这个函数是两个函数的组合。即`unlock`和`iput`。前者表示对指针解锁，后者表示直接减小一个引用。大部分情况下直接用`iunlockput`。这表示后面不会再用`ip`指针和它内部的数据了。

接下来需要修改`open`。
```c
// ....
if((ip = namei(path)) == 0){
  end_op();
  return -1;
}
ilock(ip);
//增加的部分：
if ((ip->type==T_SYMLINK)  && !(omode & O_NOFOLLOW)){
  // follow link
  readi(ip,0,(uint64)path,0,MAXPATH);
  iunlockput(ip);
  if ((ip = namei(path))<=0){
    end_op();
    // printf("%s failed2\n",path);
    return -1;
  }
  ilock(ip);
  n = 0;
  while(ip->type == T_SYMLINK){
    readi(ip,0,(uint64)path,0,MAXPATH);
    iunlockput(ip);
    n += 1;
    if ((ip = namei(path))<=0 || n>10){
      end_op();
      // printf("%s failed3\n",path);
      return -1;
    }
    ilock(ip);
  }
}
// 上面是增加的部分

if(ip->type == T_DIR && omode != O_RDONLY){
  iunlockput(ip);
  end_op();
  return -1;
}
// ....
```
这里打开文件时，如果是软链接，并且打开模式没有置`O_NOFOLLOW`，那么就顺着链接指的地址去查找文件。如果下一个链接还是软链接，继续向下，直到遇到一个非软链接。如果超过一定的长度，判断为死循环。当然也可以保存一下开头的地址，然后判断下一个地址是不是开头的地址。  

这里主要需要注意的是各种`ilock`、`iunlockput`和`end_op()`

最后要修改的是之前说的`create`函数。

这里就是添加上对`SYMLINK`文件的支持。不添加这个在`concurrent`测试中就会失败。
```c
static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if((type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE)) ||
       (type == T_SYMLINK && ip->type == T_SYMLINK))
       // 修改
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }
  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);
  return ip;
}
```

## 遇到的问题
* 一开始没理解API，不太清楚文件系统的结构，不知道怎么获取文件名。
* 看错了。以为建立软链接的时候要检查文件。
* 误解了`unlink`的意思。这其实是删除一个文件的意思。
* `iunlockput`的意思没理解
* `sys_symlink`字符数组开小了
* 没有检查`create`的返回值，导致出现了kernel trap

## 其他
这里其实没有做对ls的支持，如果对有软链接的文件做`ls`，会出现cannot stat的问题，或者直接显示链接文件的数据。这里以后可以改改。