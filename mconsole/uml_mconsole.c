/* Copyright 2001 Jeff Dike and others
 * Licensed under the GPL
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <readline/readline.h>
#include <readline/history.h>

static char uml_name[11];
static struct sockaddr_un sun;

static int do_switch(char *file, char *name)
{
  struct stat buf;

  if(stat(file, &buf) == -1){
    fprintf(stderr, "Warning: couldn't stat file: %s - ", file);
    perror("");
    return(-1);
  }
  sun.sun_family = AF_UNIX;
  strncpy(sun.sun_path, file, sizeof(sun.sun_path));
  strncpy(uml_name, name, sizeof(uml_name));
  return(0);
}

static int switch_common(char *name)
{
  char file[MAXPATHLEN + 1], dir[MAXPATHLEN + 1], tmp[MAXPATHLEN + 1], *home;
  int try_file = 1;

  if((home = getenv("HOME")) != NULL){
    snprintf(dir, sizeof(dir), "%s/.uml", home);
    snprintf(file, sizeof(file), "%s/%s/mconsole", dir, name);
    if(strncmp(name, dir, strlen(dir))){
      if(!do_switch(file, name)) return(0);
      try_file = 0;
    }
  }

  snprintf(tmp, sizeof(tmp), "/tmp/uml/%s/mconsole", name);
  if(strncmp(name, "/tmp/uml/", strlen("/tmp/uml/"))){
    if(!do_switch(tmp, name)) return(0);
  }

  if(!do_switch(name, name)) return(0);
  if(!try_file) return(-1);
  return(do_switch(file, name));
}

#define MCONSOLE_MAGIC (0xcafebabe)
#define MCONSOLE_MAX_DATA (512)
#define MCONSOLE_VERSION (2)

#define MIN(a,b) ((a)<(b) ? (a):(b))

struct mconsole_request {
	uint32_t magic;
	uint32_t version;
	uint32_t len;
	char data[MCONSOLE_MAX_DATA];
};

struct mconsole_reply {
	uint32_t err;
	uint32_t more;
	uint32_t len;
	char data[MCONSOLE_MAX_DATA];
};

static char *absolutize(char *to, int size, char *from)
{
  char save_cwd[MAXPATHLEN + 1], *slash;
  int remaining;

  if(getcwd(save_cwd, sizeof(save_cwd)) == NULL) {
    perror("absolutize : unable to get cwd");
    return(NULL);
  }
  slash = strrchr(from, '/');
  if(slash != NULL){
    *slash = '\0';
    if(chdir(from)){
      *slash = '/';
      fprintf(stderr, "absolutize : Can't cd to '%s'", from);
      perror("");
      return(NULL);
    }
    *slash = '/';
    if(getcwd(to, size) == NULL){
      fprintf(stderr, "absolutize : unable to get cwd of '%s'", from);
      perror("");
      return(NULL);
    }
    remaining = size - strlen(to);
    if(strlen(slash) + 1 > remaining){
      fprintf(stderr, "absolutize : unable to fit '%s' into %d chars\n", from,
	      size);
      return(NULL);
    }
    strcat(to, slash);
  }
  else {
    if(strlen(save_cwd) + 1 + strlen(from) + 1 > size){
      fprintf(stderr, "absolutize : unable to fit '%s' into %d chars\n", from,
	      size);
      return(NULL);
    }
    strcpy(to, save_cwd);
    strcat(to, "/");
    strcat(to, from);
  }
  chdir(save_cwd);
  return(to);
}

static int fix_filenames(char **cmd_ptr)
{
  char *cow_file, *backing_file, *equal, *new, *ptr = *cmd_ptr;
  char full_backing[MAXPATHLEN + 1], full_cow[MAXPATHLEN + 1];
  int len;

  if(strncmp(ptr, "config", strlen("config"))) return(0);
  ptr += strlen("config");

  while(isspace(*ptr) && (*ptr != '\0')) ptr++;
  if(*ptr == '\0') return(0);

  if(strncmp(ptr, "ubd", strlen("ubd"))) return(0);

  while((*ptr != '=') && (*ptr != '\0')) ptr++;
  if(*ptr == '\0') return(0);

  equal = ptr;
  cow_file = ptr + 1;
  while((*ptr != ',') && (*ptr != '\0')) ptr++;
  if(*ptr == '\0'){
    backing_file = cow_file;
    cow_file = NULL;
  }
  else {
    *ptr = '\0';
    backing_file = ptr + 1;
  }

  ptr = absolutize(full_backing, sizeof(full_backing), backing_file);
  backing_file = ptr ? ptr : backing_file;

  if(cow_file != NULL){
    ptr = absolutize(full_cow, sizeof(full_cow), cow_file);
    cow_file = ptr ? ptr : cow_file;
  }

  len = equal - *cmd_ptr;
  len += strlen("=") + strlen(backing_file) + 1;
  if(cow_file != NULL){
    len += strlen(",") + strlen(cow_file);
  }

  new = malloc(len * sizeof(char));
  if(new == NULL) return(0);

  strncpy(new, *cmd_ptr, equal - *cmd_ptr);
  ptr = new + (equal - *cmd_ptr);
  *ptr++ = '=';

  if(cow_file != NULL){
    sprintf(ptr, "%s,", cow_file);
    ptr += strlen(ptr);
  }
  strcpy(ptr, backing_file);

  *cmd_ptr = new;
  return(1);
}

static void default_cmd(int fd, char *command)
{
  struct mconsole_request request;
  struct mconsole_reply reply;
  char name[128];
  int n, free_command;

  if((sscanf(command, "%128[^: \f\n\r\t\v]:", name) == 1) &&
     (*(name + 1) == ':')){
    if(switch_common(name)) return;
    command = strchr(command, ':');
    *command++ = '\0';
    while(isspace(*command)) command++;
  }

  free_command = fix_filenames(&command);

  request.magic = MCONSOLE_MAGIC;
  request.version = MCONSOLE_VERSION;
  request.len = MIN(strlen(command), sizeof(reply.data) - 1);
  strncpy(request.data, command, request.len);
  request.data[request.len] = '\0';

  if(free_command) free(command);

  if(sendto(fd, &request, sizeof(request), 0, (struct sockaddr *) &sun, 
	    sizeof(sun)) < 0){
    fprintf(stderr, "Sending command to '%s' : ", sun.sun_path);
    perror("");
    return;
  }
  
  do {
    int len = sizeof(sun);
    n = recvfrom(fd, &reply, sizeof(reply), 0, (struct sockaddr *) &sun, &len);
    if(n < 0){
      perror("recvmsg");
      return;
    }
    if(reply.err) printf("ERR ");
    else printf("OK ");
    printf("%s", reply.data);
  } while(reply.more);

  printf("\n");
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

/* sends a command */
int issue_command(int fd, char *command)
{
  char *ptr;
  int i;

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
    
  /* in future, return command status */
  return 0;
}

/* sends a command in argv style array */
int issue_commandv(int fd, char **argv)
{
  char *command;
  int len, i, status;

  len = 1;  /* space for trailing null */
  for(i = 0; argv[i] != NULL; i++)
    len += strlen(argv[i]) + 1;  /* space for space */

  command = malloc(len);
  if(command == NULL){
    perror("issue_command");
    return(-1);
  }
  command[0] = '\0';

  for(i = 0; argv[i] != NULL; i++) {
    strcat(command, argv[i]);
    if(argv[i+1] != NULL) strcat(command, " ");
  }

  status = issue_command(fd, command);

  free(command);

  return status;
}

static void Usage(void)
{
  fprintf(stderr, "Usage : uml_mconsole socket-name [command]\n");
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

  if(argc>2)
    return issue_commandv(fd, argv+2);

  while(1){
    char *command, prompt[1 + sizeof(uml_name) + 2 + 1];

    sprintf(prompt, "(%s) ", uml_name);
    command = readline(prompt);
    if(command == NULL) break;

    if(*command) add_history(command);

    issue_command(fd, command);
    free(command);
  }
  printf("\n");
  return(0);
}
