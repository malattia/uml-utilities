#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <readline/readline.h>
#include <readline/history.h>

static struct sockaddr_un sun;

static void do_default(int fd, char *command)
{
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  struct ucred *credptr;
  char anc[CMSG_SPACE(sizeof(*credptr))];
  char buf[256];
  int n;

  iov.iov_base = command;
  iov.iov_len = strlen(command);

  msg.msg_control = anc;
  msg.msg_controllen = sizeof(anc);

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_CREDENTIALS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(*credptr));
  credptr = (struct ucred *) CMSG_DATA(cmsg);
  credptr->pid = getpid();
  credptr->uid = getuid();
  credptr->gid = getgid();

  msg.msg_name = &sun;
  msg.msg_namelen = sizeof(sun);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_controllen = cmsg->cmsg_len;
  msg.msg_flags = 0;

  if(sendmsg(fd, &msg, 0) < 0){
    perror("sendmsg");
    exit(1);
  }

  iov.iov_base = buf;
  iov.iov_len = sizeof(buf);

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  n = recvmsg(fd, &msg, 0);
  if(n < 0){
    perror("recvmsg");
    exit(1);
  }
  buf[n] = '\0';
  printf("%s\n", buf);
}

static void do_help(int fd, char *command)
{
  do_default(fd, command);
  printf("Additional local mconsole commands:\n");
  printf("    quit - Quit mconsole\n");
  printf("    switch <socket-name> - Switch control to the given machine\n");
}

static void do_switch(int fd, char *command)
{
  struct stat sbuf;
  char *ptr;

  ptr = &command[strlen("switch")];
  while(isspace(*ptr)) ptr++;
  if(stat(ptr, &sbuf) == -1){
    fprintf(stderr, "Couldn't stat file: %s - ", ptr);
    perror("");
  }
  else {
    strcpy(sun.sun_path, ptr);
    printf("Switched to '%s'\n", ptr);
  }
}

static void do_quit(int fd, char *command)
{
  exit(0);
}

struct cmd {
  char *command;
  void (*proc)(int, char *);
};

static struct cmd cmds[] = {
  { "quit", do_quit },
  { "help", do_help },
  { "switch", do_switch },
  { NULL, do_default }
};

static void Usage(void)
{
  fprintf(stderr, "Usage : uml_mconsole socket-name\n");
  exit(1);
}

int main(int argc, char **argv)
{
  struct sockaddr_un here;
  char *sock;
  int fd;

  if(argc < 2) Usage();
  sock = argv[1];
  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path, sock);

  if((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0){
    perror("socket");
    exit(1);
  }
  here.sun_family = AF_UNIX;
  memset(here.sun_path, 0, sizeof(here.sun_path));
  sprintf(&here.sun_path[1], "%5d", getpid());
  if(bind(fd, (struct sockaddr *) &here, sizeof(here)) < 0){
    perror("bind");
    exit(1);
  }

  while(1){
    char *command, *ptr;
    int i;

    command = readline("(mconsole) ");
    if(command == NULL) break;

    /* Trim trailing spaces left by readline's filename completion */
    ptr = &command[strlen(command) - 1];
    while(isspace(*ptr)) *ptr-- = '\0';

    for(i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++){
      if((cmds[i].command == NULL) || 
	 !strncmp(cmds[i].command, command, strlen(cmds[i].command))){
	(*cmds[i].proc)(fd, command);
	break;
      }
    }
    free(command);
  }
  printf("\n");
  return(0);
}
