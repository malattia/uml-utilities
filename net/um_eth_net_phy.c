#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/socket.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "um_eth_net.h"

int socket_phy_setup(char *name) {
  struct sockaddr_ll sll;
  struct packet_mreq mreq;
  struct ifreq ifr;
  int new_fd;

#if 0
  if(!strncmp(name,"tap",3)) {
    char nbuf[16];
    sprintf(nbuf,"/dev/%s",name);
    new_fd = open(name,O_RDWR);
    return new_fd;
  }
#endif

  if((new_fd = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL))) < 0) {
    perror("socket:PF_PACKET");
    goto socket_phy_setup_exit;
  }

  memset(&sll,0,sizeof(struct sockaddr_ll));
  memset(&mreq,0,sizeof(struct packet_mreq));
  memset(&ifr,0,sizeof(struct ifreq));

  /*
   * Initial setup of info, the rest we will fill in as
   * we learn it
   *
   * sll - is the Link Layer socket, it will contain info about which
   *       interface we want to bind this socket to
   *
   * mreq - is what will tell the kernel which packet to send us on this
   *        socket
   */
  sll.sll_family = AF_PACKET;
  sll.sll_protocol = htons(ETH_P_ALL);
  sll.sll_hatype = ARPHRD_ETHER;
  sll.sll_pkttype = PACKET_OTHERHOST;
  sll.sll_halen = ETH_ALEN;
      
  mreq.mr_type = PACKET_MR_PROMISC;
  mreq.mr_alen = ETH_ALEN;

  /*-------------------- HWADDR ----------------------*/
  strncpy(ifr.ifr_name,name,sizeof(ifr.ifr_name));
  if(ioctl(new_fd,SIOCGIFHWADDR,&ifr) < 0) {
    perror("ioctl:SIOCGIFHWADDR");
    goto socket_phy_setup_fail;
  }
  memcpy(mreq.mr_address,ifr.ifr_hwaddr.sa_data,ETH_ALEN);
  memcpy(sll.sll_addr,ifr.ifr_hwaddr.sa_data,ETH_ALEN);
  /*-------------------- HWADDR ----------------------*/

  /*-------------------- IFINDEX ---------------------*/
  if(ioctl(new_fd,SIOCGIFINDEX,&ifr) < 0 ) {
    perror("ioctl: IFINDEX");
    goto socket_phy_setup_fail;
  }
  mreq.mr_ifindex = ifr.ifr_ifindex;
  sll.sll_ifindex = ifr.ifr_ifindex;
  /*-------------------- IFINDEX ---------------------*/

  /*
   * BIND the socket to the physical interface
   */
  if(bind(new_fd,(struct sockaddr*)&sll,sizeof(struct sockaddr_ll))<0) {
    perror("bind");
    goto socket_phy_setup_fail;
  }

  /*
   * TELL the kernel what we want it to send us from this interface kernel
   */
  if(setsockopt(new_fd,SOL_PACKET,PACKET_ADD_MEMBERSHIP,
    &mreq,sizeof(struct packet_mreq))) {
    perror("setsockopt");
    goto socket_phy_setup_fail;
  }

  /*
   * MARK the interface as being PROMISC
   */
  if(ioctl(new_fd,SIOCGIFFLAGS,&ifr) < 0 ) {
    perror("ioctl: get IFFLAGS");
    goto socket_phy_setup_fail;
  }
  ifr.ifr_flags |= IFF_PROMISC;
  if(ioctl(new_fd,SIOCSIFFLAGS,&ifr) < 0 ) {
    perror("ioclt: set IFFLAGS");
    goto socket_phy_setup_fail;
  }

  return new_fd;

socket_phy_setup_fail:
  close(new_fd);

socket_phy_setup_exit:
  return -errno;
}
