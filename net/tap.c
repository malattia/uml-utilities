#include <fcntl.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#ifdef TUNTAP
#include <linux/if_tun.h>
#endif
#include <errno.h>
#include "um_eth.h"

int socket_tap_setup(char *name) {
  struct ifreq ifr;
  int retval = -EINVAL;
  int new_fd;

  if(!strncmp(name,"tap",3)) {
#ifdef TUNTAP
    if((new_fd = open("/dev/tun",O_RDWR)) < 0) {
      perror("/dev/tun");
    } else {
      memset(&ifr, 0, sizeof(ifr));


      /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
       *        IFF_TAP   - TAP device
       *
       *        IFF_NO_PI - Do not provide packet information
       */
      ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
      if(*name)
         strncpy(ifr.ifr_name, name, IFNAMSIZ);

      if((retval = ioctl(new_fd,TUNSETIFF,(void *) &ifr)) < 0){
         close(new_fd);
         return retval;
      }
      strcpy(name,ifr.ifr_name);
      retval = new_fd;
    }

#else
  
    sprintf(ifr.ifr_name,"/dev/%s",name);
    if((new_fd = open(ifr.ifr_name,O_RDWR|O_NONBLOCK)) < 0) {
      perror(ifr.ifr_name);
    } else {
      retval = new_fd;
    }
#endif
  }

  return retval;
}
