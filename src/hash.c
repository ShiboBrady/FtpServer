#include "hash.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static hash_node_t ** hash_get_bucket(hash_t *hash, void *key);
static hash_node_t *hash_get_node_by_key(hash_t *hash, void *key, unsigned int key_size);
static void hash_clear_bucket(hash_t *hash, unsigned int index);

//����һ��hash��Ȼ��ָ�뷵��
hash_t *hash_alloc(unsigned int buckets, hashfunc_t hash_func)
{
    hash_t *hash = (hash_t *)malloc(sizeof(hash_t));
    hash->buckets = buckets; //hash���size
    hash->hash_func = hash_func; //hash����
    int len = sizeof(hash_node_t *) * buckets; //����ռ�õ��ֽ���
    hash->nodes = (hash_node_t **)malloc(len);
    memset(hash->nodes, 0, len);
    return hash;
}

//����key����value
void *hash_lookup_value_by_key(hash_t *hash, void *key, unsigned int key_size)
{
    hash_node_t *node = hash_get_node_by_key(hash, key, key_size);
    if(node == NULL)
        return NULL;
    else
        return node->value;
}

//��hash����ӽ��
void hash_add_entry(hash_t *hash, void *key, unsigned int key_size, 
                    void *value, unsigned int value_size)
{
    //1������bucket
    hash_node_t **buck = hash_get_bucket(hash, key);
    assert(buck != NULL);

    //2�������µĽ��
    hash_node_t *node = (hash_node_t *)malloc(sizeof(hash_node_t));
    memset(node, 0, sizeof(hash_node_t));
    node->key = malloc(key_size);
    node->value = malloc(value_size);
    memcpy(node->key, key, key_size);
    memcpy(node->value, value, value_size);

    //3��������Ӧ������ͷ�巨��
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

//ɾ��һ�����
void hash_free_entry(hash_t *hash, void *key, unsigned int key_size)
{
    //1�����ҽ��
    hash_node_t *node = hash_get_node_by_key(hash, key, key_size);
    assert(node != NULL);

    //2���ͷ�ֵ�ռ�
    free(node->key);
    free(node->value);

    //3���ж��Ƿ��ǵ�һ�����
    if(node->prev != NULL)
        node->prev->next = node->next;
    else{
        hash_node_t **buck = hash_get_bucket(hash, key);
        *buck = node->next;
    }

    //4���ж��Ƿ������һ�����
    if(node->next != NULL)
        node->next->prev = node->prev;

    //5���ͷŸý��
    free(node);
}

//���hash��
void hash_clear_entry(hash_t *hash)
{
    unsigned int i;
    for(i = 0; i < hash->buckets; ++i){
        hash_clear_bucket(hash, i);
        hash->nodes[i] = NULL;
    }
}

//����hash��
void hash_destroy(hash_t *hash)
{
    hash_clear_entry(hash);
    free(hash->nodes);
    free(hash);
}

//����key��ȡbucket�ĵ�ַ
static hash_node_t ** hash_get_bucket(hash_t *hash, void *key)
{
    //����hash���������keyֵ��hash���е�λ��
    unsigned int pos = hash->hash_func(hash->buckets, key);
    assert(pos < hash->buckets);
    //��������
    return &(hash->nodes[pos]);
}

//����keyֵ��ȡnode����ָ��
static hash_node_t *hash_get_node_by_key(hash_t *hash, void *key, unsigned int key_size)
{
    //1����ȡbucket
    hash_node_t **buck = hash_get_bucket(hash, key);
    assert(buck != NULL);

    //2������Ԫ��
    hash_node_t *node = *buck;
    while(node != NULL && memcmp(node->key, key, key_size) != 0)
        node = node->next;

    //3�����ز��ҽ���������ɹ���ʧ���������
    return node;
}

//���ĳ��bucket
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
