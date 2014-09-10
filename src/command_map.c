#include "command_map.h"
#include "common.h"
#include "session.h"
#include "sysutil.h"
#include "strutil.h"
#include "ftp_codes.h"
#include "configure.h"
#include "trans_data.h"
#include "trans_ctrl.h"
#include "priv_sock.h"

//�������ִ������ĺ���֮�佨����Ӧ��ϵ
typedef struct ftpcmd
{
    const char *cmd;    //FTPָ��
    void (*cmd_handler)(session_t *sess);//��ָ������Ӧ��ִ�к���
} ftpcmd_t;

//���е��������֮���Ӧ��ִ�������
static ftpcmd_t ctrl_cmds[] = {
    /* ���ʿ������� */
    {"USER",    do_user },
    {"PASS",    do_pass },
    {"CWD",     do_cwd  },
    {"XCWD",    do_cwd  },
    {"CDUP",    do_cdup },
    {"XCUP",    do_cdup },
    {"QUIT",    do_quit },
    {"ACCT",    NULL    },
    {"SMNT",    NULL    },
    {"REIN",    NULL    },
    /* ����������� */
    {"PORT",    do_port },
    {"PASV",    do_pasv },
    {"TYPE",    do_type },
    {"STRU",    do_stru },
    {"MODE",    do_mode },

    /* �������� */
    {"RETR",    do_retr },
    {"STOR",    do_stor },
    {"APPE",    do_appe },
    {"LIST",    do_list },
    {"NLST",    do_nlst },
    {"REST",    do_rest },
    {"ABOR",    do_abor },
    {"\377\364\377\362ABOR", do_abor},
    {"PWD",     do_pwd  },
    {"XPWD",    do_pwd  },
    {"MKD",     do_mkd  },
    {"XMKD",    do_mkd  },
    {"RMD",     do_rmd  },
    {"XRMD",    do_rmd  },
    {"DELE",    do_dele },
    {"RNFR",    do_rnfr },
    {"RNTO",    do_rnto },
    {"SITE",    do_site },
    {"SYST",    do_syst },
    {"FEAT",    do_feat },
    {"SIZE",    do_size },
    {"STAT",    do_stat },
    {"NOOP",    do_noop },
    {"HELP",    do_help },
    {"STOU",    NULL    },
    {"ALLO",    NULL    }
};

//��������������ִ�к����Ķ�Ӧ��ϵ
void do_command_map(session_t *sess)
{
    int i;
    int size = sizeof(ctrl_cmds) / sizeof(ctrl_cmds[0]); //�����С
    for (i=0; i<size; ++i)
    {
        if (strcmp(ctrl_cmds[i].cmd, sess->comm) == 0)
        {
            if (ctrl_cmds[i].cmd_handler != NULL)
            {
                ctrl_cmds[i].cmd_handler(sess);
            }
            else
            {
                //������û��ʵ��
                ftp_reply(sess, FTP_COMMANDNOTIMPL, "Unimplement command.");
            }

            break;
        }
    }

    if (i == size)
    {
        //δʶ�������
        ftp_reply(sess, FTP_BADCMD, "Unknown command.");
    }
}

//ͨ������������ͻ��˷���Ӧ����Ϣ
void ftp_reply(session_t *sess, int status, const char *text)
{
    char tmp[1024] = { 0 };
    snprintf(tmp, sizeof tmp, "%d %s\r\n", status, text);
    writen(sess->peerfd, tmp, strlen(tmp));
}

void ftp_lreply(session_t *sess, int status, const char *text)
{
    char tmp[1024] = { 0 };
    snprintf(tmp, sizeof tmp, "%d-%s\r\n", status, text);
    writen(sess->peerfd, tmp, strlen(tmp));
}

//�����û���
void do_user(session_t *sess)
{
    struct passwd *pw;
    if((pw = getpwnam(sess->args)) == NULL)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
        return;
    }

    sess->user_uid = pw->pw_uid;
    ftp_reply(sess, FTP_GIVEPWORD, "Please specify the password.");
}

//��������
void do_pass(session_t *sess)
{
    //struct passwd *getpwuid(uid_t uid)
    struct passwd *pw;
    if((pw = getpwuid(sess->user_uid)) == NULL)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
        return;
    }

    //�����û�����ȡ���û�����Ӧ�ļ��ܵ�����
    struct spwd *spw;
    if((spw = getspnam(pw->pw_name)) == NULL)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
        return;
    }

    //�Ե�ǰ������м��ܣ�����֮ǰ���ܺ��������ȶԣ��۲��Ƿ�һ��
    char *encrypted_password = crypt(sess->args, spw->sp_pwdp);
    if(strcmp(encrypted_password, spw->sp_pwdp) != 0)
    {
        ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
        return;
    }

    //�û���¼�ɹ��󣬽��û�����Ϊ��ǰ��¼���û�
    if(setegid(pw->pw_gid) == -1)
        ERR_EXIT("setegid");
    if(seteuid(pw->pw_uid) == -1)
        ERR_EXIT("seteuid");

    //�л�����Ŀ¼����¼�û��ļ�Ŀ¼
    if(chdir(pw->pw_dir) == -1)
        ERR_EXIT("chdir");

    //�޸�������Ϣ
    umask(tunable_local_umask);

    //�����û���
    strcpy(sess->username, pw->pw_name);

    ftp_reply(sess, FTP_LOGINOK, "Login successful.");
}

//����Ŀ¼
void do_cwd(session_t *sess)
{
    if(chdir(sess->args) == -1){
        //550 Failed to change directory
        ftp_reply(sess, FTP_FILEFAIL, "Failed to change directory.");
        return;
    }
    //250 Directory successfully changed
    ftp_reply(sess, FTP_CWDOK, "Directory successfully changed.");;
}

//������һ��Ŀ¼
void do_cdup(session_t *sess)
{
    if(chdir("..") == -1){
        //550 Failed to change directory
        ftp_reply(sess, FTP_FILEFAIL, "Failed to change directory."    );
        return;
    }
    ftp_reply(sess, FTP_CWDOK, "Directory successfully changed.");
}

//�ͻ��˶Ͽ�����
void do_quit(session_t *sess)
{
    ftp_reply(sess, FTP_GOODBYE, "Good Bye!");
    exit(EXIT_SUCCESS);
}

//���ݴ�����������ģʽ
void do_port(session_t *sess)
{
    //������������ģʽ 
    unsigned int v[6] = {0};
    sscanf(sess->args, "%u,%u,%u,%u,%u,%u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);

    sess->p_addr = (struct sockaddr_in *)malloc(sizeof (struct sockaddr_in));
    memset(sess->p_addr, 0, sizeof(struct sockaddr_in));
    sess->p_addr->sin_family = AF_INET;

    char *p = (char*)&sess->p_addr->sin_port;
    p[0] = v[4];
    p[1] = v[5];

    p = (char*)&sess->p_addr->sin_addr.s_addr;
    p[0] = v[0];
    p[1] = v[1];
    p[2] = v[2];
    p[3] = v[3];

    ftp_reply(sess, FTP_PORTOK, "PORT command successful. Consider using PASV.");    
}

//���ݴ��䱻������ģʽ
void do_pasv(session_t *sess)
{
    char ip[16] = {0};
    get_local_ip(ip);

    //��nobody���̷������ɼ����׽ӷ�������
    priv_sock_send_cmd(sess->proto_fd, PRIV_SOCK_PASV_LISTEN);

    //����nobody���̵�Ӧ��
    char res = priv_sock_recv_result(sess->proto_fd);
    if(res == PRIV_SOCK_RESULT_BAD){
        ftp_reply(sess, FTP_BADCMD, "get listenfd error");
        return;
    }

    uint16_t port = priv_sock_recv_int(sess->proto_fd);

    //227 Entering Passive Mode (192,168,1,51,0,20).
    unsigned int v[6];
    sscanf(ip, "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]);

    uint16_t net_endian_port = htons(port);
    unsigned char *p = (unsigned char*)&net_endian_port;
    v[4] = p[0];
    v[5] = p[1];

    char text[1024] = {0};
    snprintf(text, sizeof text, "Entering Passive Mode (%u,%u,%u,%u,%u,%u).", v[0], v[1], v[2], v[3], v[4], v[5]);

    ftp_reply(sess, FTP_PASVOK, text);
}

//ָ��Ftp���ݴ����ʽ
void do_type(session_t *sess)
{
    //ָ��FTP�Ĵ���ģʽ
    if (strcmp(sess->args, "A") == 0)
    {
        sess->ascii_mode = 1;
        ftp_reply(sess, FTP_TYPEOK, "Switching to ASCII mode.");
    }
    //ָ�������Ƶ��ļ�����ģʽ
    else if (strcmp(sess->args, "I") == 0)
    {
        sess->ascii_mode = 0;
        ftp_reply(sess, FTP_TYPEOK, "Switching to Binary mode.");
    }
    else
    {
        ftp_reply(sess, FTP_BADCMD, "Unrecognised TYPE command.");
    }
}

void do_stru(session_t *sess)
{

}

void do_mode(session_t *sess)
{

}

//�����ļ�
void do_retr(session_t *sess)
{
    download_file(sess);
}

//�ϴ��ļ�
void do_stor(session_t *sess)
{
    upload_file(sess, 0);
}

//�����ļ�
void do_appe(session_t *sess)
{
    upload_file(sess, 1);
}

//������ϸĿ¼��Ϣ
void do_list(session_t *sess)
{
    trans_list(sess, 1);
}

//���ͼ���Ŀ¼��Ϣ��ֻ���ļ�����
void do_nlst(session_t *sess)
{
    trans_list(sess, 0);
}

void do_rest(session_t *sess)
{
    sess->restart_pos = atoll(sess->args);
    char text[1024] = {0};
    snprintf(text, sizeof text, "Restart position accepted (%lld).", sess->restart_pos);
    ftp_reply(sess, FTP_RESTOK, text);
}

void do_abor(session_t *sess)
{
    //225 No transfer to ABOR
    ftp_reply(sess, FTP_ABOR_NOCONN, "No transfer to ABOR.");
}

//���͵�ǰ��·��
void do_pwd(session_t *sess)
{
    char tmp[1024] = {0};
    if(getcwd(tmp, sizeof tmp) == NULL)
    {
        fprintf(stderr, "get cwd error\n");
        ftp_reply(sess, FTP_BADMODE, "error");
        return;
    }
    char text[1024] = {0};
    snprintf(text, sizeof text, "\"%s\"", tmp);
    ftp_reply(sess, FTP_PWDOK, text);
}

//�½��ļ���
void do_mkd(session_t *sess)
{
    if(mkdir(sess->args, 0777) == -1){
        ftp_reply(sess, FTP_FILEFAIL, "Create directory operation failed.");
        return;
    }
    char text[1024] = {0};
    if(sess->args[0] == '/')
        snprintf(text, sizeof(text), "%s created.", sess->args);
    else{
        char tmp[1024] = {0};
        if(getcwd(tmp, sizeof(tmp)) == NULL)
            ERR_EXIT("getcwd");
        snprintf(text, sizeof(text), "%s/%s created.", tmp, sess->args);
    }
    ftp_reply(sess, FTP_MKDIROK, text);
}

//ɾ���ļ��У��ɿͻ��˱�֤ɾ���ļ����������ļ���
void do_rmd(session_t *sess)
{
    if(rmdir(sess->args) == -1)
    {
        //550 Remove directory operation failed.
        ftp_reply(sess, FTP_FILEFAIL, "Remove directory operation failed.");
        return;
    }
    //250 Remove directory operation successful.
    ftp_reply(sess, FTP_RMDIROK, "Remove directory operation successful.");
}

//ɾ���ļ�
void do_dele(session_t *sess)
{
    if(unlink(sess->args) == -1)
    {
        //550 Delete operation failed.
        ftp_reply(sess, FTP_FILEFAIL, "Delete operation failed.");
        return;
    }
    //250 Delete operation successful.
    ftp_reply(sess, FTP_DELEOK, "Delete operation successful.");
}

//�ļ�������ʱ�ĵ�һ���������ļ���������
void do_rnfr(session_t *sess)
{
    sess->rnfr_name = (char *)malloc(strlen(sess->args) + 1);
    strcpy(sess->rnfr_name, sess->args);
    //350 Ready for RNTO
    ftp_reply(sess, FTP_RNFROK, "Ready for RNTO");
}

//�ļ�������ʱ�ĵڶ����������ļ����������������ļ���
void do_rnto(session_t *sess)
{
    if(sess->rnfr_name == NULL){
        //503 RNFR required first.
        ftp_reply(sess, FTP_NEEDRNFR, "RNFR required first.");
        return;
    }
    if(rename(sess->rnfr_name, sess->args) == -1){
        ftp_reply(sess, FTP_FILEFAIL, "Rename failed.");
        free(sess->rnfr_name);
        sess->rnfr_name = NULL;
        return;
    }
    free(sess->rnfr_name);
    sess->rnfr_name = NULL;

    //250 Rename successful
    ftp_reply(sess, FTP_RENAMEOK, "Rename successful.");
}

void do_site(session_t *sess)
{
    char cmd[1024] = {0};
    char args[1024] = {0};

    str_split(sess->args, cmd, args, ' ');
    str_upper(cmd);

    if(strcmp("CHMOD", cmd))
        do_site_chmod(sess, args);
    else if(strcmp("UMASK", cmd))
        do_site_umask(sess, args);
    else if(strcmp("HELP", cmd))
        do_site_help(sess);
    else
        ftp_reply(sess, FTP_BADCMD, "Unknown SITE command.");
}

void do_syst(session_t *sess)
{
    ftp_reply(sess, FTP_SYSTOK, "UNIX Type: L8");
}

void do_feat(session_t *sess)
{
    //211-Features:
    ftp_lreply(sess, FTP_FEAT, "Features:");

    //EPRT
    writen(sess->peerfd, " EPRT\r\n", strlen(" EPRT\r\n"));
    writen(sess->peerfd, " EPSV\r\n", strlen(" EPSV\r\n"));
    writen(sess->peerfd, " MDTM\r\n", strlen(" MDTM\r\n"));
    writen(sess->peerfd, " PASV\r\n", strlen(" PASV\r\n"));
    writen(sess->peerfd, " REST STREAM\r\n", strlen(" REST STREAM\r\n"));
    writen(sess->peerfd, " SIZE\r\n", strlen(" SIZE\r\n"));
    writen(sess->peerfd, " TVFS\r\n", strlen(" TVFS\r\n"));
    writen(sess->peerfd, " UTF8\r\n", strlen(" UTF8\r\n"));

    //211 End
    ftp_reply(sess, FTP_FEAT, "End");
}

//�鿴�ļ��Ĵ�С����֧���ļ��У�
void do_size(session_t *sess)
{
    struct stat sbuf;
    if(lstat(sess->args, &sbuf) == -1){
        ftp_reply(sess, FTP_FILEFAIL, "SIZE operation failed.");
        return;
    }
    
    if(!S_ISREG(sbuf.st_mode)){
        ftp_reply(sess, FTP_FILEFAIL, "SIZE operation failed.");
        return;
    }

    char text[1024] = {0};
    snprintf(text, sizeof(text), "%lu", sbuf.st_size);
    ftp_reply(sess, FTP_SIZEOK, text);
}

void do_stat(session_t *sess)
{
    ftp_lreply(sess, FTP_STATOK, "FTP server status:");

    char text[1024] = {0};
    struct in_addr in;
    in.s_addr = sess->ip;
    snprintf(text, sizeof text, "     Connected to %s\r\n", inet_ntoa(in));
    writen(sess->peerfd, text, strlen(text));

    snprintf(text, sizeof text, "     Logged in as %s\r\n", sess->username);
    writen(sess->peerfd, text, strlen(text));

    ftp_reply(sess, FTP_STATOK, "End of status");
}

void do_noop(session_t *sess)
{
    ftp_reply(sess, FTP_GREET, "(FtpServer 1.0)");
}

void do_help(session_t *sess)
{
    ftp_lreply(sess, FTP_HELP, "The following commands are recognized.");
    writen(sess->peerfd, " ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n", strlen(" ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n"));
    writen(sess->peerfd, " MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n", strlen(" MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n"));
    writen(sess->peerfd, " RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n", strlen(" RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n"));
    writen(sess->peerfd, " XPWD XRMD\r\n", strlen(" XPWD XRMD\r\n"));
    ftp_reply(sess, FTP_HELP, "Help OK.");
}
