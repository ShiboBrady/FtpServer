#include "session.h"
#include "common.h"
#include "ftp_nobody.h"
#include "ftp_proto.h"
#include "configure.h"
#include "trans_ctrl.h"
#include "sysutil.h"

void session_init(session_t *sess)
{
    memset(sess->command, 0, sizeof(sess->command));
    memset(sess->comm, 0, sizeof(sess->comm));
    memset(sess->args, 0, sizeof(sess->args));
    sess->peerfd = -1;
    sess->nobody_fd = -1;
    sess->proto_fd = -1;

    sess->user_uid = 0;
    sess->ascii_mode = 0;

    sess->p_addr = NULL;
    sess->data_fd = -1;
    sess->listen_fd = -1;
    sess->rnfr_name = NULL;
    sess->restart_pos = 0;
    sess->is_translating_data = 0;

    sess->limits_max_upload = tunable_upload_max_rate * 1024;
    sess->limits_max_download = tunable_download_max_rate * 1024;
    sess->start_time_sec = 0;
    sess->start_time_usec = 0;

    sess->is_receive_abor = 0;
    sess->curr_clients = 0;
    sess->curr_ip_clients = 0;

    sess->ip = 0;
    memset(sess->username, 0, sizeof(sess->username));
}

void session_clear(session_t *sess)
{
    memset(sess->command, 0, sizeof(sess->command));
    memset(sess->comm, 0, sizeof(sess->comm));
    memset(sess->args, 0, sizeof(sess->args));
}

void session_begin(session_t *sess)
{
    int fds[2];
    
    //生成一个管道，用户父子进程建通信
    if(socketpair(PF_UNIX, SOCK_STREAM, 0, fds) == -1)
        ERR_EXIT("socketpair");

    pid_t pid;
    if((pid = fork()) == -1)
        ERR_EXIT("fork");

    //子进程（proto进程）负责和客户机交互
    else if(pid == 0){
        close(fds[0]);
        sess->proto_fd = fds[1];
        
        setup_signal_alarm_ctrl_fd();
        setup_signal_sigurg();
        activate_oobinline(sess->peerfd);
        activate_signal_sigurg(sess->peerfd);

        handle_proto(sess);

    //父进程（nobody进程）负责处理从proto进程传递过来的命令
    }else{
        close(fds[1]);
        sess->nobody_fd = fds[0];
        handle_nobody(sess);
    }
}
