/* Copyright 2001 Jeff Dike
 * Licensed under the GPL
 */

#ifndef __HOST_H
#define __HOST_H

#include "output.h"

struct addr_change {
	enum { ADD_ADDR, DEL_ADDR } what;
	unsigned char addr[4];
};

extern int do_exec(char **args, int need_zero, struct output *output);
extern int route_and_arp(char *dev, char *addr, int need_route, 
			 struct output *output);
extern int no_route_and_arp(char *dev, char *addr, struct output *output);
extern void forward_ip(struct output *output);

extern void address_change(struct addr_change *change, char *dev, 
			   struct output *output);
extern int mk_node(char *devname, int major, int minor);

#endif
