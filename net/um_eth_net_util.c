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

#define NEW_CONNECTION		\
              (struct connection_data*)malloc(sizeof(struct connection_data))

#define CLOSE_CONNECTION(y)	\
        FD_CLR(y,&perm);	\
        close(y);		\
        free(uml_connection[y]);\
        uml_connection[y] = NULL;

#define FILL_CONNECTION(y)				\
        uml_connection[y]->stype = SOCKET_CONNECTION;	\
        uml_connection[y]->net_num = UM_ETH_NET_DEFAULT_NUM;	\
        if(y > high_fd) high_fd = y;			\
        FD_SET(y,&perm);

struct connection_data* uml_connection[1024];
int debug = 1;

void dump_packet(unsigned char *buffer,int size,int term) {
  int i;
  if(size > 16) size = 16;

  if(term) fprintf(stderr,"packet:");
  for(i=0;i<size;i++) {
    fprintf(stderr," %02x",buffer[i]);
  }
  if(term) fprintf(stderr,"\n");
}

int main(int argc,char **argv) {
  unsigned char hbuf[UM_ETH_NET_MAX_PACKET + UM_ETH_NET_HDR_SIZE+2];
  unsigned int *header = (unsigned int*)hbuf;
  unsigned char *buffer = hbuf + UM_ETH_NET_HDR_SIZE;
  struct connection_data *conn_in;
  struct sockaddr addr;
  struct sockaddr_in *sin;
  fd_set perm,temp;
  int type;
  int high_fd = 0;
  int one = 1;
  int new_fd;
  int result;
  int len;
  int in;

  if(!debug) if(fork()) exit(0);

  memset(uml_connection,0,sizeof(uml_connection));
  memset(&addr,0,sizeof(struct sockaddr));
  FD_ZERO(&perm);

  sin = (struct sockaddr_in*)&addr;
  sin->sin_family = AF_INET;
  sin->sin_port = htons(UM_ETH_NET_PORT);
  sin->sin_addr.s_addr = htonl(INADDR_ANY);

  if((new_fd = socket(PF_INET,SOCK_STREAM,0))<= 0) {
    perror("socket");
    exit(errno);
  }

  if(setsockopt(new_fd,SOL_SOCKET,SO_REUSEADDR,(char*)&one,sizeof(one))<0) {
    perror("setsockopt");
    exit(errno);
  }
  if(bind(new_fd,(struct sockaddr*)sin,sizeof(struct sockaddr_in))<0) {
    perror("bind");
    exit(errno);
  }
  if(listen(new_fd,15)<0) {
    perror("listen");
    exit(errno);
  }

  if((uml_connection[new_fd] = NEW_CONNECTION) == NULL) {
    perror("malloc of listen data");
    exit(errno);
  }
  FILL_CONNECTION(new_fd);
  uml_connection[new_fd]->stype = SOCKET_LISTEN;

  /*
   * check the command line for <phy net_num> pairs
   */
  if(argc > 1) {
    if((argc-1) % 2) {
      fprintf(stderr,"\nInvalid arguments!\n\numl-net [<phy net_num> ...]\n\n");
      exit(-1);
    }
    for(in=1;in<argc;in+=2) {
      if((new_fd = socket_tap_setup(argv[in])) > 0) {
        fprintf(stderr,"TAP: %s\n",argv[in]);
        type = SOCKET_TAP;
      } else if((new_fd = socket_phy_setup(argv[in])) > 0) {
        fprintf(stderr,"PHY: %s\n",argv[in]);
        type = SOCKET_PHY;
      } else {
        continue;
      }

      if((uml_connection[new_fd] = NEW_CONNECTION) == NULL) {
        perror("malloc of uml_connection data");
        close(new_fd);
        continue;
      }

      FILL_CONNECTION(new_fd);
      uml_connection[new_fd]->stype = type;
      uml_connection[new_fd]->net_num = atoi(argv[in+1]);
    }
  }

  while(1) {
loop:
    memcpy(&temp,&perm,sizeof(fd_set));
    if(select(high_fd+1,&temp,NULL,NULL,NULL)>0) {
      for(in=0;in<=high_fd;in++) {
        if((conn_in = uml_connection[in]) == NULL) continue;
        if(!FD_ISSET(in,&temp)) continue;

        switch(conn_in->stype) {
          case SOCKET_LISTEN:
          {
            if(debug) fprintf(stderr,"connect\n");
            new_fd = accept(in,(struct sockaddr*)&addr,&len);
#if 0
            if(fcntl(new_fd,F_SETFL,O_ASYNC|O_NONBLOCK) < 0){
              perror("fcntl of new uml_connection socket");
              break;
            }
#endif
            
            if((uml_connection[new_fd] = NEW_CONNECTION) == NULL) {
              perror("malloc of uml_connection data");
              break;
            }
            FILL_CONNECTION(new_fd);
            break;
          }
          case SOCKET_CONNECTION:
          {
try_again1:
            result = read(in,header,UM_ETH_NET_HDR_SIZE);
            if(result < 0) {
              if(errno == EINTR || errno == EAGAIN) {
                fprintf(stderr,"again1\n");
                goto try_again1;
              }
              fprintf(stderr,"close\n");
              CLOSE_CONNECTION(in);
              goto loop;
            } else if(result == 0) {
              fprintf(stderr,"close\n");
              CLOSE_CONNECTION(in);
              goto loop;
            } else {

try_again2:
              result = read(in,buffer,ntohl(header[1]));
              if(result < 0) {
                if(errno == EINTR || errno == EAGAIN) {
                  fprintf(stderr,"again2\n");
                  goto try_again2;
                }
                fprintf(stderr,"close\n");
                CLOSE_CONNECTION(in);
                goto loop;
              } else if(result == 0) {
                fprintf(stderr,"close\n");
                CLOSE_CONNECTION(in);
                goto loop;
              } else {
                switch(ntohl(header[0])) {
                  case PACKET_MGMT:
                  {
                    int temp;

                    memcpy(&temp,buffer,sizeof(temp));
                    conn_in->net_num = ntohl(temp);
                    if(debug) fprintf(stderr,"mgmt: %d is now on %d\n",in,conn_in->net_num);
                    break;
                  }
                  case PACKET_DATA:
                  {
                    packet_output(in,hbuf,high_fd);
                    break;
                  }
                }  /* switch(ntohl(header[0])) */
              }
            }
            break;
          }
          case SOCKET_PHY:
          {
try_again3:
            result = read(in,buffer,UM_ETH_NET_MAX_PACKET);
            if(result <= 0) {
              if(errno == EINTR || errno == EAGAIN) {
                fprintf(stderr,"again3\n");
                goto try_again3;
              }
              if(debug) fprintf(stderr,"phy close\n");
              CLOSE_CONNECTION(in);
              goto loop;
            } else {
              header[0] = htonl(PACKET_DATA);
              header[1] = htonl(result);
              packet_output(in,hbuf,high_fd);
            }
            break;
          }
          case SOCKET_TAP:
          {
try_again4:
            result = read(in,buffer,UM_ETH_NET_MAX_PACKET+2);
            if(result <= 0) {
              if(errno == EINTR || errno == EAGAIN) {
                fprintf(stderr,"again4\n");
                goto try_again4;
              }
              if(debug) fprintf(stderr,"tap close\n");
              CLOSE_CONNECTION(in);
              goto loop;
            } else {

              result -= 2;
              memmove(buffer,buffer+2,result);
 
              header[0] = htonl(PACKET_DATA);
              header[1] = htonl(result);
              packet_output(in,hbuf,high_fd);
            }
            break;
          }
        }				/* switch(conn_in->stype) */
      }					/* for(in=0;in<=high_fd;in++) */
    }
  }					/* while(1) */
  return 0;
}
