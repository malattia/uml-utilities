/* Copyright 2001 Jeff Dike and others
 * Licensed under the GPL
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "output.h"
#include "host.h"

int do_exec(char **args, int need_zero, struct output *output)
{
  int pid, status, fds[2], n;
  char buf[256], **arg;

  if(output){
    add_output(output, "*", -1);
    for(arg = args; *arg; arg++){
      add_output(output, " ", -1);
      add_output(output, *arg, -1);
    }
    add_output(output, "\n", -1);
    if(pipe(fds) < 0){
      perror("Creating pipe");
      output = NULL;
    }
  }
  if((pid = fork()) == 0){
    if(output){
      close(fds[0]);
      if((dup2(fds[1], 1) < 0) || (dup2(fds[1], 2) < 0)) perror("dup2");
    }
    execvp(args[0], args);
    fprintf(stderr, "Failed to exec '%s'", args[0]);
    perror("");
    exit(1);
  }
  else if(pid < 0){
    perror("fork failed");
    return(-1);
  }
  if(output){
    close(fds[1]);
    while((n = read(fds[0], buf, sizeof(buf))) > 0) add_output(output, buf, n);
    if(n < 0) perror("Reading command output");
  }
  if(waitpid(pid, &status, 0) < 0){
    perror("execvp");
    return(-1);
  }
  if(need_zero && (!WIFEXITED(status) || (WEXITSTATUS(status) != 0))){
    printf("'%s' didn't exit with status 0\n", args[0]);
    return(-1);
  }
  return(0);
}

int route_and_arp(char *dev, char *addr, int need_route, struct output *output)
{
  char echo[sizeof("echo 1 > /proc/sys/net/ipv4/conf/XXXXXXXXXX/proxy_arp")];
  char *echo_argv[] = { "bash", "-c", echo, NULL };
  char *route_argv[] = { "route", "add", "-host", addr, "dev", dev, NULL };
  char *arp_argv[] = { "arp", "-Ds", addr, "eth0",  "pub", NULL };

  if(do_exec(route_argv, need_route, output)) return(-1);
  sprintf(echo, "echo 1 > /proc/sys/net/ipv4/conf/%s/proxy_arp", dev);
  do_exec(echo_argv, 0, output);
  do_exec(arp_argv, 0, output);
  return(0);
}

int no_route_and_arp(char *dev, char *addr, struct output *output)
{
  char echo[sizeof("echo 0 > /proc/sys/net/ipv4/conf/XXXXXXXXXX/proxy_arp")];
  char *no_echo_argv[] = { "bash", "-c", echo, NULL };
  char *no_route_argv[] = { "route", "del", "-host", addr, "dev", dev, NULL };
  char *no_arp_argv[] = { "arp", "-i", "eth0", "-d", addr, "pub", NULL };

  do_exec(no_route_argv, 0, output);
  snprintf(echo, sizeof(echo), 
	   "echo 0 > /proc/sys/net/ipv4/conf/%s/proxy_arp", dev);
  do_exec(no_echo_argv, 0, output);
  do_exec(no_arp_argv, 0, output);
  return(0);
}

void forward_ip(struct output *output)
{
  char *forw_argv[] = { "bash",  "-c", 
			"echo 1 > /proc/sys/net/ipv4/ip_forward", NULL };
  do_exec(forw_argv, 0, output);
}

void address_change(struct addr_change *change, char *dev, 
		    struct output *output)
{
  char addr[sizeof("255.255.255.255\0")];

  sprintf(addr, "%d.%d.%d.%d", change->addr[0], change->addr[1], 
	  change->addr[2], change->addr[3]);
  switch(change->what){
  case ADD_ADDR:
    route_and_arp(dev, addr, 0, output);
    break;
  case DEL_ADDR:
    no_route_and_arp(dev, addr, output);
    break;
  default:
    fprintf(stderr, "address_change - bad op : %d\n", change->what);
    return;
  }
}

/* This is a routine to do a 'mknod' on the /dev/tap<n> if possible:
 * Return: 0 is ok, -1=already open, etc.
 */

int mk_node(char *devname, int major, int minor)
{
  struct stat statval;
  int retval;

  /* first do a stat on the node to see whether it exists and we
   * had some other reason to fail:
   */
  retval = stat(devname, &statval);
  if(retval == 0) return(0);
  else if(errno != ENOENT){
    /* it does exist. We are just going to return -1, 'cause there
     * was some other problem in the open :-(.
     */
    return -1;
  }

  /* It doesn't exist. We can create it. */

  return(mknod(devname, S_IFCHR|S_IREAD|S_IWRITE, makedev(major, minor)));
}
