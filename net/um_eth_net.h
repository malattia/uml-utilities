#ifndef _UM_ETH_NET_H
#define _UM_ETH_NET_H

#define UM_ETH_NET_MAX_PACKET	1544
#define UM_ETH_NET_PORT		5299
#define UM_ETH_NET_DEFAULT_NUM	100
#define UM_ETH_NET_HDR_SIZE	(sizeof(unsigned int)*2)

enum packet_type {
	PACKET_DATA = 1,
	PACKET_MGMT
};

enum socket_type {
	SOCKET_LISTEN = 1,
	SOCKET_CONNECTION,
	SOCKET_PHY,
	SOCKET_TAP
};

struct connection_data {
	enum socket_type stype;
	int net_num;
#ifdef __KERNEL__
	void *dev;
	int irq;
#endif
};

extern int packet_output(int, unsigned char *, int);
extern int socket_phy_setup(char *);
extern int socket_tap_setup(char *);

#endif
