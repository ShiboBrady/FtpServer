#ifndef HASH_H
#define HASH_H 

#include "common.h"

//hash����
typedef unsigned int (*hashfunc_t)(unsigned int, void *);

//hash����Ķ���
typedef struct hash_node
{
    void *key;
    void *value;
    struct hash_node *prev;
    struct hash_node *next;
}hash_node_t;

//hash��Ķ���
typedef struct hash
{
    unsigned int buckets; //hash��Ĵ�С
    hashfunc_t hash_func; //���ؼ�����λ�ö�Ӧ������hash����
    hash_node_t **nodes; //hash��������
}hash_t;

hash_t *hash_alloc(unsigned int buckets, hashfunc_t hash_func);
void *hash_lookup_value_by_key(hash_t *hash, void *key, unsigned int key_size);
void hash_add_entry(hash_t *hash, void *key, unsigned int key_size, 
                    void *value, unsigned int value_size);
void hash_free_entry(hash_t *hash, void *key, unsigned int key_size);
void hash_clear_entry(hash_t *hash);
void hash_destroy(hash_t *hash);

#endif  /*HASH_H*/
