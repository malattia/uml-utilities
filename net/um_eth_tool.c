#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
  struct ifreq ifr;
  char *temp;
  int fd;

  memset(&ifr,0,sizeof(struct ifreq));

  if(argc < 3 || argc > 4 ) {
    fprintf(stderr,"\numl-set ifname net_num [ip:port|path]\n\n");
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

  if(argc == 4) {
    if((temp = index(argv[3],':'))) {
      struct sockaddr_in *sin = (struct sockaddr_in*)&ifr.ifr_dstaddr;
      char *ip_str = argv[3];
      char *port_str = temp + 1;
      *temp = '\0';

      sin->sin_family = AF_INET;
      sin->sin_port = atoi(port_str);
      sin->sin_addr.s_addr = inet_addr(ip_str);

      printf("%s will connect to AF_INET server at %s:%s\n",argv[1],ip_str,
        port_str);
    } else {
      struct sockaddr_un *sun = (struct sockaddr_un*)&ifr.ifr_dstaddr;

      sun->sun_family = AF_UNIX;
      strcpy(sun->sun_path,argv[3]);
      printf("%s will connect to AF_UNIX server at %s\n",argv[1],argv[3]);
    }

    if(ioctl(fd,SIOCDEVPRIVATE+1,&ifr)<0) {
      perror("ioctl");
      exit(errno);
    }
  }
  return 0;
}
