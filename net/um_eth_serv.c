#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/un.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/socket.h>
#include <errno.h>
#include <unistd.h>
#include "um_eth.h"

struct connection_data* uml_connection[1024];
int debug = 1;
int high_fd = 0;
fd_set perm;
const char* const unix_socket = "/tmp/um_eth";

static void sig_int(int dummy) {
  unlink(unix_socket);
  exit(0);
}

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
  struct connection_data *conn_in;
  struct sockaddr addr;
  struct sockaddr_in *sin;
  struct sockaddr_un *sun;
  fd_set temp;
  int type;
  int one = 1;
  int new_fd;
  int in;

  if(!debug) if(fork()) exit(0);

  /* Catch SIGINT so we can shutdown cleanly. */
  {
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags   = 0;
    action.sa_handler = sig_int;
    if (sigaction(SIGINT, &action, 0) < 0)
    {
        assert(0 && "Couldn't register sig_int");
    }
  }

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

  sun = (struct sockaddr_un*)&addr;
  sun->sun_family = AF_UNIX;
  sprintf(sun->sun_path,unix_socket);

  if((new_fd = socket(PF_UNIX,SOCK_STREAM,0))<= 0) {
    perror("socket");
    exit(errno);
  }
  if(setsockopt(new_fd,SOL_SOCKET,SO_REUSEADDR,(char*)&one,sizeof(one))<0) {
    perror("setsockopt");
    exit(errno);
  }
  if(bind(new_fd,(struct sockaddr*)sun,sizeof(struct sockaddr_un))<0) {
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
    memcpy(&temp,&perm,sizeof(fd_set));
    if(select(high_fd+1,&temp,NULL,NULL,NULL)>0) {
      for(in=0;in<=high_fd;in++) {
        if((conn_in = uml_connection[in]) == NULL) continue;
        if(!FD_ISSET(in,&temp)) continue;
	packet_input(conn_in);
      }
    }
  }
  return 0;
}
