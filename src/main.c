#include "common.h"
#include "session.h"
#include "sysutil.h"
#include "configure.h"
#include "parse_conf.h"
#include "ftp_codes.h"
#include "command_map.h"
#include "hash.h"
#include "ftp_assist.h"

//һ��ȫ�ֱ�������
extern session_t *p_sess;

int main(int argc, const char *argv[])
{
    //�ж��Ƿ�����root������е�
    check_permission(); 

    //ע���ӳ����˳�ʱ���źŴ������
    setup_signal_chld();

    //��ȡ�����ļ�
    parseconf_load_file("ftpserver.conf");

    //��ʼ��hash��
    init_hash();

    //��ȡ�����׽���
    int listenfd = tcp_server(tunable_listen_address, tunable_listen_port);
    pid_t pid;

    //��ʼ���Ự
    session_t sess;
    session_init(&sess);
    p_sess = &sess;
    
    //��ʼ�����ͻ�����������
    while(1){
        struct sockaddr_in addr;
        int peerfd = accept_timeout(listenfd, &addr, tunable_accept_timeout);
        if(peerfd == -1 && errno == ETIMEDOUT)
            continue;
        else if(peerfd == -1)
            ERR_EXIT("accept");

        //��ǰ�ͻ���������Ŀ+1,�����µ�ǰ��ip��ַ����ʹ�ø�ip��ַ�Ŀͻ�������+1
        uint32_t ip = addr.sin_addr.s_addr;
        sess.ip = ip;
        add_clients_to_hash(&sess, ip);
        
        if((pid = fork()) < 0)
            ERR_EXIT("fork");

        //�ӽ��̸���Ϳͻ���ͨ��
        else if(pid == 0){
            close(listenfd);
            sess.peerfd = peerfd;
            limit_num_clients(&sess);
            session_begin(&sess);
            exit(EXIT_SUCCESS);
        }

        //�����̸�����������ͻ�����������
        else{
            close(peerfd);
            add_pid_ip_to_hash(pid, ip);
        }
    }
    return 0;
}

