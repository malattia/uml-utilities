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
#include "switch.h"
#include "hash.h"

#define GC_INTERVAL 2
#define GC_EXPIRE 100

#define IS_BROADCAST(addr) ((addr[0] & 1) == 1)

static int hub = 0;
static int compat_v0 = 0;

struct port {
  struct port *next;
  struct port *prev;
  int control;
  struct sockaddr_un sock;
};

static struct port *monitor_port = NULL;

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

struct request_v3 {
  unsigned long magic;
  int version;
  enum request_type type;
  struct sockaddr_un sock;
};

union request {
  struct request_v0 v0;
  struct request_v1 v1;
  struct request_v2 v2;
  struct request_v3 v3;
};

struct packet {
  struct {
    unsigned char dest[6];
    unsigned char src[6];
    unsigned char proto[2];
  } header;
  unsigned char data[1500];
};

static struct port *head = NULL;

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

static void free_port(struct port *port)
{
  close_descriptor(port->control);
  if(port->prev) port->prev->next = port->next;
  else head = port->next;
  if(port->next) port->next->prev = port->prev;
  free(port);
}

static void service_port(struct port *port)
{
  union request req;
  int n;

  n = read(port->control, &req, sizeof(req));
  if(n < 0){
    perror("Reading request");
    free_port(port);
    return;
  }
  else if(n == 0){
    printf("Disconnect\n");
    free_port(port);
    return;
  }
  printf("Bad request\n");  
  free_port(port);
}

static void update_src(struct port *port, struct packet *p)
{
  struct port *last;

  /* We don't like broadcast source addresses */
  if(IS_BROADCAST(p->header.src)) return;  

  last = find_in_hash(p->header.src);

  if(port != last){
    /* old value differs from actual input port */

    printf(" Addr: %02x:%02x:%02x:%02x:%02x:%02x "
	   " New port %d",
	   p->header.src[0], p->header.src[1], p->header.src[2],
	   p->header.src[3], p->header.src[4], p->header.src[5],
	   port->control);

    if(last != NULL){
      printf("old port %d", last->control);
      delete_hash_entry(p->header.src);
    }
    printf("\n");

    insert_into_hash(p->header.src, port);
  }
  update_entry_time(p->header.src);
}

static void send_dst(struct port *port, int fd, struct packet *packet, int len)
{
  struct port *target, *p;

  target = find_in_hash(packet->header.dest);
  if((target == NULL) || IS_BROADCAST(packet->header.dest) || hub){
    if((target == NULL) && !IS_BROADCAST(packet->header.dest)){
      printf("unknown Addr: %02x:%02x:%02x:%02x:%02x:%02x from port ",
	     packet->header.src[0], packet->header.src[1], 
	     packet->header.src[2], packet->header.src[3], 
	     packet->header.src[4], packet->header.src[5]);
      if(port == NULL) printf("UNKNOWN\n");
      else printf("%d\n", port->control);
    } 

    /* no cache or broadcast/multicast == all ports */
    for(p = head; p != NULL; p = p->next){
      /* don't send it back the port it came in */
      if(p == port) continue;

      sendto(fd, packet, len, 0, (struct sockaddr *) &p->sock, 
	     sizeof(p->sock));
    }
  }
  else {
    sendto(fd, packet, len, 0, (struct sockaddr *) &target->sock, 
	   sizeof(target->sock));
    if((monitor_port != NULL) && (port != monitor_port) && 
       (target != monitor_port))
      sendto(fd, packet, len, 0, (struct sockaddr *) &monitor_port->sock, 
	     sizeof(monitor_port->sock));
  }
}

static void handle_data(int fd)
{
  struct packet packet;
  struct port *p;
  struct sockaddr_un source;
  int len, socklen;

  len = recvfrom(fd, &packet, sizeof(packet), 0, (struct sockaddr *) &source,
		 &socklen);
  if(len < 0){
    if(errno != EAGAIN) perror("Reading data");
    return;
  }

  for(p = head; p != NULL; p = p->next){
    if(!memcmp(p->sock.sun_path, source.sun_path, sizeof(source.sun_path) - 1))
      break;
  }
  
  /* if we have an incoming port (we should) */
  if(p != NULL) update_src(p, &packet);
  else printf("Unknown connection for packet, shouldnt happen.\n");

  send_dst(p, fd, &packet, len);  
}

static struct port *setup_port(int fd, struct sockaddr_un *name)
{
  struct port *port;

  port = malloc(sizeof(struct port));
  if(port == NULL){
    perror("malloc");
    close_descriptor(fd);
    return(NULL);
  }
  port->next = head;
  if(head) head->prev = port;
  port->prev = NULL;
  port->control = fd;
  port->sock = *name;
  head = port;
  printf("New connection\n");
  return(port);
}

static void new_port_v0(int fd, struct port *conn, 
			      struct request_v0 *req)
{
  switch(req->type){
  case REQ_NEW_CONTROL:
    setup_port(fd, &req->u.new_control.name);
    break;
  default:
    printf("Bad request type : %d\n", req->type);
    close_descriptor(fd);
  }
}

static void new_port_v1_v3(int fd, struct port *port, enum request_type type, 
			   struct sockaddr_un *sock)
{
  struct port *new;
  int n;

  switch(type){
  case REQ_NEW_CONTROL:
    new = setup_port(fd, sock);
    if(new == NULL) return;
    n = write(fd, &data_sun, sizeof(data_sun));
    if(n != sizeof(data_sun)){
      perror("Sending data socket name");
      free_port(new);
    }
    break;
  default:
    printf("Bad request type : %d\n", type);
    close_descriptor(fd);
  }
}

static unsigned long start_range = 0;
static unsigned long end_range = 0xfffffffe;
static unsigned long current = 0;

static void new_port_v2(int fd, struct port *port, struct request_v2 *req)
{
  struct port *new;
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
    new = setup_port(fd, &req->sock);
    if(new == NULL) return;
    reply.sock = data_sun;
    n = write(fd, &reply, sizeof(reply));
    if(n != sizeof(reply)){
      perror("Sending reply");
      free_port(new);
    }
    current++;
    break;
  default:
    printf("Bad request type : %d\n", req->type);
    close_descriptor(fd);
  }
}

static void new_port(int fd)
{
  union request req;
  struct port *port;
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
    printf("EOF from new port\n");
    close_descriptor(fd);
    return;
  }
  if(req.v1.magic == SWITCH_MAGIC){
    if(req.v2.version == 2) new_port_v2(fd, port, &req.v2);
    if(req.v3.version == 3) 
      new_port_v1_v3(fd, port, req.v3.type, &req.v3.sock);
    else if(req.v2.version > 2) 
      fprintf(stderr, "Request for a version %d port, which this "
	      "uml_switch doesn't support\n", req.v2.version);
    else new_port_v1_v3(fd, port, req.v1.type, &req.v1.u.new_control.name);
  }
  else new_port_v0(fd, port, &req.v0);
}

void handle_port(int fd)
{
  struct port *p;

  for(p = head; p != NULL; p = p->next){
    if(p->control == fd){
      service_port(p);
      break;
    }
  }
  if(p == NULL) new_port(fd);
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
      else handle_port(fds[i].fd);
    }
  }
 out:
  cleanup();
  return 0;
}
