#include "trans_data.h"
#include "common.h"
#include "sysutil.h"
#include "ftp_codes.h"
#include "command_map.h"
#include "configure.h"
#include "priv_sock.h"
#include "trans_ctrl.h"

static const char *statbuf_get_perms(struct stat *sbuf);
static const char *statbuf_get_date(struct stat *sbuf);
static const char *statbuf_get_filename(struct stat *sbuf, const char *name);
static const char *statbuf_get_user_info(struct stat *sbuf);
static const char *statbuf_get_size(struct stat *sbuf);

static int is_port_active(session_t *sess);
static int is_pasv_active(session_t *sess);

static void get_port_data_fd(session_t *sess);
static void get_pasv_data_fd(session_t *sess);


void download_file(session_t *sess)
{
    //进入数据传输阶段
    sess->is_translating_data = 1;

    //获取data fd
    if(get_trans_data_fd(sess) == 0){
        ftp_reply(sess, FTP_FILEFAIL, "Failed to open file.");
        return;
    }

    //open文件
    int fd = open(sess->args, O_RDONLY);
    if(fd == -1){
        ftp_reply(sess, FTP_FILEFAIL, "Failed to open file.");
        return;
    }

    //对文件加锁
    if(lock_file_read(fd) == -1){
        ftp_reply(sess, FTP_FILEFAIL, "Failed to open file.");
        return;
    }

    //判断是否是普通文件
    struct stat sbuf;
    if(fstat(fd, &sbuf) == -1)
        ERR_EXIT("fstat");
    if(!S_ISREG(sbuf.st_mode)){
        ftp_reply(sess, FTP_FILEFAIL, "Can only download regular file.");
        return;
    }

    //判断断点续传
    unsigned long filesize = sbuf.st_size;
    int offset = sess->restart_pos;
    if(offset != 0)
        filesize -= offset;
    if(lseek(fd, offset, SEEK_SET) == -1)
        ERR_EXIT("lseek");

    //150 ascii
    char text[1024] = {0};
    if(sess->ascii_mode == 1)
        snprintf(text, sizeof text, "Opening ASCII mode data connection for %s (%lu bytes).", sess->args, filesize);
    else
        snprintf(text, sizeof text, "Opening Binary mode data connection for %s (%lu bytes).", sess->args, filesize);
    ftp_reply(sess, FTP_DATACONN, text);

    //记录时间
    sess->start_time_sec = get_curr_time_sec();
    sess->start_time_usec = get_curr_time_usec();

    int flag = 0;
    int nleft = filesize;
    int block_size = 0;
    const int kSize = 65536;
    while(nleft > 0){
        block_size = (nleft > kSize) ? kSize : nleft;
        int nwrite = sendfile(sess->data_fd, fd, NULL, block_size);

        if(sess->is_receive_abor == 1){
            flag = 2; //ABOR
            //426
            ftp_reply(sess, FTP_BADSENDNET, "Interupt downloading file.");
            sess->is_receive_abor = 0;
            break;
        }

        if(nwrite == -1){
            flag = 1; //错误
            break;
        }
        nleft -= nwrite;

        //实行限速
        limit_curr_rate(sess, nwrite, 0);
    }
    if(nleft == 0)
        flag = 0; // 正确退出

    if(unlock_file(fd) == -1)
        ERR_EXIT("unlock_file");
    close(fd);
    close(sess->data_fd);
    sess->data_fd = -1;

    if(flag == 0)
        ftp_reply(sess, FTP_TRANSFEROK, "Transfer complete.");
    else if(flag == 1)
        ftp_reply(sess, FTP_BADSENDFILE, "Sendfile failed.");
    else if(flag == 2)
        ftp_reply(sess, FTP_ABOROK, "ABOR successful.");

    //恢复控制连接的信号
    setup_signal_alarm_ctrl_fd();
    sess->is_translating_data = 0;
}

//上传文件
void upload_file(session_t *sess, int is_appe)
{
    //进入数据传输阶段
    sess->is_translating_data = 1;

    //获取data fd
    if(get_trans_data_fd(sess) == 0){
        ftp_reply(sess, FTP_UPLOADFAIL, "Failed to get data fd.");
        return;
    } 

    //open file
    int fd = open(sess->args, O_WRONLY | O_CREAT, 0666);
    if(fd == -1){
        ftp_reply(sess, FTP_UPLOADFAIL, "Failed to open file.");
        return;
    }

    //对文件加锁
    if(lock_file_write(fd) == -1){
        ftp_reply(sess, FTP_UPLOADFAIL, "Failed to lock file.");
        return;
    }

    //判断是否为普通文件
    struct stat sbuf;
    if(fstat(fd, &sbuf) == -1)
        ERR_EXIT("fstat");
    if(!S_ISREG(sbuf.st_mode)){
        ftp_reply(sess, FTP_UPLOADFAIL, "Can only upload regular file.");
        return;
    }

    //区分模式
    long long offset = sess->restart_pos;
    unsigned long filesize = 0;
    if(!is_appe && offset == 0)
        ftruncate(fd, 0);
    else if(!is_appe && offset != 0){
        ftruncate(fd, offset);
        if(lseek(fd, offset, SEEK_SET) == -1)
            ERR_EXIT("lseek");
        filesize = offset;
    }
    else{
        if(lseek(fd, 0, SEEK_END) == -1)
            ERR_EXIT("fstat");
        
        if(fstat(fd, &sbuf) == -1)
            ERR_EXIT("fstat");
        filesize = sbuf.st_size;
    }

    //150 ascii
    //150 Opening ASCII mode data connection for /home/wing/redis-stable.tar.gz (1251318 bytes).
    char text[1024] = {0};
    if(sess->ascii_mode == 1)
        snprintf(text, sizeof text, "Opening ASCII mode data connection for %s (%lu bytes).", sess->args, filesize);
    else
        snprintf(text, sizeof text, "Opening Binary mode data connection for %s (%lu bytes).", sess->args, filesize);
    ftp_reply(sess, FTP_DATACONN, text);

    sess->start_time_sec = get_curr_time_sec();
    sess->start_time_usec = get_curr_time_usec();

    //上传阶段
    char buf[65536] = {0};
    int flag = 0;
    while(1){
        int nread = read(sess->data_fd, buf, sizeof buf);

        if(sess->is_receive_abor == 1){
            flag = 3; //ABOR
            ftp_reply(sess, FTP_BADSENDNET, "Interupt uploading file.");
            sess->is_receive_abor = 0;
            break;
        }

        if(nread == -1){
            if(errno == EINTR)
                continue;
            flag = 1;
            break;
        }
        else if(nread == 0){
            flag = 0;
            break;
        }

        if(writen(fd, buf, nread) != nread){
            flag = 2;
            break;
        }

        //实行限速
        limit_curr_rate(sess, nread, 1);
    }

    //关闭fd 文件解锁
    if(unlock_file(fd) == -1)
        ERR_EXIT("unlock_file");
    close(fd);
    close(sess->data_fd);
    sess->data_fd = -1;

    if(flag == 0)
        ftp_reply(sess, FTP_TRANSFEROK, "Transfer complete.");
    else if(flag == 1)
        ftp_reply(sess, FTP_BADSENDNET, "Reading from Network Failed.");
    else if(flag == 2)
        ftp_reply(sess, FTP_BADSENDFILE, "Writing to File Failed.");
    else if(flag == 3)
        ftp_reply(sess, FTP_ABOROK, "ABOR successful.");

    //恢复控制连接的信号
    setup_signal_alarm_ctrl_fd();
    sess->is_translating_data = 0;
}



//返回值表示成功与否
int get_trans_data_fd(session_t *sess)
{
    int is_port = is_port_active(sess);
    int is_pasv = is_pasv_active(sess);


    //两者都未开启，应返回425
    if(!is_port && !is_pasv)
    {
        ftp_reply(sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
        return 0;
    }

    if(is_port && is_pasv)
    {
        fprintf(stderr, "both of PORT and PASV are active\n");
        exit(EXIT_FAILURE);
    }

    //主动模式
    if(is_port)
        get_port_data_fd(sess);
    //被动模式
    if(is_pasv)
        get_pasv_data_fd(sess);

    //这里获取data fd 成功
    //安装信号
    setup_signal_alarm_data_fd();
    //开始计时
    start_signal_alarm_data_fd();
    return 1;
}

void trans_list_detail(session_t *sess)
{
    trans_list_common(sess, 1);    
}

void trans_list_simple(session_t *sess)
{
    trans_list_common(sess, 0);    
}

void trans_list_common(session_t *sess, int kind)
{
    DIR *dir = opendir(".");
    if(dir == NULL)
        ERR_EXIT("opendir");

    struct dirent *dr;
    while((dr = readdir(dir)))
    {
        const char *filename = dr->d_name;
        if(filename[0] == '.')
            continue;

        char buf[1024] = {0};
        struct stat sbuf;
        if(lstat(filename, &sbuf) == -1)
            ERR_EXIT("lstat");

        if(kind){
            strcpy(buf, statbuf_get_perms(&sbuf));
            strcat(buf, " ");
            strcat(buf, statbuf_get_user_info(&sbuf));
            strcat(buf, " ");
            strcat(buf, statbuf_get_size(&sbuf));
            strcat(buf, " ");
            strcat(buf, statbuf_get_date(&sbuf));
            strcat(buf, " ");
            strcat(buf, statbuf_get_filename(&sbuf, filename));
        }
        else
            strcpy(buf, statbuf_get_filename(&sbuf, filename));

        strcat(buf, "\r\n");

        writen(sess->data_fd, buf, strlen(buf));
    }

    closedir(dir);
}

void trans_list(session_t *sess, int kind)
{
    //发起数据连接
    if(get_trans_data_fd(sess) == 0)
        return ;

    //给出150 Here comes the directory listing.
    ftp_reply(sess, FTP_DATACONN, "Here comes the directory listing.");

    //传输目录列表
    if(kind)
        trans_list_detail(sess);
    else
        trans_list_simple(sess);

    close(sess->data_fd);   //传输结束记得关闭
    sess->data_fd = -1;

    //给出226 Directory send OK.
    ftp_reply(sess, FTP_TRANSFEROK, "Directory send OK.");

}

static const char *statbuf_get_perms(struct stat *sbuf)
{
    static char perms[] = "----------";
    mode_t mode = sbuf->st_mode;

    //文件类型
    switch(mode & S_IFMT)
    {
        case S_IFSOCK:
            perms[0] = 's';
            break;
        case S_IFLNK:
            perms[0] = 'l';
            break;
        case S_IFREG:
            perms[0] = '-';
            break;
        case S_IFBLK:
            perms[0] = 'b';
            break;
        case S_IFDIR:
            perms[0] = 'd';
            break;
        case S_IFCHR:
            perms[0] = 'c';
            break;
        case S_IFIFO:
            perms[0] = 'p';
            break;
    }

    //权限
    if(mode & S_IRUSR)
        perms[1] = 'r';
    if(mode & S_IWUSR)
        perms[2] = 'w';
    if(mode & S_IXUSR)
        perms[3] = 'x';
    if(mode & S_IRGRP)
        perms[4] = 'r';
    if(mode & S_IWGRP)
        perms[5] = 'w';
    if(mode & S_IXGRP)
        perms[6] = 'x';
    if(mode & S_IROTH)
        perms[7] = 'r';
    if(mode & S_IWOTH)
        perms[8] = 'w';
    if(mode & S_IXOTH)
        perms[9] = 'x';

    if(mode & S_ISUID)
        perms[3] = (perms[3] == 'x') ? 's' : 'S';
    if(mode & S_ISGID)
        perms[6] = (perms[6] == 'x') ? 's' : 'S';
    if(mode & S_ISVTX)
        perms[9] = (perms[9] == 'x') ? 't' : 'T';

    return perms;
}

static const char *statbuf_get_date(struct stat *sbuf)
{
    static char datebuf[1024] = {0};
    struct tm *ptm;
    time_t ct = sbuf->st_ctime;
    if((ptm = localtime(&ct)) == NULL)
        ERR_EXIT("localtime");

    const char *format = "%b %e %H:%M"; //时间格式

    if(strftime(datebuf, sizeof datebuf, format, ptm) == 0)
    {
        fprintf(stderr, "strftime error\n");
        exit(EXIT_FAILURE);
    }

    return datebuf;
}

static const char *statbuf_get_filename(struct stat *sbuf, const char *name)
{
    static char filename[1024] = {0};
    //name 处理链接名字
    if(S_ISLNK(sbuf->st_mode))
    {
        char linkfile[1024] = {0};
        if(readlink(name, linkfile, sizeof linkfile) == -1)
            ERR_EXIT("readlink");
        snprintf(filename, sizeof filename, " %s -> %s", name, linkfile);
    }else
    {
        strcpy(filename, name);
    }

    return filename;
}

static const char *statbuf_get_user_info(struct stat *sbuf)
{
    static char info[1024] = {0};
    snprintf(info, sizeof info, " %3d %8d %8d", sbuf->st_nlink, sbuf->st_uid, sbuf->st_gid);

    return info;
}

static const char *statbuf_get_size(struct stat *sbuf)
{
    static char buf[100] = {0};
    snprintf(buf, sizeof buf, "%8lu", (unsigned long)sbuf->st_size);
    return buf;
}


static int is_port_active(session_t *sess)
{
    return (sess->p_addr != NULL);
}

static int is_pasv_active(session_t *sess)
{
    //给nobody进程发送命令，需要调用priv_sock_pasv_active函数
    priv_sock_send_cmd(sess->proto_fd, PRIV_SOCK_PASV_ACTIVE);

    //接受结果，并作为返回值返回
    return priv_sock_recv_int(sess->proto_fd);
}

static void get_port_data_fd(session_t *sess)
{
    priv_sock_send_cmd(sess->proto_fd, PRIV_SOCK_GET_DATA_SOCK);

    char *ip = inet_ntoa(sess->p_addr->sin_addr);
    uint16_t port = ntohs(sess->p_addr->sin_port);
    priv_sock_send_str(sess->proto_fd, ip, strlen(ip));
    priv_sock_send_int(sess->proto_fd, port);

    char ret = priv_sock_recv_result(sess->proto_fd);
    if(ret == PRIV_SOCK_RESULT_BAD){
        ftp_reply(sess, FTP_BADCMD, "get port data_fd error.");
        fprintf(stderr, "get data fd error.\n");
        exit(EXIT_FAILURE);
    }

    sess->data_fd = priv_sock_recv_fd(sess->proto_fd);

    free(sess->p_addr);
    sess->p_addr = NULL;
}

static void get_pasv_data_fd(session_t *sess)
{
    priv_sock_send_cmd(sess->proto_fd, PRIV_SOCK_PASV_ACCEPT);

    char res = priv_sock_recv_result(sess->proto_fd);
    if(res == PRIV_SOCK_RESULT_BAD){
        ftp_reply(sess, FTP_BADCMD, "get pasv data_fd error.");
        fprintf(stderr, "get data fd error.\n");
        exit(EXIT_FAILURE);
    }

    sess->data_fd = priv_sock_recv_fd(sess->proto_fd);
}


