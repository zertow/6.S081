#include "kernel/types.h"
#include "user/user.h"

int
main (int argc, char *argv[])
{
  /**
   * 把输入转成输出，然后
   *
   */
  int i;
  char buf[32];
  char *arg[32];
  int arg_p = 0;
  int count = 0;
  for (i = 1; i < argc; i++) {
    arg[arg_p++] = argv[i];
  }
  count = read (0, buf, 32);
  buf[count] = 0;
  arg[arg_p++] = buf;
  for (i = 0; i < count; i++) {
    //分割字符串
    if (buf[i] == ' ') {
      buf[i] = 0;
      arg[arg_p++] = buf + i + 1;
    }
  }

  if (fork () == 0) {
    exec (argv[1], arg);
    exit (0);
  }
  wait (0);
  exit (0);
}
// echo hello | xargs echo bye sds