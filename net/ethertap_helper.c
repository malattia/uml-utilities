#include <stddef.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

void do_exec(char **args, int need_zero)
{
  int pid, status;

  if((pid = fork()) == 0){
    execvp(args[0], args);
    exit(1);
  }
  else if(pid < 0){
    perror("fork failed");
    exit(1);
  }
  if(waitpid(pid, &status, 0) < 0){
    perror("execvp");
    exit(1);
  }
  if(need_zero && (!WIFEXITED(status) || (WEXITSTATUS(status) != 0))){
    printf("'%s' didn't exit with status 0\n", args[0]);
    exit(1);
  }
}

#define BUF_SIZE 1500

main(int argc, char **argv)
{
  char *dev = argv[1];
  int fd = atoi(argv[2]);
  char *gate_addr = argv[3];
  char *remote_addr = argv[4];
  int pid, status;
  char *ifconfig_argv[] = { "ifconfig", dev, "arp", gate_addr, "up", NULL };
  char *route_argv[] = { "route", "add", "-host", remote_addr, "gw", 
			 gate_addr, NULL };
  char dev_file[sizeof("/dev/tapxxxx\0")], zero;
  int tap, *fdptr;

  setreuid(0, 0);
  do_exec(ifconfig_argv, 1);
  do_exec(route_argv, 0);

  sprintf(dev_file, "/dev/%s", dev);
  if((tap = open(dev_file, O_RDWR | O_NONBLOCK)) < 0){
    perror("open");
    exit(1);
  }
  
  if(write(fd, &zero, sizeof(zero)) != sizeof(zero)){
    perror("write");
    exit(1);
  }

  while(1){
    fd_set fds, except;
    char buf[BUF_SIZE];
    int from, to, n, max;

    FD_ZERO(&fds);
    FD_SET(tap, &fds);
    FD_SET(fd, &fds);
    except = fds;
    max = ((tap > fd) ? tap : fd) + 1;
    if(select(max, &fds, NULL, &except, NULL) < 0){
      perror("select");
      continue;
    }
    if(FD_ISSET(tap, &fds)){
      from = tap;
      to = fd;
    }
    else if(FD_ISSET(fd, &fds)){
      from = fd;
      to = tap;
    }
    else continue;
    n = read(from, buf, sizeof(buf));
    if(n == 0){
      exit(0);
    }
    else if(n < 0) perror("read");
    n = write(to, buf, n);
    if(n < 0) perror("write");
  }
  return(0);
}
