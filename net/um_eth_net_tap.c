#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <errno.h>
#include "um_eth_net.h"

int socket_tap_setup(char *name) {
  char nbuf[16];
  int retval = -EINVAL;
  int new_fd;

  if(!strncmp(name,"tap",3)) {
    sprintf(nbuf,"/dev/%s",name);
    if((new_fd = open(nbuf,O_RDWR|O_NONBLOCK)) < 0) {
      perror(nbuf);
    } else {
      retval = new_fd;
    }
  }

  return retval;
}
