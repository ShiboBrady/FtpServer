#include "ftp_assist.h"
#include "common.h"
#include "ftp_codes.h"
#include "sysutil.h"
#include "hash.h"
#include "command_map.h"
#include "parse_conf.h"
#include "configure.h"

unsigned int num_of_clients = 0; //表示连接的数目

//从ip地址到使用该ip地址客户端数量的映射
hash_t *ip_to_clients;

//从进程id到该进程所连客户端ip地址的映射
hash_t *pid_to_ip;


//检查权限
void check_permission()
{
    if(getuid()){
        fprintf(stderr, "Must be started by root.\n");
        exit(EXIT_FAILURE);
    }
}

//安装子进程信号监测函数
void setup_signal_chld()
{
    if(signal(SIGCHLD, handle_sigchld) == SIG_ERR)
        ERR_EXIT("signal");
}

//初始化hash表
void init_hash()
{
    ip_to_clients = hash_alloc(256, hash_func);
    pid_to_ip = hash_alloc(256, hash_func);
}

//将ip地址加入到hash表中
void add_clients_to_hash(session_t *sess, uint32_t ip)
{
    //当前客户端连接数目+1
    ++num_of_clients;
    sess->curr_clients = num_of_clients;

    //记下当前的ip地址，将使用该ip地址的客户端数量+1
    sess->curr_ip_clients = add_ip_to_hash(ip_to_clients, ip);
}

//将进程号和ip地址加入到hash表中
void add_pid_ip_to_hash(unsigned int pid, uint32_t ip)
{
    hash_add_entry(pid_to_ip, &pid, sizeof(pid), &ip, sizeof(ip));
}

//处理客户端太多的情况
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


//子进程退出时的操作
void handle_sigchld(int sig)
{
    pid_t pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0){

        //将对应的客户端数量-1
        --num_of_clients;

        //pid -> ip
        uint32_t *p_ip = hash_lookup_value_by_key(pid_to_ip, &pid, sizeof(pid));
        assert(p_ip != NULL);
        uint32_t ip = *p_ip;

        //ip -> clients
        unsigned int *p_value = (unsigned int *)hash_lookup_value_by_key(ip_to_clients, &ip, sizeof(ip));
        assert(p_value != NULL && *p_value > 0);
        --*p_value;

        //释放pid_to_ip这个hash表的结点
        hash_free_entry(pid_to_ip, &pid, sizeof(pid));
    }
}

//hash函数
unsigned int hash_func(unsigned int buckets, void *key)
{
    unsigned int *number = (unsigned int *)key;

    return (*number) % buckets;
}

//在对应的ip记录中+1，返回当前ip的客户数量
unsigned int add_ip_to_hash(hash_t *hash, uint32_t ip)
{
    unsigned int *p_value = (unsigned int *)hash_lookup_value_by_key(hash, &ip, sizeof(ip));
    //当不存在该ip记录的时候，新建结点并返回1
    if(p_value == NULL){
        unsigned int value = 1;
        hash_add_entry(hash, &ip, sizeof(ip), &value, sizeof(value));
        return 1;
    }
    //当存在该ip记录时，将该ip对应的客户机的数量+1，并返回修改后的值
    else{
        unsigned int value = *p_value;
        value++;
        *p_value = value;
        return value;
    }
}
