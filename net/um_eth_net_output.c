#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include "um_eth_net.h"

extern void dump_packet(unsigned char *buffer,int size,int term);

extern struct connection_data* uml_connection[];
extern int debug;

int packet_output(int in,unsigned char *hbuf,int max_fd) {
  struct connection_data *conn_in,*conn_out;
  unsigned int *hdr = (unsigned int*)hbuf;
  unsigned char *buf = hbuf + UM_ETH_NET_HDR_SIZE;
  int retval = 0;
  int count = 0;
  int out;

  conn_in = uml_connection[in];

  if(debug) {
    fprintf(stderr,"%02d %02d ->",in,conn_in->net_num);
    dump_packet(buf,ntohl(hdr[1]),0);
    fprintf(stderr," ->");
  }

  for(out=0;out<=max_fd;out++) {
    conn_out = uml_connection[out];
    if(conn_out == NULL) continue;

    if((out != in) &&
      (conn_out->net_num == conn_in->net_num)) {

      if(debug && conn_out->stype != SOCKET_LISTEN) fprintf(stderr," %02d",out);
      switch(conn_out->stype) {
        case SOCKET_LISTEN:
        { break; }
        case SOCKET_CONNECTION:
        {
try_again1:
          retval = write(out,hbuf,ntohl(hdr[1]) + UM_ETH_NET_HDR_SIZE);
          if(retval <= 0) {
            if(errno == EAGAIN || errno == EINTR) {
              fprintf(stderr,"out again1\n");
              goto try_again1;
            }
            close(out);
          }
          count++;
          break;
        }
        case SOCKET_PHY:
        {
try_again2:
          retval = write(out,buf,ntohl(hdr[1]));
          if(retval <= 0) {
            if(errno == EAGAIN || errno == EINTR) {
              fprintf(stderr,"out again2\n");
              goto try_again2;
            }
            close(out);
          }
          count++;
          break;
        }
        case SOCKET_TAP:
        {
try_again3:
          memmove(buf+2,buf,ntohl(hdr[1]));
          retval = write(out,buf,ntohl(hdr[1])+2);
          if(retval <= 0) {
            if(errno == EAGAIN || errno == EINTR) {
              fprintf(stderr,"out again3\n");
              goto try_again3;
            }
            close(out);
          }
          count++;
          break;
        }
      }
    }
  }
  if(debug) fprintf(stderr,"\n");
  return count;
}
