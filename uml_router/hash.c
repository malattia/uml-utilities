/* Copyright 2002 Yon Uriarte and Jeff Dike
 * Licensed under the GPL
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include "switch.h"
#include "hash.h"

#define HASH_SIZE 128
#define HASH_MOD 11

struct hash_entry {
  struct hash_entry *next;
  struct hash_entry *prev;
  time_t last_seen;
  void *port;
  unsigned char dst[ETH_ALEN];
};

static struct hash_entry *h[HASH_SIZE];

static int calc_hash(char *src)
{
  return ((*(u_int32_t *) &src[0] % HASH_MOD) ^ src[4] ^ src[5] ) % HASH_SIZE ;
}

static struct hash_entry *find_entry(char *dst)
{
  struct hash_entry *e;
  int k = calc_hash(dst);

  for(e = h[k]; e; e = e->next){
    if(!memcmp(&e->dst, dst, ETH_ALEN)) return(e);
  }
  return(NULL);  
}

void *find_in_hash(char *dst)
{
  struct hash_entry *e = find_entry(dst);
  if(e == NULL) return(NULL);
  return(e->port);
}

void insert_into_hash(char *src, void *port)
{
  struct hash_entry *new;
  int k = calc_hash(src);

  new = find_in_hash(src);
  if(new != NULL) return;

  new = malloc(sizeof(*new));
  if(new == NULL){
    perror("Failed to malloc hash entry");
    return;
  }

  memcpy(&new->dst, src, ETH_ALEN );
  if(h[k] != NULL) h[k]->prev = new;
  new->next = h[k];
  new->prev = NULL;
  new->port = port;
  new->last_seen = 0;
  h[k] = new;
}

void update_entry_time(char *src)
{
  struct hash_entry *e;

  e = find_entry(src);
  if(e == NULL) return;
  e->last_seen = time(NULL);
}

void delete_hash_entry(char *dst)
{
  int k = calc_hash(dst);
  struct hash_entry *old = find_entry(dst);

  if(old == NULL) return;
  if(old->prev != NULL) old->prev->next = old->next;
  if(old->next != NULL) old->next->prev = old->prev;
  if(h[k] == old) h[k] = old->next;
  free(old);
}

static void for_all_hash(void (*f)(struct hash_entry *, void *), void *arg)
{
  int i;
  struct hash_entry *e, *next;

  for(i = 0; i < HASH_SIZE; i++){
    for(e = h[i]; e; e = next){
      next = e->next;
      (*f)(e, arg);
    }
  }
}

struct printer {
  time_t now;
  char *(*port_id)(void *);
};

static void print_hash_entry(struct hash_entry *e, void *arg)
{
  struct printer *p = arg;

  printf("Hash: %d Addr: %02x:%02x:%02x:%02x:%02x:%02x to port: %s  " 
	 "age %ld secs\n", calc_hash(e->dst),
	 e->dst[0], e->dst[1], e->dst[2], e->dst[3], e->dst[4], e->dst[5],
	 (*p->port_id)(e->port), (int) p->now - e->last_seen);
}

void print_hash(char *(*port_id)(void *))
{
  struct printer p = ((struct printer) { now : 		time(NULL),
					 port_id :	port_id });

  for_all_hash(print_hash_entry, &p);
}
