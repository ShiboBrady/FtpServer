#include "ftp_proto.h"

#include "common.h"
#include "sysutil.h"
#include "strutil.h"
#include "ftp_codes.h"
#include "command_map.h"
#include "trans_ctrl.h"

void handle_proto(session_t *sess)
{
    ftp_reply(sess, FTP_GREET, "(FtpServer 1.0)");
    while(1){
        session_clear(sess);
        start_signal_alarm_ctrl_fd();

        //1、从客户端接受命令
        int ret = readline(sess->peerfd, sess->command, MAX_COMMAND);
        if(ret == -1){
            if(errno == EINTR)
                continue;
            ERR_EXIT("readline");
        }
        else if(ret == 0){
            exit(EXIT_SUCCESS);
        }
        
        //去掉字符串后面的/r/n
        str_trim_crlf(sess->command);

        //解析出命令和命令对应的参数
        str_split(sess->command, sess->comm, sess->args, 32);

        //将命令全部转化为大写形式
        str_upper(sess->comm);
        printf("command = [%s], args = [%s]\n", sess->comm, sess->args);
        //执行命令
        do_command_map(sess);
    }
}
