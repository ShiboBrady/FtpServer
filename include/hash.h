#ifndef HASH_H
#define HASH_H 

#include "common.h"

//hash函数
typedef unsigned int (*hashfunc_t)(unsigned int, void *);

//hash表结点的定义
typedef struct hash_node
{
    void *key;
    void *value;
    struct hash_node *prev;
    struct hash_node *next;
}hash_node_t;

//hash表的定义
typedef struct hash
{
    unsigned int buckets; //hash表的大小
    hashfunc_t hash_func; //将关键字与位置对应起来的hash函数
    hash_node_t **nodes; //hash链表数组
}hash_t;

hash_t *hash_alloc(unsigned int buckets, hashfunc_t hash_func);
void *hash_lookup_value_by_key(hash_t *hash, void *key, unsigned int key_size);
void hash_add_entry(hash_t *hash, void *key, unsigned int key_size, 
                    void *value, unsigned int value_size);
void hash_free_entry(hash_t *hash, void *key, unsigned int key_size);
void hash_clear_entry(hash_t *hash);
void hash_destroy(hash_t *hash);

#endif  /*HASH_H*/
