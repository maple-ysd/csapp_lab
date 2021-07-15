#ifndef __CACHE_H__
#define __CACHE_H__

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define LRU_MAGIC_NUMBER 9999
#define CACHE_OBJS_COUNT 10

#include "csapp.h"

/* $begin cache */
typedef struct {
    int readcnt;                /* Initially = 0 */
    int LRU;
    int is_empty;
    sem_t mutex_rdcnt;                /* protects accesses to readcnt */
    sem_t mutex_w;                    /* protects writing accesses to cache */
    char uri[MAXLINE];         /* store uri */
    char obj[MAX_OBJECT_SIZE]; /* store object from server */

} cache_block;
/* $end cache */

typedef struct {
    cache_block cacheobjs[CACHE_OBJS_COUNT];    /* 10 cache blocks */
} cache_t;

void cache_init(cache_t *cache);
int  cache_find(cache_t *cache, char *uri);
int  cache_eviction(cache_t *cache);
void cache_store(cache_t *cache, char *uri, char *buf);
void cache_lru(cache_t *cache, int i);
void read_pre(cache_t *cache, int i);
void read_after(cache_t *cache, int i);
void write_pre(cache_t *cache, int i);
void write_after(cache_t *cache, int i);


#endif  /* __CACHE_H__*/

