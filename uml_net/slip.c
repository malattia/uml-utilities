/* Copyright 2001 Jeff Dike and others
 * Licensed under the GPL
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ioctl.h>
#include "host.h"
#include "output.h"

static void slip_up(char **argv, struct output *output)
{
  int fd = atoi(argv[0]);
  char *gate_addr = argv[1];
  char *remote_addr = argv[2];
  char slip_name[sizeof("slxxxx\0")];
  char *up_argv[] = { "ifconfig", slip_name, gate_addr, "pointopoint", 
		      remote_addr, "mtu", "1500", "up", NULL };
  int disc, sencap, n;
  
  disc = N_SLIP;
  if((n = ioctl(fd, TIOCSETD, &disc)) < 0){
    output_errno(output, "Setting slip line discipline");
    write_output(1, output);
    exit(1);
  }
  sencap = 0;
  if(ioctl(fd, SIOCSIFENCAP, &sencap) < 0){
    output_errno(output, "Setting slip encapsulation");
    write_output(1, output);
    exit(1);
  }
  sprintf(slip_name, "sl%d", n);
  if(do_exec(up_argv, 1, output)){
    write_output(1, output);
    exit(1);
  }
  route_and_arp(slip_name, remote_addr, 0, output);
  forward_ip(output);
  write_output(1, output);
}

static void slip_down(char **argv, struct output *output)
{
  int fd = atoi(argv[0]);
  char *remote_addr = argv[1];
  char slip_name[sizeof("slxxxx\0")];
  char *down_argv[] = { "ifconfig", slip_name, "0.0.0.0", "down", NULL };
  int n, disc;

  if((n = ioctl(fd, TIOCGETD, &disc)) < 0){
    output_errno(output, "Getting slip line discipline");
    write_output(1, output);
    exit(1);
  }
  sprintf(slip_name, "sl%d", n);
  no_route_and_arp(slip_name, remote_addr, output);
  if(do_exec(down_argv, 1, output)){
    write_output(1, output);
    exit(1);
  }
  write_output(1, output);
}

void slip_v0_v2(int argc, char **argv)
{
  char *op = argv[0];

  if(setreuid(0, 0) < 0){
    perror("slip - setreuid failed");
    exit(1);
  }

  if(!strcmp(argv[0], "up")) slip_up(&argv[1], NULL);
  else if(!strcmp(argv[0], "down")) slip_down(&argv[1], NULL);
  else {
    printf("slip - Unknown op '%s'\n", op);
    exit(1);
  }
}

void slip_v3(int argc, char **argv)
{
  struct output output = INIT_OUTPUT;
  char *op = argv[0];

  if(setreuid(0, 0) < 0){
    perror("slip - setreuid failed");
    exit(1);
  }

  if(!strcmp(argv[0], "up")) slip_up(&argv[1], &output);
  else if(!strcmp(argv[0], "down")) slip_down(&argv[1], &output);
  else {
    printf("slip - Unknown op '%s'\n", op);
    exit(1);
  }
}
