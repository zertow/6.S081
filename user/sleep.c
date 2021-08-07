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