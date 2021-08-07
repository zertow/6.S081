#include "kernel/types.h"
#include "user/user.h"

#define PRIME_MAX_N 35
typedef char bool;

// int child[PRIME_MAX_N + 1];
// int is_prime[PRIME_MAX_N + 1];
// int p[2];
// char recv_buf[5];

// int
// main (int argc, char *argv[])
// {
//   int i, j, pid, child_ptr = 2;
//   bool check = 0;
//   for (i = 2; i <= PRIME_MAX_N; i++) {
//     pipe (p);
//     if ((pid = fork ()) == 0) {
//       //子进程
//       wait (0);
//       read (p[0], recv_buf, 4);
//       close (p[0]);
//       check = 0;
//       for (j = 2; j < i; j++) {
//         if (i % j == 0) {
//           check = 1;
//           break;
//         }
//       }
//       fprintf (p[1], "%d", !check);
//     //   printf ("%d %d %d\n", p[1], i, !check);
//       close (p[1]);
//       exit (0);
//     } else {
//       fprintf (p[1], "%d", i);
//       //不关闭读通道
//       close (p[1]);
//       is_prime[child_ptr] = p[0];
//       child[child_ptr] = pid;
//     }
//     child_ptr++;
//   }
//   wait (child);
//   for (i = 2; i < PRIME_MAX_N; i++) {
//     read (is_prime[i], recv_buf, 4);
//     if (atoi (recv_buf)) {
//       printf ("prime %d\n", i);
//     }
//     close (is_prime[child_ptr]);
//   }
//   exit (0);
// }

char read_buf[4];
int p[2];
void
prime ()
{
  int count, m;
  bool is_create_child = 0;
  read (0, read_buf, 4);
  int n = *(int *)read_buf;
  printf ("prime %d\n", n);

  while ((count = read (0, read_buf, 4) != 0)) {
    m = *(int *)read_buf;
    if (m % n != 0) {
      if (!is_create_child) {
        pipe (p);
        if (fork () == 0) {
          close (0);
          dup (p[0]);
          close (p[0]);
          close (p[1]);
          prime ();
          exit (0);
        } else {
          close (p[0]);
          close (1);
          dup (p[1]);
          close (p[1]);
        }
        is_create_child = 1;
      }
      write (1, &m, 4);
    }
  }
  close (0);
  close (1);
  wait (0);
}

int
main (int argc, char *argv)
{
  int i, pid;
  pipe (p);
  if ((pid = fork ()) == 0) {
    close (p[1]);
    close (0);
    dup (p[0]);
    close (p[0]);
    prime ();
    exit (0);
  } else {
    close (p[0]);
    for (i = 2; i < PRIME_MAX_N; i++) {
      write (p[1], &i, 4);
    }
    close (p[1]);
  }
  wait (0);
  exit (0);
}