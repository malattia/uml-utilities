#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define ETH_ALEN 6

struct connection {
  struct connection *next;
  struct connection *prev;
  int control;
  struct sockaddr_un name;
  unsigned char addr[ETH_ALEN];
};

enum request_type { REQ_NEW_CONTROL };

struct request {
  enum request_type type;
  union {
    struct {
      unsigned char addr[ETH_ALEN];
      struct sockaddr_un name;
    } new_control;
    struct {
      unsigned long cookie;
    } new_data;
  } u;
};

struct packet {
  struct {
    unsigned char dest[6];
    unsigned char src[6];
    unsigned char proto[2];
  } header;
  unsigned char data[1500];
};

struct connection *head = NULL;
fd_set perm_fds;
int max_fd = -1;

static char *ctl_socket = "/tmp/uml.ctl";
static char *data_socket = "/tmp/uml.data";

static void free_connection(struct connection *conn)
{
  close(conn->control);
  FD_CLR(conn->control, &perm_fds);
  if(conn->prev) conn->prev->next = conn->next;
  else head = conn->next;
  if(conn->next) conn->next->prev = conn->prev;
  free(conn);
}

static void service_connection(struct connection *conn)
{
  struct request req;

  if(read(conn->control, &req, sizeof(req)) != sizeof(req)){
    if(errno != 0) perror("Reading request");
    free_connection(conn);
    return;
  }
  switch(req.type){
  default:
    printf("Bad request type : %d\n", req.type);
    free_connection(conn);
  }
}

static int match_addr(unsigned char *host, unsigned char *packet)
{
  if((packet[0] == 0xff) && (packet[1] == 0xff) && (packet[2] == 0xff) && 
     (packet[3] == 0xff) && (packet[4] == 0xff) && (packet[5] == 0xff))
    return(1);
  return((packet[0] == host[0]) && (packet[1] == host[1]) && 
	 (packet[2] == host[2]) && (packet[3] == host[3]) && 
	 (packet[4] == host[4]) && (packet[5] == host[5]));
}

static void handle_data(int fd)
{
  struct packet p;
  struct connection *c, *next;
  int len, n;

  len = recvfrom(fd, &p, sizeof(p), 0, NULL, 0);
  if(len < 0){
    perror("Reading request");
    return;
  }
  for(c = head; c != NULL; c = next){
    next = c->next;
    if(match_addr(c->addr, p.header.dest)){
      n = sendto(fd, &p, len, 0, &c->name, sizeof(c->name));
      if(n != len){
	free_connection(c);
      }
    }
  }
}

static void new_connection(int fd)
{
  struct request req;
  struct connection *conn;

  if(read(fd, &req, sizeof(req)) != sizeof(req)){
    if(errno != 0) perror("Reading initial request");
    close(fd);
    return;
  }
  switch(req.type){
  case REQ_NEW_CONTROL:
    conn = malloc(sizeof(struct connection));
    if(conn == NULL){
      perror("malloc");
      close(fd);
      return;
    }
    conn->next = head;
    if(head) head->prev = conn;
    conn->prev = NULL;
    conn->control = fd;
    memcpy(conn->addr, req.u.new_control.addr, sizeof(conn->addr));
    conn->name = req.u.new_control.name;
    head = conn;
    break;
  default:
    printf("Bad request type : %d\n", req.type);
    close(fd);
  }
}

void handle_connections(fd_set *fds, int max)
{
  struct connection *c;
  int i;

  for(i=0;i<max;i++){
    if(FD_ISSET(i, fds)){
      for(c = head; c != NULL; c = c->next){
	if(c->control == i){
	  service_connection(c);
	  break;
	}
      }
      if(c == NULL) new_connection(i);
    }
  }
}

void accept_connection(int fd)
{
  struct sockaddr addr;
  int len, new;

  new = accept(fd, &addr, &len);
  if(new < 0){
    perror("accept");
    return;
  }
  if(fcntl(new, F_SETFL, O_ASYNC | O_NONBLOCK) < 0){
    perror("fcntl - setting O_ASYNC and O_NONBLOCK");
    close(new);
    return;
  }
  if(new > max_fd) max_fd = new;
  FD_SET(new, &perm_fds);
}

int still_used(struct sockaddr_un *sun)
{
  int test_fd, ret = 1;

  if((test_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0){
    perror("socket");
    exit(1);
  }
  if(connect(test_fd, sun, sizeof(*sun)) < 0){
    if(errno == ECONNREFUSED){
      if(unlink(sun->sun_path) < 0){
	fprintf(stderr, "Failed to removed unused socket '%s':", 
		sun->sun_path);
	perror("");
      }
      ret = 0;
    }
    else perror("connect");
  }
  close(test_fd);
  return(ret);
}

void bind_socket(int fd, const char *name)
{
  struct sockaddr_un sun;

  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path, name);
  
  if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
    if((errno != EADDRINUSE) || still_used(&sun)){
      perror("bind");
      exit(1);
    }
    else if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
      perror("bind");
      exit(1);
    }
  }

}

static void Usage(void)
{
  fprintf(stderr, "Usage : uml_router [-unix control-socket data-socket]\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int connect_fd, data_fd, n, one = 1;

  if(!strcmp(argv[1], "-unix")){
    if(argc < 4) Usage();
    ctl_socket = argv[2];
    data_socket = argv[3];
  }
  
  if((connect_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0){
    perror("socket");
    exit(1);
  }
  if(setsockopt(connect_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, 
		sizeof(one)) < 0){
    perror("setsockopt");
    exit(1);
  }
  bind_socket(connect_fd, ctl_socket);
  if(listen(connect_fd, 15) < 0){
    perror("listen");
    exit(1);
  }
  if((data_fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0){
    perror("socket");
    exit(1);
  }
  bind_socket(data_fd, data_socket);
    
  FD_ZERO(&perm_fds);
  FD_SET(connect_fd, &perm_fds);
  FD_SET(data_fd, &perm_fds);
  max_fd = (connect_fd > data_fd ? connect_fd : data_fd);
  while(1){
    fd_set temp;

    temp = perm_fds;
    n = select(max_fd + 1, &temp, NULL, NULL, NULL);
    if(n < 0){
      perror("select");
      break;
    }
    if(FD_ISSET(connect_fd, &temp)){
      accept_connection(connect_fd);
      FD_CLR(connect_fd, &temp);
    }
    if(FD_ISSET(data_fd, &temp)){
      handle_data(data_fd);
      FD_CLR(data_fd, &temp);
    }
    handle_connections(&temp, max_fd + 1);
  }
  return 0;
}
