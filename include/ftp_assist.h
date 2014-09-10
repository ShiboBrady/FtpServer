#ifndef FTP_ASSIST_H
#define FTP_ASSIST_H 

#include "session.h"
#include "hash.h"

//检查权限
void check_permission();

//安装子进程信号监测函数
void setup_signal_chld();

//初始化hash表
void init_hash();

//将ip地址加入到hash表中
void add_clients_to_hash(session_t *sess, uint32_t ip);

//将进程号和ip地址加入到hash表中
void add_pid_ip_to_hash(unsigned int pid, uint32_t ip);

//处理客户端太多的情况
void limit_num_clients(session_t *sess);

//子进程退出时的操作
void handle_sigchld(int sig);

//hash函数
unsigned int hash_func(unsigned int buckets, void *key);

//在对应的ip记录中+1，返回当前ip的客户数量
unsigned int add_ip_to_hash(hash_t *hash, uint32_t ip);

#endif  /*FTP_ASSIST_H*/
