#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/uio.h>
#ifdef TUNTAP
#include <linux/if_tun.h>
#endif

struct output {
  int total;
  int used;
  char *buffer;
};

#define INIT_OUTPUT { 0, 0, NULL }

/* Shouldn't there be some other way to figure out the major and minor
 * number of the tap device other than hard-wiring it here? Does FreeBSD
 * have a 'tap' device? Should we have some kind of #ifdef here?
 */
#define TAP_MAJOR 36
#define TAP_MINOR 16  /* plus whatever tap device it was. */

static void fail(int fd)
{
  char c = 0;

  if(write(fd, &c, sizeof(c)) != sizeof(c))
    perror("Writing failure byte");
  exit(1);
}

static void add_output(struct output *output, char *new, int len)
{
  if(len == -1) len = strlen(new) + 1;
  else len++;
  if(output->used + len > output->total){
    output->total = output->used + len;
    output->total = (output->total + 4095) & ~4095;
    if(output->buffer == NULL){
      if((output->buffer = malloc(output->total)) == NULL){
	perror("mallocing new output buffer");
	*output = ((struct output) { 0, 0, NULL});
	return;
      }
    }
    else if((output->buffer = realloc(output->buffer, output->total)) == NULL){
	perror("reallocing new output buffer");
	*output = ((struct output) { 0, 0, NULL});
	return;
    }
  }
  strncpy(&output->buffer[output->used], new, len - 1);
  output->used += len - 1;
  output->buffer[output->used] = '\0';
}

static void output_errno(struct output *output, char *str)
{
    add_output(output, str, -1);
    add_output(output, strerror(errno), -1);
    add_output(output, "\n", -1);
}

static int do_exec(char **args, int need_zero, struct output *output)
{
  int pid, status, fds[2], n;
  char buf[256], **arg;

  if(output){
    add_output(output, "*", -1);
    for(arg = args; *arg; arg++){
      add_output(output, " ", -1);
      add_output(output, *arg, -1);
    }
    add_output(output, "\n", -1);
    if(pipe(fds) < 0){
      perror("Creating pipe");
      output = NULL;
    }
  }
  if((pid = fork()) == 0){
    if(output){
      close(fds[0]);
      if((dup2(fds[1], 1) < 0) || (dup2(fds[1], 2) < 0)) perror("dup2");
    }
    execvp(args[0], args);
    fprintf(stderr, "Failed to exec '%s'", args[0]);
    perror("");
    exit(1);
  }
  else if(pid < 0){
    perror("fork failed");
    return(-1);
  }
  if(output){
    close(fds[1]);
    while((n = read(fds[0], buf, sizeof(buf))) > 0) add_output(output, buf, n);
    if(n < 0) perror("Reading command output");
  }
  if(waitpid(pid, &status, 0) < 0){
    perror("execvp");
    return(-1);
  }
  if(need_zero && (!WIFEXITED(status) || (WEXITSTATUS(status) != 0))){
    printf("'%s' didn't exit with status 0\n", args[0]);
    return(-1);
  }
  return(0);
}

static int maybe_insmod(char *dev)
{
  struct ifreq ifr;
  int fd, unit;
  char unit_buf[sizeof("unit=nnn\0")];
  char ethertap_buf[sizeof("ethertapnnn\0")];
  char *ethertap_argv[] = { "insmod", "ethertap", unit_buf, "-o", ethertap_buf,
			    NULL };
  char *netlink_argv[] = { "insmod", "netlink_dev", NULL };
  
  if((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
    perror("socket");
    return(-1);
  }
  strcpy(ifr.ifr_name, dev);
  if(ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) return(0);
  if(errno != ENODEV){
    perror("SIOCGIFFLAGS on tap device");
    return(-1);
  }
  if(sscanf(dev, "tap%d", &unit) != 1){
    fprintf(stderr, "failed to get unit number from '%s'\n", dev);
    return(-1);
  }
  sprintf(unit_buf, "unit=%d", unit);
  sprintf(ethertap_buf, "ethertap%d", unit);
  do_exec(netlink_argv, 0, NULL);
  return(do_exec(ethertap_argv, 0, NULL));
}

/* This is a routine to do a 'mknod' on the /dev/tap<n> if possible:
 * Return: 0 is ok, -1=already open, etc.
 */

static int mk_node(char *devname, int major, int minor)
{
  struct stat statval;
  int retval;

  /* first do a stat on the node to see whether it exists and we
   * had some other reason to fail:
   */
  retval = stat(devname, &statval);
  if(retval == 0) return(0);
  else if(errno != ENOENT){
    /* it does exist. We are just going to return -1, 'cause there
     * was some other problem in the open :-(.
     */
    return -1;
  }

  /* It doesn't exist. We can create it. */

  return(mknod(devname, S_IFCHR|S_IREAD|S_IWRITE, makedev(major, minor)));
}

static int route_and_arp(char *dev, char *addr, int need_route, 
			 struct output *output)
{
  char *arp_argv[] = { "arp", "-Ds", addr, "eth0", "pub", NULL };
  char *route_argv[] = { "route", "add", "-host", addr, "dev", dev, NULL };

  if(do_exec(route_argv, need_route, output)) return(-1);
  do_exec(arp_argv, 0, output);
  return(0);
}

static int no_route_and_arp(char *dev, char *addr)
{
  char *no_arp_argv[] = { "arp", "-i", "eth0", "-d", addr, "pub", NULL };
  char *no_route_argv[] = { "route", "del", "-host", addr, "dev", dev, NULL };

  do_exec(no_route_argv, 0, NULL);
  do_exec(no_arp_argv, 0, NULL);
  return(0);
}

static void forward_ip(struct output *output)
{
  char *forw_argv[] = { "bash",  "-c", 
			"echo 1 > /proc/sys/net/ipv4/ip_forward", NULL };
  do_exec(forw_argv, 0, output);
}

struct addr_change {
	enum { ADD_ADDR, DEL_ADDR } what;
	unsigned char addr[4];
};

static void address_change(struct addr_change *change, char *dev)
{
  char addr[sizeof("255.255.255.255\0")];

  sprintf(addr, "%d.%d.%d.%d", change->addr[0], change->addr[1], 
	  change->addr[2], change->addr[3]);
  switch(change->what){
  case ADD_ADDR:
    route_and_arp(dev, addr, 0, NULL);
    break;
  case DEL_ADDR:
    no_route_and_arp(dev, addr);
    break;
  default:
    fprintf(stderr, "address_change - bad op : %d\n", change->what);
    return;
  }
}

#define BUF_SIZE 1500

#define max(i, j) (((i) > (j)) ? (i) : (j))

static void ethertap(char *dev, int data_fd, int control_fd, char *gate, 
		     char *remote)
{
  int ip[4];

  char *ifconfig_argv[] = { "ifconfig", dev, "arp", "mtu", "1500", gate,
			    "netmask", "255.255.255.255", "up", NULL };
  char *down_argv[] = { "ifconfig", dev, "0.0.0.0", "down", NULL };
  char dev_file[sizeof("/dev/tapxxxx\0")], c;
  int tap, minor;

  signal(SIGHUP, SIG_IGN);
  if(setreuid(0, 0) < 0){
    perror("setreuid");
    fail(control_fd);
  }
  if(gate != NULL){
    sscanf(gate, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
    if(maybe_insmod(dev)) fail(control_fd);

    if(do_exec(ifconfig_argv, 1, NULL)) fail(control_fd);

    if((remote != NULL) && route_and_arp(dev, remote, 1, NULL)) 
      fail(control_fd);

    forward_ip(NULL);

  }
  sprintf(dev_file, "/dev/%s", dev);

/* do a mknod on it in case it doesn't exist. */

  if(sscanf(dev_file, "/dev/tap%d", &minor) != 1){
    fprintf(stderr, "failed to get unit number from '%s'\n", dev_file);
    fail(control_fd);
  }
  minor += TAP_MINOR;
  mk_node(dev_file, TAP_MAJOR, minor); 

  if((tap = open(dev_file, O_RDWR | O_NONBLOCK)) < 0){
    perror("open");
    fail(control_fd);
  }

  c = 1;
  if(write(control_fd, &c, sizeof(c)) != sizeof(c)){
    perror("write");
    fail(control_fd);
  }

  while(1){
    fd_set fds, except;
    char buf[BUF_SIZE];
    int n, max_fd;

    FD_ZERO(&fds);
    FD_SET(tap, &fds);
    FD_SET(data_fd, &fds);
    FD_SET(control_fd, &fds);
    except = fds;
    max_fd = max(max(tap, data_fd), control_fd) + 1;
    if(select(max_fd, &fds, NULL, &except, NULL) < 0){
      perror("select");
      continue;
    }
    if(FD_ISSET(tap, &fds)){
      n = read(tap, buf, sizeof(buf));
      if(n == 0) break;
      else if(n < 0){
	perror("read");
	continue;
      }
      n = send(data_fd, buf, n, 0);
      if((n < 0) && (errno != EAGAIN)){
	perror("send");
	break;
      }
    }
    else if(FD_ISSET(data_fd, &fds)){
      n = recvfrom(data_fd, buf, sizeof(buf), 0, NULL, NULL);
      if(n == 0) break;
      else if(n < 0) perror("recvfrom");
      n = write(tap, buf, n);
      if(n < 0) perror("write");      
    }
    else if(FD_ISSET(control_fd, &fds)){
      struct addr_change change;

      n = read(control_fd, &change, sizeof(change));
      if(n == sizeof(change)) address_change(&change, dev);
      else if(n == 0) break;
      else {
	fprintf(stderr, "read from UML failed, n = %d, errno = %d\n", n, 
		errno);
	break;
      }
    }
  }
  if(gate != NULL) do_exec(down_argv, 0, NULL);
  if(remote != NULL) no_route_and_arp(dev, remote);
}

static void ethertap_v0(int argc, char **argv)
{
  char *dev = argv[0];
  int fd = atoi(argv[1]);
  char *gate_addr = NULL;
  char *remote_addr = NULL;

  if(argc > 2){
    gate_addr = argv[2];
    remote_addr = argv[3];
  }
  ethertap(dev, fd, fd, gate_addr, remote_addr);
}

static void ethertap_v1_v2(int argc, char **argv)
{
  char *dev = argv[0];
  int data_fd = atoi(argv[1]);
  int control_fd = atoi(argv[2]);
  char *gate_addr = NULL;

  if(argc > 3) gate_addr = argv[3];
  ethertap(dev, data_fd, control_fd, gate_addr, NULL);
}

static void slip_up(char **argv)
{
  int fd = atoi(argv[0]);
  char *gate_addr = argv[1];
  char *remote_addr = argv[2];
  char slip_name[sizeof("slxxxx\0")];
  char *up_argv[] = { "ifconfig", slip_name, gate_addr, "pointopoint", 
		      remote_addr, "mtu", "1500", "up", NULL };
  char *arp_argv[] = { "arp", "-Ds", remote_addr, "eth0", "pub", NULL };
  int disc, sencap, n;
  
  disc = N_SLIP;
  if((n = ioctl(fd, TIOCSETD, &disc)) < 0){
    perror("Setting slip line discipline");
    exit(1);
  }
  sencap = 0;
  if(ioctl(fd, SIOCSIFENCAP, &sencap) < 0){
    perror("Setting slip encapsulation");
    exit(1);
  }
  sprintf(slip_name, "sl%d", n);
  if(do_exec(up_argv, 1, NULL)) exit(1);
  do_exec(arp_argv, 1, NULL);
  forward_ip(NULL);
}

static void slip_down(char **argv)
{
  int fd = atoi(argv[0]);
  char *remote_addr = argv[1];
  char slip_name[sizeof("slxxxx\0")];
  char *down_argv[] = { "ifconfig", slip_name, "0.0.0.0", "down", NULL };
  char *no_arp_argv[] = { "arp", "-i", "eth0", "-d", remote_addr, "pub", 
			  NULL };
  int n, disc;

  if((n = ioctl(fd, TIOCGETD, &disc)) < 0){
    perror("Getting slip line discipline");
    exit(1);
  }
  sprintf(slip_name, "sl%d", n);  
  if(do_exec(down_argv, 1, NULL)) exit(1);
  do_exec(no_arp_argv, 1, NULL);
}

static void slip_v0_v2(int argc, char **argv)
{
  char *op = argv[0];

  if(!strcmp(argv[0], "up")) slip_up(&argv[1]);
  else if(!strcmp(argv[0], "down")) slip_down(&argv[1]);
  else {
    printf("slip - Unknown op '%s'\n", op);
    exit(1);
  }
}

#ifdef TUNTAP
static void tuntap_up(int argc, char **argv)
{
  struct ifreq ifr;
  int tap_fd, *fd_ptr;
  char anc[CMSG_SPACE(sizeof(tap_fd))];
  char *dev = argv[0];
  int uml_fd = atoi(argv[1]);
  char *gate_addr = argv[2];
  struct msghdr msg;
  struct cmsghdr *cmsg;
  char *ifconfig_argv[] = { "ifconfig", ifr.ifr_name, gate_addr, "netmask", 
			    "255.255.255.255", "up", NULL };
  char *insmod_argv[] = { "insmod", "tun", NULL };
  struct output output = INIT_OUTPUT;
  struct iovec iov[2];
  int retval;

  msg.msg_control = NULL;
  msg.msg_controllen = 0;

  if(setreuid(0, 0) < 0){
    output_errno(&output, "setreuid to root failed : ");
    goto out;
  }

  /* XXX hardcoded dir name and perms */
  umask(0);
  retval = mkdir("/dev/net", 0755);

  /* #includes for MISC_MAJOR and TUN_MINOR bomb in userspace */
  mk_node("/dev/net/tun", 10, 200);

  do_exec(insmod_argv, 0, &output);
  
  if((tap_fd = open("/dev/net/tun", O_RDWR)) < 0){
    output_errno(&output, "Opening /dev/net/tun failed : ");
    goto out;
  }
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  ifr.ifr_name[0] = '\0';
  if(ioctl(tap_fd, TUNSETIFF, (void *) &ifr) < 0){
    output_errno(&output, "TUNSETIFF");
    goto out;
  }

  if((*gate_addr != '\0') && do_exec(ifconfig_argv, 1, &output)) goto out;
  forward_ip(&output);

  msg.msg_control = anc;
  msg.msg_controllen = sizeof(anc);
  
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(tap_fd));

  msg.msg_controllen = cmsg->cmsg_len;

  fd_ptr = (int *) CMSG_DATA(cmsg);
  *fd_ptr = tap_fd;

 out:
  iov[0].iov_base = ifr.ifr_name;
  iov[0].iov_len = IFNAMSIZ;
  iov[1].iov_base = output.buffer;
  iov[1].iov_len = output.used;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = iov;
  msg.msg_iovlen = sizeof(iov)/sizeof(iov[0]);
  msg.msg_flags = 0;

  if(sendmsg(uml_fd, &msg, 0) < 0){
    perror("sendmsg");
    exit(1);
  }
}

static void tuntap_change(int argc, char **argv)
{
  char *op = argv[0];
  char *dev = argv[1];
  char *address = argv[2];

  if(!strcmp(op, "add")) route_and_arp(dev, address, 0, NULL);
  else no_route_and_arp(dev, address);
}

static void tuntap_v2(int argc, char **argv)
{
  char *op = argv[0];

  if(!strcmp(op, "up")) tuntap_up(argc - 1, argv + 1);
  else if(!strcmp(op, "add") || !strcmp(op, "del")) tuntap_change(argc, argv);
  else {
    fprintf(stderr, "Bad tuntap op : '%s'\n", op);
    exit(1);
  }
}
#endif

#define CURRENT_VERSION (2)

void (*ethertap_handlers[])(int argc, char **argv) = {
  ethertap_v0, 
  ethertap_v1_v2,
  ethertap_v1_v2
};

void (*slip_handlers[])(int argc, char **argv) = { 
  slip_v0_v2, 
  slip_v0_v2,
  slip_v0_v2, 
};

#ifdef TUNTAP
void (*tuntap_handlers[])(int argc, char **argv) = {
  NULL,
  NULL,
  tuntap_v2
};
#endif

int main(int argc, char **argv)
{
  char *version = argv[1];
  char *transport = argv[2];
  void (**handlers)(int, char **);
  char *out;
  int n = 3, v;

  setenv("PATH", "/bin:/usr/bin:/sbin:/usr/sbin", 1);
  v = strtoul(version, &out, 0);
  if(out != version){
    if(v > CURRENT_VERSION){
      fprintf(stderr, "Version mismatch - requested version %d, uml_net "
	      "supports up to version %d\n", v, CURRENT_VERSION);
      exit(1);
    }
  }
  else {
    v = 0;
    transport = version;
    n = 2;
  }
  if(!strcmp(transport, "ethertap")) handlers = ethertap_handlers;
  else if(!strcmp(transport, "slip")) handlers = slip_handlers;
#ifdef TUNTAP
  else if(!strcmp(transport, "tuntap")) handlers = tuntap_handlers;
#endif
  else {
    printf("Unknown transport : '%s'\n", transport);
    exit(1);
  }
  if(handlers[v] != NULL) (*handlers[v])(argc - n, &argv[n]);
  else {
    printf("No version #%d handler for '%s'\n", v, transport);
    exit(1);
  }
  return(0);
}
