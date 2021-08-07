#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

int
check_name (char *path, char *filename)
{
  char *p;

  // Find first character after last slash.
  for (p = path + strlen (path); p >= path && *p != '/'; p--)
    ;
  p++;
  // Return blank-padded name.
  if (strlen (p) >= DIRSIZ)
    return 0;
  return !strcmp (p, filename);
}

void
traversal (char *path, char *filename)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  //   printf ("[debug]%s\n", path);
  if ((fd = open (path, 0)) < 0) {
    fprintf (2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat (fd, &st) < 0) {
    fprintf (2, "find: cannot stat %s\n", path);
    close (fd);
    return;
  }

  switch (st.type) {
  case T_FILE:
    if (check_name (path, filename)) {
      printf ("%s\n", path);
    }
    break;

  case T_DIR:
    if (strlen (path) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf ("find: path too long\n");
      break;
    }
    strcpy (buf, path);
    p = buf + strlen (buf);
    *p++ = '/';
    while (read (fd, &de, sizeof (de)) == sizeof (de)) {
      if (de.inum == 0)
        continue;
      if (!strcmp (de.name, ".") || !strcmp (de.name, "..")) {
        continue;
      }
      memmove (p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      traversal (buf, filename);
    }
    break;
  }
  close (fd);
}

int
main (int argc, char *argv[])
{
  if (argc != 3) {
    printf ("find : find needs 2 arguments\n");
    exit (0);
  }
  traversal (argv[1], argv[2]);
  exit (0);
}