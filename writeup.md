# LAB Utility

# 环境配置

我是在wsl2 ubuntu 20.04上做这个lab的，因为打开wsl2比较方便。但是缺点就是无法关闭`qemu`（使用`ctrl-a x`没有反应，只能手动关掉窗口）

首先配置一下环境：
```shell
$ sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu 
```
如果出现错误，可以试着`sudo apt-get update`一下。
装完之后，可以看下版本。注意gcc编译器可能不是和我一样的。
```shell
$ riscv64-linux-gnu-gcc  --version
riscv64-linux-gnu-gcc (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0
Copyright (C) 2019 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
$  qemu-system-riscv64 --version
QEMU emulator version 4.2.1 (Debian 1:4.2-3ubuntu6.17)
Copyright (c) 2003-2019 Fabrice Bellard and the QEMU Project developers
```

拉取lab：
```shell
$ git clone git://g.csail.mit.edu/xv6-labs-2020

$ cd xv6-labs-2020

$ git checkout util

$ make qemu
```

为了方便进行格式化，我调了一个clang-format。需要使用的可以拷贝一下。


# sleep
这个lab就是调用一下`syscall`。`syscall`在`user/user.h`里面。  
注意，由于使用了`uint`，还需要引入`kernel/types.h`，并且要放在之前。

```c
#include "kernel/types.h"
#include "user/user.h"

int
main (int argc, char *argv[])
{
  int sleep_time;

  if (argc != 2)
    {
      fprintf (2, "Usage: Sleep should have one argument.\n");
      exit (0);
    }

  sleep_time = atoi (argv[1]);

  if (sleep_time < 0)
    {
      fprintf (2, "Usage: Sleep time should bigger than 0\n");
    }

  sleep (sleep_time);

  exit (0);
}
```

# pingpong
这个就是管道的简单应用。
