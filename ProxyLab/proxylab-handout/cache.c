#include "csapp.h"
#include "cache.h"

/* Create an empty cache */
void cache_init(cache_t *cache) {
    for (int i = 0; i < CACHE_OBJS_COUNT; i++) {
        cache->cacheobjs[i].readcnt = 0;
        cache->cacheobjs[i].LRU = 0;
        cache->cacheobjs[i].is_empty = 1;
        sem_init(&(cache->cacheobjs[i].mutex_rdcnt), 0, 1);
        sem_init(&(cache->cacheobjs[i].mutex_w), 0, 1);
    }
}

/* find uri is in the cache or not, if found return related index, else return -1*/
int cache_find(cache_t *cache, char *uri) {
    int i;
    for (i = 0; i < CACHE_OBJS_COUNT; i++) {
        read_pre(cache, i);
        if ((cache->cacheobjs[i].is_empty==0) && (strcmp(uri, cache->cacheobjs[i].uri)==0)) {
            read_after(cache, i);
            break;
        }
        read_after(cache, i);
    }

    if (i >= CACHE_OBJS_COUNT) return -1;    /* can not find url in the cache */
    return i;
}

/* find an available cache, if empty cache block exists, return related index immediately, otherwise return the index with smallest lru*/
int  cache_eviction(cache_t *cache) {
    int min = LRU_MAGIC_NUMBER;
    int minindex = 0;
    int i;
    for (i = 0; i < CACHE_OBJS_COUNT; i++) {
        read_pre(cache, i);
        if (cache->cacheobjs[i].is_empty == 1) {
            minindex = i;
            read_after(cache, i);
            break;
        }

        if (cache->cacheobjs[i].LRU < min) {
            minindex = i;
        }
        read_after(cache, i);
    }
    return minindex;
}

void cache_store(cache_t *cache, char *uri, char *buf) {
    int i = cache_eviction(cache);

    write_pre(cache, i);

    strcpy(cache->cacheobjs[i].uri, uri);
    strcpy(cache->cacheobjs[i].obj, buf);
    cache->cacheobjs[i].is_empty = 0;
    cache->cacheobjs[i].LRU = LRU_MAGIC_NUMBER;
    cache_lru(cache, i);

    write_after(cache, i);
}

/* update the LRU number except the new cache one */
void cache_lru(cache_t *cache, int index) {
    int i;
    for(i=0; i<index; i++)    {
        write_pre(cache, i);
        if(cache->cacheobjs[i].is_empty==0 && i!=index){
            cache->cacheobjs[i].LRU--;
        }
        write_after(cache, i);
    }
    i++;
    for(; i<CACHE_OBJS_COUNT; i++) {
        write_pre(cache, i);
        if(cache->cacheobjs[i].is_empty==0 && i!=index){
            cache->cacheobjs[i].LRU--;
        }
        write_after(cache, i);
    }
}

void read_pre(cache_t *cache, int i) {
    P(&cache->cacheobjs[i].mutex_rdcnt);
    cache->cacheobjs[i].readcnt++;
    if (cache->cacheobjs[i].readcnt == 1) {   /* first in */
        P(&cache->cacheobjs[i].mutex_w);
    }
    V(&cache->cacheobjs[i].mutex_rdcnt);
}

void read_after(cache_t *cache, int i) {
    P(&cache->cacheobjs[i].mutex_rdcnt);
    cache->cacheobjs[i].readcnt--;
    if (cache->cacheobjs[i].readcnt == 0) {   /* Last out */
        V(&cache->cacheobjs[i].mutex_w);
    }
    V(&cache->cacheobjs[i].mutex_rdcnt);
}

void write_pre(cache_t *cache, int i) {
    P(&cache->cacheobjs[i].mutex_w);
}

void write_after(cache_t *cache, int i) {
    V(&cache->cacheobjs[i].mutex_w);
}

