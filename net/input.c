#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "um_eth.h"

static unsigned char hbuf[UM_ETH_NET_MAX_PACKET + UM_ETH_NET_HDR_SIZE + 2];
static unsigned int *header = (unsigned int*)hbuf;
static unsigned char *buffer = hbuf + UM_ETH_NET_HDR_SIZE;

/*
char packets[10][UM_ETH_NET_MAX_PACKET + UM_ETH_NET_HDR_SIZE + 2];
int packet_index = 0;
*/

int nb_read(int fd, void* buf, int tot, int flag) {
  int result;
  int len = 0;
  int remain = tot;

try_again:
  result = read(fd,buf+len,remain);
  if(result < 0) {
    if(errno == EINTR || errno == EAGAIN) {
      /*      fprintf(stderr,"read again\n");*/
      goto try_again;
    }
    return -1;
  }
  if(!result) {
    return -1;
  }
  if(flag && (result < remain)) {
    remain -= result;
    len += result;
    flag = 1;
    goto try_again;
  }
  /*  memcpy(packets[packet_index++], hbuf, len + result);
   *  if(packet_index == 10) packet_index = 0;
   */
  return len+result;
}

int packet_input(struct connection_data *conn_in) {
  int in = conn_in->fd;
  struct sockaddr addr;
  int len;
  int new_fd;
  int result = -1;

  switch(conn_in->stype) {
    case SOCKET_LISTEN:
    {
      if(debug) fprintf(stderr,"connect\n");
      new_fd = accept(in,(struct sockaddr*)&addr,&len);
      fcntl(new_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
      if((uml_connection[new_fd] = NEW_CONNECTION) == NULL) {
        perror("malloc of uml_connection data");
	return -1;
      }
      FILL_CONNECTION(new_fd);
      return 0;
    }
    case SOCKET_CONNECTION:
    {
      result = nb_read(in,header,UM_ETH_NET_HDR_SIZE,1);
      if(result < 0) {
        if(debug) fprintf(stderr,"socket close\n");
        CLOSE_CONNECTION(in);
        return -1;
      }
      if(ntohl(header[1]) > UM_ETH_NET_MAX_PACKET + 2){
	fprintf(stderr, "big packet came in");
      }
      result = nb_read(in,buffer,ntohl(header[1]),1);
      if(result < 0) {
        if(debug) fprintf(stderr,"socket close\n");
        CLOSE_CONNECTION(in);
        return -1;
      }
      break;
    }
    case SOCKET_TAP:
#ifndef TUNTAP
    {
      result = nb_read(in,buffer-2,UM_ETH_NET_MAX_PACKET+2,0);
      if(result < 0) {
        if(debug) fprintf(stderr,"tap close\n");
        CLOSE_CONNECTION(in);
        return -1;
      }
      header[0] = htonl(PACKET_DATA);
      header[1] = htonl(result-2);
      break;
    }
#endif
    case SOCKET_PHY:
    {
      result = nb_read(in,buffer,UM_ETH_NET_MAX_PACKET,0);
      if(result < 0) {
        if(debug) fprintf(stderr,"phy close\n");
        CLOSE_CONNECTION(in);
        return -1;
      }
      header[0] = htonl(PACKET_DATA);
      header[1] = htonl(result);
      break;
    }
  }				/* switch(conn_in->stype) */

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
      if(debug > 3) {
        fprintf(stderr,"%02d %02d <- ",in,conn_in->net_num);
        dump_packet(buffer,header[1],1);
      }
      packet_output(in,hbuf);
      break;
    }
  }				/* switch(ntohl(header[0])) */
  return 0;
}
