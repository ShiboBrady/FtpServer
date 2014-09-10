#include "common.h"
#include "session.h"
#include "sysutil.h"
#include "configure.h"
#include "parse_conf.h"
#include "ftp_codes.h"
#include "command_map.h"
#include "hash.h"
#include "ftp_assist.h"

//一个全局变量声明
extern session_t *p_sess;

int main(int argc, const char *argv[])
{
    //判断是否是用root身份运行的
    check_permission(); 

    //注册子程序退出时的信号处理程序
    setup_signal_chld();

    //读取配置文件
    parseconf_load_file("ftpserver.conf");

    //初始化hash表
    init_hash();

    //获取监听套接字
    int listenfd = tcp_server(tunable_listen_address, tunable_listen_port);
    pid_t pid;

    //初始化会话
    session_t sess;
    session_init(&sess);
    p_sess = &sess;
    
    //开始监听客户端连接请求
    while(1){
        struct sockaddr_in addr;
        int peerfd = accept_timeout(listenfd, &addr, tunable_accept_timeout);
        if(peerfd == -1 && errno == ETIMEDOUT)
            continue;
        else if(peerfd == -1)
            ERR_EXIT("accept");

        //当前客户端连接数目+1,并记下当前的ip地址，将使用该ip地址的客户端数量+1
        uint32_t ip = addr.sin_addr.s_addr;
        sess.ip = ip;
        add_clients_to_hash(&sess, ip);
        
        if((pid = fork()) < 0)
            ERR_EXIT("fork");

        //子进程负责和客户端通信
        else if(pid == 0){
            close(listenfd);
            sess.peerfd = peerfd;
            limit_num_clients(&sess);
            session_begin(&sess);
            exit(EXIT_SUCCESS);
        }

        //父进程负责继续监听客户端连接请求
        else{
            close(peerfd);
            add_pid_ip_to_hash(pid, ip);
        }
    }
    return 0;
}

