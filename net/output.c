#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include "um_eth.h"

int nb_write(int fd, void* buf, int tot) {
  int len = 0;
  int remain = tot;
  int retval;

try_again:
  retval = write(fd,buf+len,remain);
  if(retval < 0) {
    if(errno == EAGAIN || errno == EINTR) {
      fprintf(stderr,"out again\n");
      goto try_again;
    }
    return -1;
  }
  if(retval < remain) {
    len += retval;
    remain -= retval;
    goto try_again;
  }
  return 0;
}

int packet_output(int in,unsigned char *hbuf) {
  struct connection_data *conn_in,*conn_out;
  unsigned int *hdr = (unsigned int*)hbuf;
  unsigned char *buf = hbuf + UM_ETH_NET_HDR_SIZE;
#ifndef TUNTAP
  unsigned char tapbuf[UM_ETH_NET_HDR_SIZE + 2 + UM_ETH_NET_MAX_PACKET];
#endif
  int count = 0;
  int out;
  void* out_buf = NULL;
  int out_size = 0;

  conn_in = uml_connection[in];

  if(debug > 2) {
    fprintf(stderr,"%02d %02d ->",in,conn_in->net_num);
    dump_packet(buf,ntohl(hdr[1]),0);
    fprintf(stderr," ->");
  }

  for(out=0;out<=high_fd;out++) {
    conn_out = uml_connection[out];
    if(conn_out == NULL) continue;

    if((out != in) &&
      (conn_out->net_num == conn_in->net_num)) {

      if(debug > 1 && conn_out->stype != SOCKET_LISTEN)
        fprintf(stderr," %02d",out);
      switch(conn_out->stype) {
        case SOCKET_CONNECTION:
        {
	  out_buf = hbuf;
          out_size = ntohl(hdr[1]) + UM_ETH_NET_HDR_SIZE;
          break;
        }
        case SOCKET_TAP:
#ifndef TUNTAP
	/* when sending on the old TAP device you need to
         * add 2 byte to comply with the DIX format
         */
        {
          memcpy(tapbuf+2,buf,ntohl(hdr[1]));
          memset(tapbuf,0,2);
          out_buf = tapbuf;
          out_size = ntohl(hdr[1])+2;
          break;
        }
#endif
        case SOCKET_PHY:
        {
          out_buf = buf;
          out_size = ntohl(hdr[1]);
          break;
        }
        case SOCKET_LISTEN:
        default:
          continue;
      }

      if(nb_write(out,out_buf,out_size)) {
        close(out);
      }
      count++;
    }
  }
  if(debug > 1) fprintf(stderr,"\n");
  return count;
}
