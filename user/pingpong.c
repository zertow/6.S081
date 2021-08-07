#include "kernel/types.h"
#include "user/user.h"

int
main (int argc, char *argv[])
{
  int p[2];
  char buf = '0';
  pipe (p);

  if (fork () == 0)
    {
      wait (0);
      read (p[0], &buf, 1);
      printf ("%d: received ping\n", getpid ());
      write (p[0], &buf, 1);
    }
  else
    {
      write (p[1], &buf, 1);
      wait (0);
      read (p[1], &buf, 1);
      printf ("%d: received pong\n", getpid ());
    }
  exit (0);
}