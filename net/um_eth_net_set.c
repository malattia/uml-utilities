#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <stdlib.h>
#include "um_eth_net.h"

int main(int argc,char **argv) {
  struct ifreq ifr;
  int fd;

  if(argc != 3 ) {
    fprintf(stderr,"\numl-set ifname net_num\n\n");
    exit(-1);
  }

  if((fd = socket(PF_INET,SOCK_STREAM,0))<= 0) {
    perror("socket");
    exit(errno);
  }

  strcpy(ifr.ifr_name,argv[1]);
  ifr.ifr_flags = atoi(argv[2]);

  if(ioctl(fd,SIOCDEVPRIVATE,&ifr)<0) {
    perror("ioctl");
    exit(errno);
  }
  return 0;
}
