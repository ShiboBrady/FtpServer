#include "hash.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static hash_node_t ** hash_get_bucket(hash_t *hash, void *key);
static hash_node_t *hash_get_node_by_key(hash_t *hash, void *key, unsigned int key_size);
static void hash_clear_bucket(hash_t *hash, unsigned int index);

//建立一个hash表，然后将指针返回
hash_t *hash_alloc(unsigned int buckets, hashfunc_t hash_func)
{
    hash_t *hash = (hash_t *)malloc(sizeof(hash_t));
    hash->buckets = buckets; //hash表的size
    hash->hash_func = hash_func; //hash函数
    int len = sizeof(hash_node_t *) * buckets; //数组占用的字节数
    hash->nodes = (hash_node_t **)malloc(len);
    memset(hash->nodes, 0, len);
    return hash;
}

//根据key查找value
void *hash_lookup_value_by_key(hash_t *hash, void *key, unsigned int key_size)
{
    hash_node_t *node = hash_get_node_by_key(hash, key, key_size);
    if(node == NULL)
        return NULL;
    else
        return node->value;
}

//向hash中添加结点
void hash_add_entry(hash_t *hash, void *key, unsigned int key_size, 
                    void *value, unsigned int value_size)
{
    //1、查找bucket
    hash_node_t **buck = hash_get_bucket(hash, key);
    assert(buck != NULL);

    //2、生成新的结点
    hash_node_t *node = (hash_node_t *)malloc(sizeof(hash_node_t));
    memset(node, 0, sizeof(hash_node_t));
    node->key = malloc(key_size);
    node->value = malloc(value_size);
    memcpy(node->key, key, key_size);
    memcpy(node->value, value, value_size);

    //3、插入相应的链表（头插法）
    if(*buck != NULL){
        hash_node_t *head = *buck;
        node->next = head;
        head->prev = node;
        *buck = node;
    }else{
        *buck = node;
    }
    //printf("node->key = %u\nnode->value = %d\n", (unsigned int)node->key, (uint32_t)node->value);
}

//删除一个结点
void hash_free_entry(hash_t *hash, void *key, unsigned int key_size)
{
    //1、查找结点
    hash_node_t *node = hash_get_node_by_key(hash, key, key_size);
    assert(node != NULL);

    //2、释放值空间
    free(node->key);
    free(node->value);

    //3、判断是否是第一个结点
    if(node->prev != NULL)
        node->prev->next = node->next;
    else{
        hash_node_t **buck = hash_get_bucket(hash, key);
        *buck = node->next;
    }

    //4、判断是否是最后一个结点
    if(node->next != NULL)
        node->next->prev = node->prev;

    //5、释放该结点
    free(node);
}

//清空hash表
void hash_clear_entry(hash_t *hash)
{
    unsigned int i;
    for(i = 0; i < hash->buckets; ++i){
        hash_clear_bucket(hash, i);
        hash->nodes[i] = NULL;
    }
}

//销毁hash表
void hash_destroy(hash_t *hash)
{
    hash_clear_entry(hash);
    free(hash->nodes);
    free(hash);
}

//根据key获取bucket的地址
static hash_node_t ** hash_get_bucket(hash_t *hash, void *key)
{
    //根据hash函数计算出key值在hash表中的位置
    unsigned int pos = hash->hash_func(hash->buckets, key);
    assert(pos < hash->buckets);
    //返回引用
    return &(hash->nodes[pos]);
}

//根据key值获取node结点的指针
static hash_node_t *hash_get_node_by_key(hash_t *hash, void *key, unsigned int key_size)
{
    //1、获取bucket
    hash_node_t **buck = hash_get_bucket(hash, key);
    assert(buck != NULL);

    //2、查找元素
    hash_node_t *node = *buck;
    while(node != NULL && memcmp(node->key, key, key_size) != 0)
        node = node->next;

    //3、返回查找结果，包含成功和失败两种情况
    return node;
}

//清空某个bucket
static void hash_clear_bucket(hash_t *hash, unsigned int index)
{
    assert(index < hash->buckets);
    hash_node_t *node = hash->nodes[index];
    hash_node_t *tmp = node;
    while(node){
        tmp = node->next;
        free(node->key);
        free(node->value);
        free(node);
        node = tmp;
    }
    hash->nodes[index] = NULL;
}
