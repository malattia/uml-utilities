#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
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

static struct connection *head = NULL;
static fd_set perm_fds;
static int max_fd = -1;
static char *prog;

static char *ctl_socket = "/tmp/uml.ctl";
static char *data_socket = "/tmp/uml.data";

static void cleanup(void)
{
  if(unlink(ctl_socket) < 0){
    printf("Couldn't remove control socket '%s' : ", ctl_socket);
    perror("");
  }
  if(unlink(data_socket) < 0){
    printf("Couldn't remove data socket '%s' : ", data_socket);
    perror("");
  }
}

static void sig_handler(int sig)
{
  printf("Caught signal %d, cleaning up and exiting\n", sig);
  cleanup();
  signal(sig, SIG_DFL);
  kill(getpid(), sig);
}

static void close_descriptor(int fd)
{
  FD_CLR(fd, &perm_fds);
  close(fd);
}

static void free_connection(struct connection *conn)
{
  close_descriptor(conn->control);
  FD_CLR(conn->control, &perm_fds);
  if(conn->prev) conn->prev->next = conn->next;
  else head = conn->next;
  if(conn->next) conn->next->prev = conn->prev;
  free(conn);
}

static void service_connection(struct connection *conn)
{
  struct request req;
  int n;

  n = read(conn->control, &req, sizeof(req));
  if(n < 0){
    perror("Reading request");
    free_connection(conn);
    return;
  }
  else if(n == 0){
    printf("Disconnect from hw address %02x:%02x:%02x:%02x:%02x:%02x\n",
	   conn->addr[0], conn->addr[1], conn->addr[2], conn->addr[3], 
	   conn->addr[4], conn->addr[5]);
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
  if(packet[0] & 1) return(1);
  return((packet[0] == host[0]) && (packet[1] == host[1]) && 
	 (packet[2] == host[2]) && (packet[3] == host[3]) && 
	 (packet[4] == host[4]) && (packet[5] == host[5]));
}

static void handle_data(int fd)
{
  struct packet p;
  struct connection *c, *next;
  int len;

  len = recvfrom(fd, &p, sizeof(p), 0, NULL, 0);
  if(len < 0){
    if(errno != EAGAIN) perror("Reading data");
    return;
  }
  for(c = head; c != NULL; c = next){
    next = c->next;
    if(match_addr(c->addr, p.header.dest)){
      sendto(fd, &p, len, 0, (struct sockaddr *) &c->name, sizeof(c->name));
    }
  }
}

static void new_connection(int fd)
{
  struct request req;
  struct connection *conn;
  int len;

  len = read(fd, &req, sizeof(req));
  if(len < 0){
    if(errno != EAGAIN){
      perror("Reading request");
      close_descriptor(fd);
    }
    return;
  }
  else if(len == 0){
    printf("EOF from new connection\n");
    close_descriptor(fd);
    return;
  }
  switch(req.type){
  case REQ_NEW_CONTROL:
    conn = malloc(sizeof(struct connection));
    if(conn == NULL){
      perror("malloc");
      close_descriptor(fd);
      return;
    }
    conn->next = head;
    if(head) head->prev = conn;
    conn->prev = NULL;
    conn->control = fd;
    memcpy(conn->addr, req.u.new_control.addr, sizeof(conn->addr));
    conn->name = req.u.new_control.name;
    head = conn;
    printf("New connection - hw address %02x:%02x:%02x:%02x:%02x:%02x\n",
	   conn->addr[0], conn->addr[1], conn->addr[2], conn->addr[3], 
	   conn->addr[4], conn->addr[5]);
    break;
  default:
    printf("Bad request type : %d\n", req.type);
    close_descriptor(fd);
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
  if(fcntl(new, F_SETFL, O_NONBLOCK) < 0){
    perror("fcntl - setting O_NONBLOCK");
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
  if(connect(test_fd, (struct sockaddr *) sun, sizeof(*sun)) < 0){
    if(errno == ECONNREFUSED){
      if(unlink(sun->sun_path) < 0){
	fprintf(stderr, "Failed to removed unused socket '%s': ", 
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

int bind_socket(int fd, const char *name)
{
  struct sockaddr_un sun;

  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path, name);
  
  if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
    if((errno == EADDRINUSE) && still_used(&sun)) return(EADDRINUSE);
    else if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
      perror("bind");
      return(EPERM);
    }
  }
  return(0);
}

void bind_sockets(int ctl_fd, const char *ctl_name, 
		  int data_fd, const char *data_name)
{
  int ctl_err, ctl_present = 0, ctl_used = 0;
  int data_err, data_present = 0, data_used = 0;
  int try_remove_ctl, try_remove_data;

  ctl_err = bind_socket(ctl_fd, ctl_name);
  if(ctl_err != 0) ctl_present = 1;
  if(ctl_err == EADDRINUSE) ctl_used = 1;

  data_err = bind_socket(data_fd, data_name);
  if(data_err != 0) data_present = 1;
  if(data_err == EADDRINUSE) data_used = 1;

  if(!ctl_err && !data_err) return;

  unlink(ctl_name);
  unlink(data_name);

  try_remove_ctl = ctl_present;
  try_remove_data = data_present;
  if(ctl_present && ctl_used){
    fprintf(stderr, "The control socket '%s' has another server "
	    "attached to it\n", ctl_name);
    try_remove_ctl = 0;
  }
  else if(ctl_present && !ctl_used)
    fprintf(stderr, "The control socket '%s' exists, isn't used, but couldn't "
	    "be removed\n", ctl_name);
  if(data_present && data_used){
    fprintf(stderr, "The data socket '%s' has another server "
	    "attached to it\n", data_name);
    try_remove_data = 0;
  }
  else if(data_present && !data_used)
    fprintf(stderr, "The data socket '%s' exists, isn't used, but couldn't "
	    "be removed\n", data_name);
  if(try_remove_ctl || try_remove_data){
    fprintf(stderr, "You can either\n");
    if(try_remove_ctl && !try_remove_data) 
      fprintf(stderr, "\tremove '%s'\n", ctl_socket);
    else if(!try_remove_ctl && try_remove_data) 
      fprintf(stderr, "\tremove '%s'\n", data_socket);
    else fprintf(stderr, "\tremove '%s' and '%s'\n", ctl_socket, data_socket);
    fprintf(stderr, "\tor rerun with different, unused filenames for "
	    "sockets:\n");
    fprintf(stderr, "\t\t%s -unix <control> <data>\n", prog);
    fprintf(stderr, "\t\tand run the UMLs with "
	    "'eth0=daemon,,unix,<control>,<data>\n");
    exit(1);
  }
  else {
    fprintf(stderr, "You should rerun with different, unused filenames for "
	    "sockets:\n");
    fprintf(stderr, "\t%s -unix <control> <data>\n", prog);
    fprintf(stderr, "\tand run the UMLs with "
	    "'eth0=daemon,,unix,<control>,<data>'\n");
    exit(1);
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

  prog = argv[0];
  if(argc > 1){
    if(!strcmp(argv[1], "-unix")){
      if(argc < 4) Usage();
      ctl_socket = argv[2];
      data_socket = argv[3];
    }
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
  if(fcntl(connect_fd, F_SETFL, O_NONBLOCK) < 0){
    perror("Setting O_NONBLOCK on connection fd");
    exit(1);
  }
  if((data_fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0){
    perror("socket");
    exit(1);
  }
  if(fcntl(data_fd, F_SETFL, O_NONBLOCK) < 0){
    perror("Setting O_NONBLOCK on data fd");
    exit(1);
  }

  bind_sockets(connect_fd, ctl_socket, data_fd, data_socket);

  if(listen(connect_fd, 15) < 0){
    perror("listen");
    exit(1);
  }

  if(signal(SIGINT, sig_handler) < 0)
    perror("Setting handler for SIGINT");

  printf("%s attached to unix sockets '%s' and '%s'\n", prog, ctl_socket,
	 data_socket);
  FD_ZERO(&perm_fds);
  if(isatty(0)) FD_SET(0, &perm_fds);
  FD_SET(connect_fd, &perm_fds);
  FD_SET(data_fd, &perm_fds);
  max_fd = (connect_fd > data_fd ? connect_fd : data_fd);
  while(1){
    fd_set temp;
    char buf[128];

    temp = perm_fds;
    n = select(max_fd + 1, &temp, NULL, NULL, NULL);
    if(n < 0){
      perror("select");
      break;
    }
    if(FD_ISSET(0, &temp)){
      n = read(0, buf, sizeof(buf));
      if(n < 0){
	perror("Reading from stdin");
	break;
      }
      else if(n == 0){
	printf("EOF on stdin, cleaning up and exiting\n");
	break;
      }
      continue;
    }
    else if(FD_ISSET(connect_fd, &temp)){
      accept_connection(connect_fd);
      FD_CLR(connect_fd, &temp);
    }
    if(FD_ISSET(data_fd, &temp)){
      handle_data(data_fd);
      FD_CLR(data_fd, &temp);
    }
    handle_connections(&temp, max_fd + 1);
  }
  cleanup();
  return 0;
}
