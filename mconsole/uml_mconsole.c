#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <readline/readline.h>
#include <readline/history.h>

static char uml_name[11];
static struct sockaddr_un sun;

static int do_switch(char *name)
{
  struct stat buf;

  if(stat(name, &buf) == -1){
    fprintf(stderr, "Couldn't stat file: %s - ", name);
    perror("");
    return(-1);
  }
  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path, name);
  if(sscanf(name, "/tmp/uml/%10[^/]/mconsole", uml_name) < 1){
    printf("Couldn't determine UML name from '%s'\n", name);
    strcpy(uml_name, "[Unknown]");
  }
  return(0);
}

static int switch_common(char *name)
{
  char file[MAXPATHLEN];
  int try_file = 1;

  sprintf(file, "/tmp/uml/%s/mconsole", name);
  if(strncmp(name, "/tmp/uml/", strlen("/tmp/uml/"))){
    if(!do_switch(file)) return(0);
    try_file = 0;
  }
  if(!do_switch(name)) return(0);
  if(!try_file) return(-1);
  return(do_switch(file));
}

static void default_cmd(int fd, char *command)
{
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  struct ucred *credptr;
  char anc[CMSG_SPACE(sizeof(*credptr))];
  char buf[256], name[128];
  int n;

  if((sscanf(command, "%128[^: \f\n\r\t\v]:", name) == 1) && 
     (strchr(command, ':') != NULL)){
    if(switch_common(name)) return;
    command = strchr(command, ':');
    *command++ = '\0';
    while(isspace(*command)) command++;
  }

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
    fprintf(stderr, "Sending command to '%s' : ", sun.sun_path);
    perror("");
    return;
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
    return;
  }
  buf[n] = '\0';
  printf("%s\n", buf);
}

static void help_cmd(int fd, char *command)
{
  default_cmd(fd, command);
  printf("Additional local mconsole commands:\n");
  printf("    quit - Quit mconsole\n");
  printf("    switch <socket-name> - Switch control to the given machine\n");
}

static void switch_cmd(int fd, char *command)
{
  char *ptr;

  ptr = &command[strlen("switch")];
  while(isspace(*ptr)) ptr++;
  if(switch_common(ptr)) return;
  printf("Switched to '%s'\n", ptr);
}

static void quit_cmd(int fd, char *command)
{
  exit(0);
}

struct cmd {
  char *command;
  void (*proc)(int, char *);
};

static struct cmd cmds[] = {
  { "quit", quit_cmd },
  { "help", help_cmd },
  { "switch", switch_cmd },
  { NULL, default_cmd }
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
  strcpy(uml_name, "[None]");
  sock = argv[1];
  switch_common(sock);

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
    char *command, *ptr, prompt[1 + sizeof(uml_name) + 2 + 1];
    int i;

    sprintf(prompt, "(%s) ", uml_name);
    command = readline(prompt);
    if(command == NULL) break;

    if(*command) add_history(command);

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
