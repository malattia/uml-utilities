/* Copyright 2002 Jeff Dike
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

static void Usage(void)
{
  fprintf(stderr, "persist uid\n");
  fprintf(stderr, "persist -nopersist device-name\n");
  exit(1);
}

int main(int argc, char **argv)
{
  struct ifreq ifr;
  char *end, *dev = "";
  long owner;
  int tap_fd;

  argc--;
  argv++;
  if(argc == 0) Usage();
  if(!strcmp(*argv, "-nopersist")){
    if(argc < 2) Usage();
    dev = argv[1];
  }
  else if(argc > 0){
    owner = strtol(*argv, &end, 0);
    if(*end != '\0') Usage();
  }
  else Usage();

  if((tap_fd = open("/dev/net/tun", O_RDWR)) < 0){
    perror("opening /dev/net/tun");
    exit(1);
  }
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name) - 1);
  if(ioctl(tap_fd, TUNSETIFF, (void *) &ifr) < 0){
    perror("TUNSETIFF");
    exit(1);
  }
  if(*dev != '\0'){
    if(ioctl(tap_fd, TUNSETPERSIST, 0) < 0){
      perror("TUNSETPERSIST");
      exit(1);
    }    
    printf("Set '%s' nonpersistent\n", ifr.ifr_name);
  }
  else {
    if(ioctl(tap_fd, TUNSETPERSIST, 1) < 0){
      perror("TUNSETPERSIST");
      exit(1);
    }
    if(ioctl(tap_fd, TUNSETOWNER, owner) < 0){
      perror("TUNSETPERSIST");
      exit(1);
    }
    printf("Set '%s' persistent and owned by uid %ld\n", ifr.ifr_name, owner);
  }
  return(0);
}
