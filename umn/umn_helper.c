#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>

int main(int argc, char **argv)
{
  char *args[] = { "ifconfig", NULL, NULL, "pointopoint", NULL, "up", NULL };
  char slip_name[sizeof("slxxxx")], *addr = argv[2], *ptp_addr = argv[3];
  char *path, *newpath;
  int fd = atoi(argv[1]), disc, sencap, n;

  disc = N_SLIP;
  sencap = 0;
  if((n = ioctl(fd, TIOCSETD, &disc)) < 0){
    perror("Setting slip line discipline");
    exit(1);
  }
  if(ioctl(fd, SIOCSIFENCAP, &sencap) < 0){
    perror("Setting slip encapsulation");
    exit(1);
  }
  path = getenv("PATH");
  if(path == NULL) newpath = "/sbin:/usr/sbin";
  else {
    newpath = malloc(strlen(path) + strlen(":/sbin:/usr/sbin") + 1);
    if(newpath != NULL) sprintf(newpath, "%s:/sbin:/usr/sbin", path);
  }
  if(newpath != NULL) setenv("PATH", newpath, 1);
  sprintf(slip_name, "sl%d", n);
  args[1] = slip_name;
  args[2] = addr;
  args[4] = ptp_addr;
  execvp(args[0], args);
}
