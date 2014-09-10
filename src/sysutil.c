#include "sysutil.h"

static int lock_file(int fd, int type);

static struct timeval tv = {0, 0};

void activate_oobinline(int sockfd)
{
    int oob_inline = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_OOBINLINE, &oob_inline, sizeof(oob_inline)) == -1)
        ERR_EXIT("setsockopt oobinline");
}

void activate_signal_sigurg(int sockfd)
{
    if(fcntl(sockfd, F_SETOWN, getpid()) == -1)
        ERR_EXIT("fcntl sigurg.");
}

int get_curr_time_sec()
{
    if(gettimeofday(&tv, NULL) == -1)
        ERR_EXIT("gettimeofday");
    return tv.tv_sec;
}

int get_curr_time_usec()
{
    return tv.tv_usec;
}

int nano_sleep(double t)
{
    int sec = (time_t)t; //只取整数部分
    int nsec = (t - sec) * 1000000000;
    struct timespec ts;
    ts.tv_sec = sec;
    ts.tv_nsec = nsec;

    //当睡眠被打断时， 剩余时间放到ts里面
    int ret;
    do{
        ret = nanosleep(&ts, &ts);
    }while(ret == -1 && errno == EINTR);

    return ret;
}

//下载时锁定文件
int lock_file_read(int fd)
{
    return lock_file(fd, F_RDLCK);
}

//上传时锁定文件
int lock_file_write(int fd)
{
    return lock_file(fd, F_WRLCK);
}

static int lock_file(int fd, int type)
{
    struct flock lock;
    memset(&lock, 0, sizeof lock);
    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();

    int ret;
    do{
        ret = fcntl(fd, F_SETLKW, &lock);
    }while(ret == -1 && errno == EINTR);

    return ret;
}

int unlock_file(int fd)
{
    struct flock lock;
    memset(&lock, 0, sizeof lock);
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();

    return fcntl(fd, F_SETLK, &lock);
}

int tcp_client(unsigned int port)
{
    int sockfd;
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        ERR_EXIT("socket");

    if(port > 0)
    {
        int on = 1;
        if((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) == -1)
            ERR_EXIT("setsockopt");

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        char ip[16] = {0};
        get_local_ip(ip);
        addr.sin_addr.s_addr = inet_addr(ip);

        if(bind(sockfd, (struct sockaddr *)&addr, sizeof addr) == -1)
            ERR_EXIT("bind");
    }

    return sockfd;
}

/**
 * tcp_server - 启动tcp服务器
 * @host: 服务器IP地址或者服务器主机名
 * @port: 服务器端口
 * 成功返回监听套接字
 */
int tcp_server(const char *host, unsigned short port)
{
	int listenfd;
	if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		ERR_EXIT("tcp_server");

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
    /*
     *一共就两种情况
     *1.参数host为NULL
     *2.参数host为IP地址，IP地址
     */
	if (host != NULL)
	{
		if (inet_aton(host, &servaddr.sin_addr) == 0)
		{
			struct hostent *hp;
            //这个函数用来根据域名或者主机名得到一个hostent结构
			hp = gethostbyname(host);
			if (hp == NULL)
				ERR_EXIT("gethostbyname");

			servaddr.sin_addr = *(struct in_addr*)hp->h_addr;
		}
	}
	else
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	servaddr.sin_port = htons(port);

	int on = 1;
	if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on))) < 0)
		ERR_EXIT("setsockopt");

	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		ERR_EXIT("bind");

	if (listen(listenfd, SOMAXCONN) < 0)
		ERR_EXIT("listen");

	return listenfd;
}

int get_local_ip(char *ip)
{
    /*
	char host[100] = {0};
	if (gethostname(host, sizeof(host)) < 0)
		return -1;
	struct hostent *hp;
	if ((hp = gethostbyname(host)) == NULL)
	return -1;

	strcpy(ip, inet_ntoa(*(struct in_addr*)hp->h_addr));
	return 0;
    */

    int sockfd; 
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        ERR_EXIT("socket");
    }

    struct ifreq req;

    bzero(&req, sizeof(struct ifreq));
    strcpy(req.ifr_name, "eth0");

    if(ioctl(sockfd, SIOCGIFADDR, &req) == -1)
        ERR_EXIT("ioctl");

    struct sockaddr_in *host = (struct sockaddr_in*)&req.ifr_addr;
    strcpy(ip, inet_ntoa(host->sin_addr));
    close(sockfd);
    return 1;
}


/**
 * activate_noblock - 设置I/O为非阻塞模式
 * @fd: 文件描符符
 */
void activate_nonblock(int fd)
{
	int ret;
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		ERR_EXIT("fcntl");

	flags |= O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, flags);
	if (ret == -1)
		ERR_EXIT("fcntl");
}

/**
 * deactivate_nonblock - 设置I/O为阻塞模式
 * @fd: 文件描符符
 */
void deactivate_nonblock(int fd)
{
	int ret;
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		ERR_EXIT("fcntl");

	flags &= ~O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, flags);
	if (ret == -1)
		ERR_EXIT("fcntl");
}


/**
 * read_timeout - 读超时检测函数，不含读操作
 * @fd: 文件描述符
 * @wait_seconds: 等待超时秒数，如果为0表示不检测超时
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 */
int read_timeout(int fd, unsigned int wait_seconds)
{
	int ret = 0;
	if (wait_seconds > 0)
	{
		fd_set read_fdset;
		struct timeval timeout;

		FD_ZERO(&read_fdset);
		FD_SET(fd, &read_fdset);

		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			ret = select(fd + 1, &read_fdset, NULL, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);

		if (ret == 0)
		{
			ret = -1;
			errno = ETIMEDOUT;
		}
		else if (ret == 1)
			ret = 0;
	}

	return ret;
}

/**
 * write_timeout - 读超时检测函数，不含写操作
 * @fd: 文件描述符
 * @wait_seconds: 等待超时秒数，如果为0表示不检测超时
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 */
int write_timeout(int fd, unsigned int wait_seconds)
{
	int ret = 0;
	if (wait_seconds > 0)
	{
		fd_set write_fdset;
		struct timeval timeout;

		FD_ZERO(&write_fdset);
		FD_SET(fd, &write_fdset);

		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			ret = select(fd + 1, NULL, &write_fdset, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);

		if (ret == 0)
		{
			ret = -1;
			errno = ETIMEDOUT;
		}
		else if (ret == 1)
			ret = 0;
	}

	return ret;
}

/**
 * accept_timeout - 带超时的accept
 * @fd: 套接字
 * @addr: 输出参数，返回对方地址
 * @wait_seconds: 等待超时秒数，如果为0表示正常模式
 * 成功（未超时）返回已连接套接字，超时返回-1并且errno = ETIMEDOUT
 */
int accept_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
	int ret;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	if (wait_seconds > 0)
	{
		fd_set accept_fdset;
		struct timeval timeout;
		FD_ZERO(&accept_fdset);
		FD_SET(fd, &accept_fdset);
		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			ret = select(fd + 1, &accept_fdset, NULL, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);
		if (ret == -1)
			return -1;
		else if (ret == 0)
		{
			errno = ETIMEDOUT;
			return -1;
		}
	}

	if (addr != NULL)
		ret = accept(fd, (struct sockaddr*)addr, &addrlen);
	else
		ret = accept(fd, NULL, NULL);
/*	if (ret == -1)
		ERR_EXIT("accept");
		*/

	return ret;
}

/**
 * connect_timeout - connect
 * @fd: 套接字
 * @addr: 要连接的对方地址
 * @wait_seconds: 等待超时秒数，如果为0表示正常模式
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 */
int connect_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
	int ret;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	if (wait_seconds > 0)
		activate_nonblock(fd);

	ret = connect(fd, (struct sockaddr*)addr, addrlen);
	if (ret < 0 && errno == EINPROGRESS)
	{
		printf("AAAAA\n");
		fd_set connect_fdset;
		struct timeval timeout;
		FD_ZERO(&connect_fdset);
		FD_SET(fd, &connect_fdset);
		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			/* 一量连接建立，套接字就可写 */
			ret = select(fd + 1, NULL, &connect_fdset, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);
		if (ret == 0)
		{
			ret = -1;
			errno = ETIMEDOUT;
		}
		else if (ret < 0)
			return -1;
		else if (ret == 1)
		{
			printf("BBBBB\n");
			/* ret返回为1，可能有两种情况，一种是连接建立成功，一种是套接字产生错误，*/
			/* 此时错误信息不会保存至errno变量中，因此，需要调用getsockopt来获取。 */
			int err;
			socklen_t socklen = sizeof(err);
			int sockoptret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &socklen);
			if (sockoptret == -1)
			{
				return -1;
			}
			if (err == 0)
			{
				printf("DDDDDDD\n");
				ret = 0;
			}
			else
			{
				printf("CCCCCC\n");
				errno = err;
				ret = -1;
			}
		}
	}
	if (wait_seconds > 0)
	{
		deactivate_nonblock(fd);
	}
	return ret;
}

/**
 * readn - 读取固定字节数
 * @fd: 文件描述符
 * @buf: 接收缓冲区
 * @count: 要读取的字节数
 * 成功返回count，失败返回-1，读到EOF返回<count
 */
ssize_t readn(int fd, void *buf, size_t count)
{
	size_t nleft = count;
	ssize_t nread;
	char *bufp = (char*)buf;

	while (nleft > 0)
	{
		if ((nread = read(fd, bufp, nleft)) < 0)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}
		else if (nread == 0)
			return count - nleft;

		bufp += nread;
		nleft -= nread;
	}

	return count;
}

/**
 * writen - 发送固定字节数
 * @fd: 文件描述符
 * @buf: 发送缓冲区
 * @count: 要读取的字节数
 * 成功返回count，失败返回-1
 */
ssize_t writen(int fd, const void *buf, size_t count)
{
	size_t nleft = count;
	ssize_t nwritten;
	char *bufp = (char*)buf;

	while (nleft > 0)
	{
		if ((nwritten = write(fd, bufp, nleft)) < 0)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}
		else if (nwritten == 0)
			continue;

		bufp += nwritten;
		nleft -= nwritten;
	}

	return count;
}

/**
 * recv_peek - 仅仅查看套接字缓冲区数据，但不移除数据
 * @sockfd: 套接字
 * @buf: 接收缓冲区
 * @len: 长度
 * 成功返回>=0，失败返回-1
 */
ssize_t recv_peek(int sockfd, void *buf, size_t len) {
    int nread;
    while (1) {
        //这个过程只成功调用一次
        nread = recv(sockfd, buf, len, MSG_PEEK);
        if (nread < 0 && errno == EINTR) {  //被中断则继续读取
            continue;
        }
        if (nread < 0) {
            return -1;
        }
        break;
    }
    return nread;
}

/**
 * readline - 按行读取数据
 * @sockfd: 套接字
 * @buf: 接收缓冲区
 * @maxline: 每行最大长度
 * 成功返回>=0，失败返回-1
 */
ssize_t readline(int sockfd, void *buf, size_t maxline) {
    int nread;  //一次IO读取的数量
    int nleft;  //还剩余的字节数
    char *ptr;  //存放数据的指针的位置
    int ret;    //readn的返回值
    int total = 0;  //目前总共读取的字节数

    nleft = maxline-1;
    ptr = buf;

    while (nleft > 0) {
        //这一次调用仅仅是预览数据
        //并没有真的把数据从缓冲区中取走
        ret = recv_peek(sockfd, ptr, nleft);
        //注意这里读取的字节不够，绝对不是错误！！！
        if (ret <= 0) {
            return ret;
        }

        nread = ret;
        int i;
        for (i = 0; i < nread; ++i) {
            if (ptr[i] == '\n') {
                //这里才是真正的读取过程
                ret = readn(sockfd, ptr, i + 1);
                if (ret != i + 1) {
                    return -1;
                }
                total += ret;
                ptr += ret;
                *ptr = 0;
                return total;   //返回此行的长度 '\n'包含在其中
            }
        }
        //如果没有发现\n，这些数据应全部接收
        ret = readn(sockfd, ptr, nread);
        if (ret != nread) {
            return -1;
        }
        nleft -= nread;
        total += nread;
        ptr += nread;
    }
    *ptr = 0;
    return maxline-1;
}

void send_fd(int sock_fd, int fd)
{
	int ret;
	struct msghdr msg;
	struct cmsghdr *p_cmsg;
	struct iovec vec;
	char cmsgbuf[CMSG_SPACE(sizeof(fd))];
	int *p_fds;
	char sendchar = 0;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	p_cmsg = CMSG_FIRSTHDR(&msg);
	p_cmsg->cmsg_level = SOL_SOCKET;
	p_cmsg->cmsg_type = SCM_RIGHTS;
	p_cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	p_fds = (int*)CMSG_DATA(p_cmsg);
	*p_fds = fd;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;

	vec.iov_base = &sendchar;
	vec.iov_len = sizeof(sendchar);
	ret = sendmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("sendmsg");
}

int recv_fd(const int sock_fd)
{
	int ret;
	struct msghdr msg;
	char recvchar;
	struct iovec vec;
	int recv_fd;
	char cmsgbuf[CMSG_SPACE(sizeof(recv_fd))];
	struct cmsghdr *p_cmsg;
	int *p_fd;
	vec.iov_base = &recvchar;
	vec.iov_len = sizeof(recvchar);
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	msg.msg_flags = 0;

	p_fd = (int*)CMSG_DATA(CMSG_FIRSTHDR(&msg));
	*p_fd = -1;  
	ret = recvmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("recvmsg");

	p_cmsg = CMSG_FIRSTHDR(&msg);
	if (p_cmsg == NULL)
		ERR_EXIT("no passed fd");


	p_fd = (int*)CMSG_DATA(p_cmsg);
	recv_fd = *p_fd;
	if (recv_fd == -1)
		ERR_EXIT("no passed fd");

	return recv_fd;
}

