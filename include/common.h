#ifndef COMMON_H
#define COMMON_H 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <dirent.h>
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/capability.h>
#include <sys/syscall.h>
#include <bits/syscall.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <assert.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)


#endif  /*COMMON_H*/
