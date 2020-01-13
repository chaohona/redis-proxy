#ifndef _GR_DAEMONIZE_H__
#define _GR_DAEMONIZE_H__
#include "define.h"
#include "log.h"
#include <stdlib.h>
#include <signal.h>
static int
GR_Daemonize(int dump_core)
{
    int status;
    pid_t pid, sid;
    int fd;

    pid = fork();
    switch (pid) {
    case -1:
        GR_STDERR("fork() failed: %s", strerror(errno));
        return GR_ERROR;

    case 0:
        break;

    default:
        /* parent terminates */
        exit(0);
    }

    /* 1st child continues and becomes the session leader */

    sid = setsid();
    if (sid < 0) {
        GR_STDERR("setsid() failed: %s", strerror(errno));
        return GR_ERROR;
    }

    umask(0);

    /* redirect stdin, stdout and stderr to "/dev/null" */

    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        GR_STDERR("open(\"/dev/null\") failed: %s", strerror(errno));
        return GR_ERROR;
    }

    status = dup2(fd, STDIN_FILENO);
    if (status < 0) {
        GR_STDERR("dup2(%d, STDIN) failed: %s", fd, strerror(errno));
        close(fd);
        return GR_ERROR;
    }

    status = dup2(fd, STDOUT_FILENO);
    if (status < 0) {
        GR_STDERR("dup2(%d, STDOUT) failed: %s", fd, strerror(errno));
        close(fd);
        return GR_ERROR;
    }

    status = dup2(fd, STDERR_FILENO);
    if (status < 0) {
        GR_STDERR("dup2(%d, STDERR) failed: %s", fd, strerror(errno));
        close(fd);
        return GR_ERROR;
    }

    if (fd > STDERR_FILENO) {
        status = close(fd);
        if (status < 0) {
            GR_STDERR("close(%d) failed: %s", fd, strerror(errno));
            return GR_ERROR;
        }
    }

    return GR_OK;
}


#endif
