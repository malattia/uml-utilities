#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static void hup(int sig)
{
  exit(0);
}

static void send_fd(int fd, int target)
{
  char anc[CMSG_SPACE(sizeof(fd))];
  struct msghdr msg;
  struct cmsghdr *cmsg;
  int *fd_ptr;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = NULL;
  msg.msg_iovlen = 0;
  msg.msg_flags = 0;
  msg.msg_control = anc;
  msg.msg_controllen = sizeof(anc);

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

  fd_ptr = (int *) CMSG_DATA(cmsg);
  *fd_ptr = fd;

  msg.msg_controllen = cmsg->cmsg_len;

  if(sendmsg(target, &msg, 0) < 0){
    perror("sendmsg");
    exit(1);
  }
}

int main(int argc, char **argv)
{
  signal(SIGHUP, hup);
  send_fd(0, 3);
  pause();
  return(0);
}
