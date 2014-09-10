#ifndef SESSION_H
#define SESSION_H 

#include "common.h"
#define MAX_COMMAND 1024

//�����洢��ǰ�Ự��Ϣ�Ľṹ��
typedef struct{
    char command[MAX_COMMAND]; //�ͻ��˷�������FTPָ��
    char comm[MAX_COMMAND]; //FTPָ��
    char args[MAX_COMMAND]; //FTPָ��Ĳ���

    int peerfd; //��ͻ��˽���ͨ�ŵ��׽���

    int nobody_fd; //nobody���̳��еĹܵ���һ�Σ����ں�proto����ͨ��
    int proto_fd; //proto���̳��еĹܵ�����һ�ˣ����ں�nobody����ͨ��

    uid_t user_uid; //��ǰ���������û���uid
    int ascii_mode; //��������ͻ��������ļ�����ʱ�ı����ʽ

    struct sockaddr_in *p_addr; //portģʽ�¶Է���ip��port
    int data_fd; //���ݴ���fd
    int listen_fd; //����fd,����pasvģʽ

    char *rnfr_name; //���ļ�������ʱ��ԭ�ļ���

    long long restart_pos; //�ļ�����ϵ�

    int is_translating_data; //�Ƿ��ڴ�������

    int limits_max_upload; //�޶�������ϴ��ٶ�
    int limits_max_download; //�޶�����������ٶ�
    int start_time_sec; //��ʼ������
    int start_time_usec; //��ʼ��΢����

    int is_receive_abor; //�Ƿ���ܵ�abor�źţ��������ݴ����źţ�

    unsigned int curr_clients; //��ǰ�Ŀͻ�����
    unsigned int curr_ip_clients; // ��ǰip�Ŀͻ�����

    uint32_t ip; //�ͻ���ip��ַ
    char username[100]; //�û���
}session_t;

void session_clear(session_t *sess);
void session_init(session_t *sess); //�Ự��ʼ��
void session_begin(session_t *sess); //�Ự��ʼ

#endif  /*SESSION_H*/
