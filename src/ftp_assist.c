#include "ftp_assist.h"
#include "common.h"
#include "ftp_codes.h"
#include "sysutil.h"
#include "hash.h"
#include "command_map.h"
#include "parse_conf.h"
#include "configure.h"

unsigned int num_of_clients = 0; //��ʾ���ӵ���Ŀ

//��ip��ַ��ʹ�ø�ip��ַ�ͻ���������ӳ��
hash_t *ip_to_clients;

//�ӽ���id���ý��������ͻ���ip��ַ��ӳ��
hash_t *pid_to_ip;


//���Ȩ��
void check_permission()
{
    if(getuid()){
        fprintf(stderr, "Must be started by root.\n");
        exit(EXIT_FAILURE);
    }
}

//��װ�ӽ����źż�⺯��
void setup_signal_chld()
{
    if(signal(SIGCHLD, handle_sigchld) == SIG_ERR)
        ERR_EXIT("signal");
}

//��ʼ��hash��
void init_hash()
{
    ip_to_clients = hash_alloc(256, hash_func);
    pid_to_ip = hash_alloc(256, hash_func);
}

//��ip��ַ���뵽hash����
void add_clients_to_hash(session_t *sess, uint32_t ip)
{
    //��ǰ�ͻ���������Ŀ+1
    ++num_of_clients;
    sess->curr_clients = num_of_clients;

    //���µ�ǰ��ip��ַ����ʹ�ø�ip��ַ�Ŀͻ�������+1
    sess->curr_ip_clients = add_ip_to_hash(ip_to_clients, ip);
}

//�����̺ź�ip��ַ���뵽hash����
void add_pid_ip_to_hash(unsigned int pid, uint32_t ip)
{
    hash_add_entry(pid_to_ip, &pid, sizeof(pid), &ip, sizeof(ip));
}

//����ͻ���̫������
void limit_num_clients(session_t *sess)
{
    if(tunable_max_clients > 0 && sess->curr_clients > tunable_max_clients){
        //421 There are too many connected users, please try later
        ftp_reply(sess, FTP_TOO_MANY_USERS, "There are too many connected users, please try later.");
        exit(EXIT_FAILURE);
    } 
    if(tunable_max_per_ip > 0 && sess->curr_ip_clients > tunable_max_per_ip){
        //421 There are too many connections from your internet address
        ftp_reply(sess, FTP_IP_LIMIT, "There are too many connections from your internet address.");
        exit(EXIT_FAILURE);
    }
}


//�ӽ����˳�ʱ�Ĳ���
void handle_sigchld(int sig)
{
    pid_t pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0){

        //����Ӧ�Ŀͻ�������-1
        --num_of_clients;

        //pid -> ip
        uint32_t *p_ip = hash_lookup_value_by_key(pid_to_ip, &pid, sizeof(pid));
        assert(p_ip != NULL);
        uint32_t ip = *p_ip;

        //ip -> clients
        unsigned int *p_value = (unsigned int *)hash_lookup_value_by_key(ip_to_clients, &ip, sizeof(ip));
        assert(p_value != NULL && *p_value > 0);
        --*p_value;

        //�ͷ�pid_to_ip���hash��Ľ��
        hash_free_entry(pid_to_ip, &pid, sizeof(pid));
    }
}

//hash����
unsigned int hash_func(unsigned int buckets, void *key)
{
    unsigned int *number = (unsigned int *)key;

    return (*number) % buckets;
}

//�ڶ�Ӧ��ip��¼��+1�����ص�ǰip�Ŀͻ�����
unsigned int add_ip_to_hash(hash_t *hash, uint32_t ip)
{
    unsigned int *p_value = (unsigned int *)hash_lookup_value_by_key(hash, &ip, sizeof(ip));
    //�������ڸ�ip��¼��ʱ���½���㲢����1
    if(p_value == NULL){
        unsigned int value = 1;
        hash_add_entry(hash, &ip, sizeof(ip), &value, sizeof(value));
        return 1;
    }
    //�����ڸ�ip��¼ʱ������ip��Ӧ�Ŀͻ���������+1���������޸ĺ��ֵ
    else{
        unsigned int value = *p_value;
        value++;
        *p_value = value;
        return value;
    }
}
