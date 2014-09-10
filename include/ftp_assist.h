#ifndef FTP_ASSIST_H
#define FTP_ASSIST_H 

#include "session.h"
#include "hash.h"

//���Ȩ��
void check_permission();

//��װ�ӽ����źż�⺯��
void setup_signal_chld();

//��ʼ��hash��
void init_hash();

//��ip��ַ���뵽hash����
void add_clients_to_hash(session_t *sess, uint32_t ip);

//�����̺ź�ip��ַ���뵽hash����
void add_pid_ip_to_hash(unsigned int pid, uint32_t ip);

//����ͻ���̫������
void limit_num_clients(session_t *sess);

//�ӽ����˳�ʱ�Ĳ���
void handle_sigchld(int sig);

//hash����
unsigned int hash_func(unsigned int buckets, void *key);

//�ڶ�Ӧ��ip��¼��+1�����ص�ǰip�Ŀͻ�����
unsigned int add_ip_to_hash(hash_t *hash, uint32_t ip);

#endif  /*FTP_ASSIST_H*/
