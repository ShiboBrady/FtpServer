#ifndef SESSION_H
#define SESSION_H 

#include "common.h"
#define MAX_COMMAND 1024

//用来存储当前会话信息的结构体
typedef struct{
    char command[MAX_COMMAND]; //客户端发送来的FTP指令
    char comm[MAX_COMMAND]; //FTP指令
    char args[MAX_COMMAND]; //FTP指令的参数

    int peerfd; //与客户端进行通信的套接字

    int nobody_fd; //nobody进程持有的管道的一段，用于和proto进程通信
    int proto_fd; //proto进程持有的管道的另一端，用于和nobody进程通信

    uid_t user_uid; //当前进程所属用户的uid
    int ascii_mode; //服务器与客户机进程文件传输时的编码格式

    struct sockaddr_in *p_addr; //port模式下对方的ip和port
    int data_fd; //数据传输fd
    int listen_fd; //监听fd,用于pasv模式

    char *rnfr_name; //对文件重命名时的原文件名

    long long restart_pos; //文件传输断点

    int is_translating_data; //是否在传输数据

    int limits_max_upload; //限定的最大上传速度
    int limits_max_download; //限定的最大下载速度
    int start_time_sec; //开始的秒数
    int start_time_usec; //开始的微秒数

    int is_receive_abor; //是否接受到abor信号（带外数据传输信号）

    unsigned int curr_clients; //当前的客户数量
    unsigned int curr_ip_clients; // 当前ip的客户数量

    uint32_t ip; //客户端ip地址
    char username[100]; //用户名
}session_t;

void session_clear(session_t *sess);
void session_init(session_t *sess); //会话初始化
void session_begin(session_t *sess); //会话开始

#endif  /*SESSION_H*/
