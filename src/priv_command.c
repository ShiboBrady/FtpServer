#include "priv_command.h"
#include "common.h"
#include "sysutil.h"
#include "priv_sock.h"
#include "configure.h"

//��ȡ�����׽���
void privop_pasv_get_data_sock(session_t *sess)
{
    char ip[16] = {0};
    priv_sock_recv_str(sess->nobody_fd, ip, sizeof ip);
    uint16_t port = priv_sock_recv_int(sess->nobody_fd);
    //����fd
    int data_fd = tcp_client(20);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    int ret = connect_timeout(data_fd, &addr, tunable_connect_timeout);
    if(ret == -1)
        ERR_EXIT("connect_timeout");
    priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_OK);
    priv_sock_send_fd(sess->nobody_fd, data_fd);
    close(data_fd);
}

//�ж�pasvģʽ�Ƿ���
void privop_pasv_active(session_t *sess)
{
    //����proto���
    priv_sock_send_int(sess->nobody_fd, (sess->listen_fd != -1));
}

//��ȡ����fd
void privop_pasv_listen(session_t *sess)
{
    //����listen fd
    char ip[16] = {0};
    get_local_ip(ip);
    int listenfd = tcp_server(ip, 20);
    sess->listen_fd = listenfd;

    struct sockaddr_in addr;
    socklen_t len = sizeof addr;
    if(getsockname(listenfd, (struct sockaddr*)&addr, &len) == -1)
        ERR_EXIT("getsockname");

    //����Ӧ��
    priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_OK);

    //����port
    uint16_t net_endian_port = ntohs(addr.sin_port);
    priv_sock_send_int(sess->nobody_fd, net_endian_port);
}

//acceptһ���µ�����
void privop_pasv_accept(session_t *sess)
{
    //����������
    int peerfd = accept_timeout(sess->listen_fd, NULL, tunable_accept_timeout);
    //���״̬
    close(sess->listen_fd);
    sess->listen_fd = -1;
    if(peerfd == -1)
    {
        priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_BAD);
        ERR_EXIT("accept_timeout");
    }
    
    //���Է���Ӧ
    priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_OK);

    //��data fd�����Է�
    priv_sock_send_fd(sess->nobody_fd, peerfd);
    close(peerfd);
}
