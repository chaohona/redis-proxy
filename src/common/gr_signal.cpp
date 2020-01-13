#include "gr_signal.h"
#include "include.h"
#include "utils.h"
#include "define.h"

#include <stdlib.h>
#include <signal.h>

GR_SignalFlag   gr_global_signal = GR_SignalFlag();

void GR_SignalFlag::Reset()
{
    memset(this, 0, sizeof(GR_SignalFlag));
}

GR_Signal vsignals[] = {
    { SIGCHLD, "SIGCHLD", 0,                 GR_SignalHandler },
    { SIGUSR1, "SIGUSR1", 0,                 GR_SignalHandler },
    { SIGUSR2, "SIGUSR2", 0,                 GR_SignalHandler },
    { SIGTTIN, "SIGTTIN", 0,                 GR_SignalHandler },
    { SIGTTOU, "SIGTTOU", 0,                 GR_SignalHandler },
    { SIGHUP,  "SIGHUP",  0,                 SIG_IGN },
    { SIGINT,  "SIGINT",  0,                 GR_SignalHandler },
    { SIGSEGV, "SIGSEGV", (int)SA_RESETHAND, GR_SignalHandler },
    { SIGPIPE, "SIGPIPE", 0,                 SIG_IGN }, // 写以关闭的socket
    { SIGQUIT, "SIGQUIT", 0,                 GR_SignalHandler },
    { SIGTERM, "SIGTERM", 0,                 GR_SignalHandler },
    { SIGALRM, "SIGALRM", 0,                 GR_SignalHandler },
    { 0,        NULL,     0,                 NULL }
};

void
GR_SignalHandler(int signo)
{
    struct GR_Signal *sig;
    void (*action)(void);
    char *actionstr;
    bool done;

    for (sig = vsignals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }
    ASSERT(sig->signo != 0);

    actionstr = "";
    action = NULL;
    done = false;

    switch (signo) {
    case SIGUSR1:
        gr_global_signal.NewChild = 1;
        break;

    case SIGUSR2:
        gr_global_signal.Expansion = 2;
        break;

    case SIGTTIN:
        actionstr = ", up logging level";
        break;

    case SIGTTOU:
        actionstr = ", down logging level";
        break;

    case SIGHUP:
        actionstr = ", reopening log file";
        break;

    case SIGINT:
        done = true;
        actionstr = ", exiting";
        raise(SIGINT);
        break;

    case SIGSEGV:
        //GR_Stacktrace();
        actionstr = ", core dumping";
        raise(SIGSEGV);
        break;
    case SIGQUIT:
    {
        gr_global_signal.Exit = 1;
        break;
    }
    case SIGTERM:
    {
        gr_global_signal.Exit = 1;
        break;
    }
    case SIGCHLD:
    {
        gr_global_signal.ChildExit = 1;
        break;
    }
    case SIGALRM:
    {
        break;
    }
    default:
        NOT_REACHED();
    }

    if (signo != SIGALRM)
    {
        //cout << "receive signal " << sig->signame << endl;
    }

    if (action != NULL) {
        action();
    }

    if (done) {
        exit(1);
    }
}

int GR_SignalInit()
{
    struct GR_Signal *sig;

    for (sig = vsignals; sig->signo != 0; sig++) {
        int status;
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig->handler;
        sa.sa_flags = sig->flags;
        sigemptyset(&sa.sa_mask);

        status = sigaction(sig->signo, &sa, NULL);
        if (status < 0) {
            GR_STDERR("sigaction(%s) failed: %s", sig->signame,
                      strerror(errno));
            return GR_ERROR;
        }
    }

    return GR_OK;
}

int SetSysTimer(int iMS)
{
    itimerval   itv;
    itv.it_interval.tv_sec = iMS / 1000;
    itv.it_interval.tv_usec = (iMS % 1000 ) * 1000;
    itv.it_value.tv_sec = iMS / 1000;
    itv.it_value.tv_usec = (iMS % 1000 ) * 1000;

    if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
        GR_STDERR("setitimer failed, errno:%d, errmsg:%s", errno, strerror(errno));
        return GR_ERROR;
    }

    return GR_OK;
}

