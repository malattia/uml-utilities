/* Copyright 2001, 2002 Jeff Dike and others
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/poll.h>

#define ETH_ALEN 6

struct connection {
  struct connection *next;
  struct connection *prev;
  int control;
  struct sockaddr_un name;
  unsigned char addr[ETH_ALEN];
};

enum request_type { REQ_NEW_CONTROL };

struct request_v0 {
  enum request_type type;
  union {
    struct {
      unsigned char addr[ETH_ALEN];
      struct sockaddr_un name;
    } new_control;
  } u;
};

#define SWITCH_MAGIC 0xfeedface

struct request_v1 {
  unsigned long magic;
  enum request_type type;
  union {
    struct {
      unsigned char addr[ETH_ALEN];
      struct sockaddr_un name;
    } new_control;
  } u;
};

struct request_v2 {
  unsigned long magic;
  int version;
  enum request_type type;
  struct sockaddr_un sock;
};

struct reply_v2 {
  unsigned char mac[ETH_ALEN];
  struct sockaddr_un sock;
};

union request {
  struct request_v0 v0;
  struct request_v1 v1;
  struct request_v2 v2;
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

static char *ctl_socket = "/tmp/uml.ctl";

static char *data_socket = NULL;
static struct sockaddr_un data_sun;

static void cleanup(void)
{
  if(unlink(ctl_socket) < 0){
    printf("Couldn't remove control socket '%s' : ", ctl_socket);
    perror("");
  }
  if((data_socket != NULL) && (unlink(data_socket) < 0)){
    printf("Couldn't remove data socket '%s' : ", data_socket);
    perror("");
  }
}

static struct pollfd *fds = NULL;
static int maxfds = 0;
static int nfds = 0;

static void add_fd(int fd)
{
  struct pollfd *p;

  if(nfds == maxfds){
    maxfds = maxfds ? 2 * maxfds : 4;
    if((fds = realloc(fds, maxfds * sizeof(struct pollfd))) == NULL){
      perror("realloc");
      cleanup();
      exit(1);
    }
  }
  p = &fds[nfds++];
  p->fd = fd;
  p->events = POLLIN;
}

static void remove_fd(int fd)
{
  int i;

  for(i = 0; i < nfds; i++){
    if(fds[i].fd == fd) break;
  }
  if(i == nfds){
    fprintf(stderr, "remove_fd : Couldn't find descriptor %d\n", fd);
  }
  memmove(&fds[i], &fds[i + 1], (maxfds - i - 1) * sizeof(struct pollfd));
  nfds--;
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
  remove_fd(fd);
  close(fd);
}

static void free_connection(struct connection *conn)
{
  close_descriptor(conn->control);
  if(conn->prev) conn->prev->next = conn->next;
  else head = conn->next;
  if(conn->next) conn->next->prev = conn->prev;
  free(conn);
}

static void service_connection_v0(struct connection *conn, 
				  struct request_v0 *req)
{
  switch(req->type){
  default:
    printf("Bad request type : %d\n", req->type);
    free_connection(conn);
  }
}

static void service_connection_v1(struct connection *conn, 
				  struct request_v1 *req)
{
  switch(req->type){
  default:
    printf("Bad request type : %d\n", req->type);
    free_connection(conn);
  }
}

static void service_connection(struct connection *conn)
{
  union request req;
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
  if(req.v1.magic == SWITCH_MAGIC) service_connection_v1(conn, &req.v1);
  else service_connection_v0(conn, &req.v0);
}

static int hub = 0;
static int compat_v0 = 0;

static int match_addr(unsigned char *host, unsigned char *packet)
{
  if(hub) return(1);
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

static struct connection * setup_connection(int fd, unsigned char *mac, 
					    struct sockaddr_un *name)
{
  struct connection *conn;

  conn = malloc(sizeof(struct connection));
  if(conn == NULL){
    perror("malloc");
    close_descriptor(fd);
    return(NULL);
  }
  conn->next = head;
  if(head) head->prev = conn;
  conn->prev = NULL;
  conn->control = fd;
  memcpy(conn->addr, mac, sizeof(conn->addr));
  conn->name = *name;
  head = conn;
  printf("New connection - hw address %02x:%02x:%02x:%02x:%02x:%02x\n",
	 conn->addr[0], conn->addr[1], conn->addr[2], conn->addr[3], 
	 conn->addr[4], conn->addr[5]);
  return(conn);
}

static void new_connection_v0(int fd, struct connection *conn, 
			      struct request_v0 *req)
{
  switch(req->type){
  case REQ_NEW_CONTROL:
    setup_connection(fd, req->u.new_control.addr, &req->u.new_control.name);
    break;
  default:
    printf("Bad request type : %d\n", req->type);
    close_descriptor(fd);
  }
}

static void new_connection_v1(int fd, struct connection *conn, 
			      struct request_v1 *req)
{
  struct connection *new;
  int n;

  switch(req->type){
  case REQ_NEW_CONTROL:
    new = setup_connection(fd, req->u.new_control.addr, 
			   &req->u.new_control.name);
    if(new == NULL) return;
    n = write(fd, &data_sun, sizeof(data_sun));
    if(n != sizeof(data_sun)){
      perror("Sending data socket name");
      free_connection(new);
    }
    break;
  default:
    printf("Bad request type : %d\n", req->type);
    close_descriptor(fd);
  }
}

static unsigned long start_range = 0;
static unsigned long end_range = 0xfffffffe;
static unsigned long current = 0;

static void new_connection_v2(int fd, struct connection *conn, 
			      struct request_v2 *req)
{
  struct connection *new;
  struct reply_v2 reply;
  int n;

  switch(req->type){
  case REQ_NEW_CONTROL:
    if(current > end_range){
      fprintf(stderr, "Out of MACs\n");
      close(fd);
      return;
    }
    reply.mac[0] = 0xfe;
    reply.mac[1] = 0xfd;
    reply.mac[2] = current >> 24;
    reply.mac[3] = (current >> 16) & 0xff;
    reply.mac[4] = (current >> 8) & 0xff;
    reply.mac[5] = current & 0xff;
    new = setup_connection(fd, reply.mac, &req->sock);
    if(new == NULL) return;
    reply.sock = data_sun;
    n = write(fd, &reply, sizeof(reply));
    if(n != sizeof(reply)){
      perror("Sending reply");
      free_connection(new);
    }
    current++;
    break;
  default:
    printf("Bad request type : %d\n", req->type);
    close_descriptor(fd);
  }
}

static void new_connection(int fd)
{
  union request req;
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
  if(req.v1.magic == SWITCH_MAGIC){
    if(req.v2.version == 2) new_connection_v2(fd, conn, &req.v2);
    else if(req.v2.version > 2) 
      fprintf(stderr, "Request for a version %d connection, which this "
	      "uml_switch doesn't support\n", req.v2.version);
    else new_connection_v1(fd, conn, &req.v1);
  }
  else new_connection_v0(fd, conn, &req.v0);
}

void handle_connection(int fd)
{
  struct connection *c;

  for(c = head; c != NULL; c = c->next){
    if(c->control == fd){
      service_connection(c);
      break;
    }
  }
  if(c == NULL) new_connection(fd);
}

void accept_connection(int fd)
{
  struct sockaddr addr;
  int len, new;

  len = sizeof(addr);
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
  add_fd(new);
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

int bind_socket(int fd, const char *name, struct sockaddr_un *sock_out)
{
  struct sockaddr_un sun;

  sun.sun_family = AF_UNIX;
  strncpy(sun.sun_path, name, sizeof(sun.sun_path));
  
  if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
    if((errno == EADDRINUSE) && still_used(&sun)) return(EADDRINUSE);
    else if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
      perror("bind");
      return(EPERM);
    }
  }
  if(sock_out != NULL) *sock_out = sun;
  return(0);
}

static char *prog;

void bind_sockets_v0(int ctl_fd, const char *ctl_name, 
		     int data_fd, const char *data_name)
{
  int ctl_err, ctl_present = 0, ctl_used = 0;
  int data_err, data_present = 0, data_used = 0;
  int try_remove_ctl, try_remove_data;

  ctl_err = bind_socket(ctl_fd, ctl_name, NULL);
  if(ctl_err != 0) ctl_present = 1;
  if(ctl_err == EADDRINUSE) ctl_used = 1;

  data_err = bind_socket(data_fd, data_name, &data_sun);
  if(data_err != 0) data_present = 1;
  if(data_err == EADDRINUSE) data_used = 1;

  if(!ctl_err && !data_err){
    return;
  }

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

void bind_data_socket(int fd, struct sockaddr_un *sun)
{
  struct {
    char zero;
    int pid;
    int usecs;
  } name;
  struct timeval tv;

  name.zero = 0;
  name.pid = getpid();
  gettimeofday(&tv, NULL);
  name.usecs = tv.tv_usec;
  sun->sun_family = AF_UNIX;
  memcpy(sun->sun_path, &name, sizeof(name));
  if(bind(fd, (struct sockaddr *) sun, sizeof(*sun)) < 0){
    perror("Binding to data socket");
    exit(1);
  }
}

void bind_sockets(int ctl_fd, const char *ctl_name, int data_fd)
{
  int err, used;

  err = bind_socket(ctl_fd, ctl_name, NULL);
  if(err == 0){
    bind_data_socket(data_fd, &data_sun);
    return;
  }
  else if(err == EADDRINUSE) used = 1;
  
  if(used){
    fprintf(stderr, "The control socket '%s' has another server "
	    "attached to it\n", ctl_name);
    fprintf(stderr, "You can either\n");
    fprintf(stderr, "\tremove '%s'\n", ctl_name);
    fprintf(stderr, "\tor rerun with a different, unused filename for a "
	    "socket\n");
  }
  else
    fprintf(stderr, "The control socket '%s' exists, isn't used, but couldn't "
	    "be removed\n", ctl_name);
  exit(1);
}

static void Usage(void)
{
  fprintf(stderr, "Usage : %s [ -unix control-socket ] [ -hub ]\n"
	  "or : %s -compat-v0 [ -unix control-socket data-socket ] "
	  "[ -hub ]\n", prog, prog);
  exit(1);
}

int main(int argc, char **argv)
{
  int connect_fd, data_fd, n, i, one = 1;

  prog = argv[0];
  argv++;
  argc--;
  while(argc > 0){
    if(!strcmp(argv[0], "-unix")){
      if(argc < 2) Usage();
      ctl_socket = argv[1];
      argc -= 2;
      argv += 2;
      if(!compat_v0) break;
      if(argc < 1) Usage();
      data_socket = argv[0];
      argc--;
      argv++;
    }
    else if(!strcmp(argv[0], "-hub")){
      printf("%s will be a hub instead of a switch\n", prog);
      hub = 1;
      argc--;
      argv++;
    }
    else if(!strcmp(argv[0], "-compat-v0")){
      printf("Control protocol 0 compatibility\n");
      compat_v0 = 1;
      data_socket = "/tmp/uml.data";
      argc--;
      argv++;
    }
    else Usage();
  }

  current = start_range;
  
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

  if(compat_v0) bind_sockets_v0(connect_fd, ctl_socket, data_fd, data_socket);
  else bind_sockets(connect_fd, ctl_socket, data_fd);

  if(listen(connect_fd, 15) < 0){
    perror("listen");
    exit(1);
  }

  if(signal(SIGINT, sig_handler) < 0)
    perror("Setting handler for SIGINT");

  if(compat_v0) 
    printf("%s attached to unix sockets '%s' and '%s'\n", prog, ctl_socket,
	   data_socket);
  else printf("%s attached to unix socket '%s'\n", prog, ctl_socket);

  if(isatty(0)) add_fd(0);
  add_fd(connect_fd);
  add_fd(data_fd);
  while(1){
    char buf[128];

    n = poll(fds, nfds, -1);
    if(n < 0){
      if(errno == EINTR) continue;
      perror("poll");
      break;
    }
    for(i = 0; i < nfds; i++){
      if(fds[i].revents == 0) continue;
      if(fds[i].fd == 0){
	if(fds[i].revents & POLLHUP){
	  printf("EOF on stdin, cleaning up and exiting\n");
	  goto out;
	}
	n = read(0, buf, sizeof(buf));
	if(n < 0){
	  perror("Reading from stdin");
	  break;
	}
	else if(n == 0){
	  printf("EOF on stdin, cleaning up and exiting\n");
	  goto out;
	}
      }
      else if(fds[i].fd == connect_fd){
	if(fds[i].revents & POLLHUP){
	  printf("Error on connection fd\n");
	  continue;
	}
	accept_connection(connect_fd);
      }
      else if(fds[i].fd == data_fd) handle_data(data_fd);
      else handle_connection(fds[i].fd);
    }
  }
 out:
  cleanup();
  return 0;
}
