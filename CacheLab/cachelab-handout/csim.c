/*
	maple-ysd
*/
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>	// for INT_MAX

typedef struct
{
    int valid;
    int tag;
    int time_stamp;
} Cache_line;
typedef struct
{
    int S;
    int E;
    int B;
    Cache_line ***Cache;
} Cache_t;

Cache_t cache;
int flag = 0;
int TIME_STAMP = 0;
int hit = 0, miss = 0, eviction = 0;

void mallocCache();
void freeCache();
int evict(int i);
int check(int i, int tag);
int find_line(int group);
int isfull(int group);
void load(int group, int tag);
void store(int group, int tag);
void modify(int group, int tag);

int main(int argc, char * argv[])
{
    // get command line parameters
    int s, E, b;
    FILE *fin;
    char ch;
    while ((ch = getopt(argc, argv, "v::s:E:b:t:"))!= -1)
    {
    	switch(ch)
    	{
    	    case 'v': flag = 1; break;
    	    case 's': s = atoi(optarg); break;
    	    case 'E': E = atoi(optarg); break;
    	    case 'b': b = atoi(optarg); break;
    	    case 't': fin = fopen(optarg, "r");
    	}
    }
    /*if (fin)
    	printf("flag = %d, s = %d, E = %d, b = %d", flag, s, E, b);
    else
    {
    	printf("can not open file");
    	exit(EXIT_FAILURE);
    }*/
    cache.S = 1 << s;
    cache.E = E;
    cache.B = 1 << b;
    
    // allocate memory for cache
    mallocCache();
    
    // read input and count
    int addr, size;
    int sb = s + b;
    while (fscanf(fin, " %c %x,%d", &ch, &addr, &size) != EOF)
    {
    	int group = (addr >> b) & ((1 << s) - 1);
    	int tag = (addr >> sb) & ((1 << sb) - 1);
    	if (flag)
    	{
	    printf("%c %x,%x ", ch, addr, size);
      	}
    	switch (ch)
    	{
    	    case 'L': load(group, tag); if(flag) printf("\n"); break;
    	    case 'S': store(group, tag); if(flag) printf("\n"); break;
    	    case 'M': load(group, tag); store(group, tag); if(flag) printf("\n"); break;
    	}
    }
    freeCache();
    fclose(fin);
    printSummary(hit, miss, eviction);
    return 0;
}

void mallocCache()
{
    int S = cache.S, E = cache.E;
    cache.Cache = (Cache_line ***)malloc(sizeof(Cache_line**) * S);
    for (int i = 0; i < S; ++i)
    {
    	Cache_line **group = (Cache_line**)malloc(sizeof(Cache_line *) * E);
    	for (int j = 0; j < E; ++j)
    	{
    	    Cache_line *line = (Cache_line*)malloc(sizeof(Cache_line));
    	    line->valid = 0;
    	    line->tag = 0;
    	    line->time_stamp = 0;
    	    group[j] = line;
    	}
    	cache.Cache[i] = group;
    }
}
void freeCache()
{
    for (int i = 0; i < cache.S; ++i)
    {
    	for (int j = 0; j < cache.E; ++j)
    	{
    	    free(cache.Cache[i][j]);
    	}
    	free(cache.Cache[i]);
    }
    free(cache.Cache);
    cache.Cache = NULL;
}
int evict(int i)
{
    int temp;
    int Min_time_stamp = INT_MAX;
    for (int j = 0; j < cache.E; ++j)
    {
        if (cache.Cache[i][j]->time_stamp < Min_time_stamp)
        {
            temp = j;
            Min_time_stamp = cache.Cache[i][j]->time_stamp;
        }
    }
    cache.Cache[i][temp]->valid = 0;
    return temp;
}

// check set(or called group) i for tag
// if found, return index
// else return -1
int check(int i, int tag)
{
    for (int j = 0; j < cache.E; ++j)
    {
    	if (tag == cache.Cache[i][j]->tag && cache.Cache[i][j]->valid == 1)
    	    return j;
    }
    return -1;
}
// find the empty line
int find_line(int group)
{
    int line = -1;
    for (int i = 0; i < cache.E; ++i)
    {
    	if (cache.Cache[group][i]->valid == 0)
    	{
    	    line = i;
    	    break;
    	}
    }
    return line;
}
int isfull(int group)
{
    for (int j = 0; j < cache.E; ++j)
    {
        if (cache.Cache[group][j]->valid == 0)
         return 0;
    }
    return 1;
}
void load(int group, int tag)
{
    int line;
    if ((line = check(group, tag)) != -1)
    {
        ++hit;
        if (flag)
            printf("hit ");
    }
    else
    {
        if (flag)
            printf("miss ");
    	++miss;
    	if (isfull(group))
    	{
    	    line = evict(group);
    	    ++eviction;
    	    if (flag)
            printf("eviction ");
   	}
    	else
    	{
    	    line = find_line(group);
    	}
        cache.Cache[group][line]->valid = 1;
        cache.Cache[group][line]->tag = tag;
    }
    cache.Cache[group][line]->time_stamp = ++TIME_STAMP;    
}
void store(int group, int tag)
{
    load(group, tag);
}
void modify(int group, int tag)
{
    load(group, tag);
    store(group, tag);
}

