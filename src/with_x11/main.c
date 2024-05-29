#include "gui.h"
#include <clubar.h>
#include <clubar/blocks.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>

static int RUNNING = 1;

#define MUTEX_GUARD(mutex_ptr)                                                 \
    GUARD(pthread_mutex_lock(mutex_ptr), pthread_mutex_unlock(mutex_ptr))

typedef struct ThreadSync {
    bool ready;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ThreadSync;

// clang-format off
#define THREADSYNC()                                                           \
    {false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#define THREADSYNC_WAIT(t)                                                     \
    MUTEX_GUARD(&t.mutex) if (!t.ready) pthread_cond_wait(&t.cond, &t.mutex);
#define THREADSYNC_SIGNAL(t)                                                   \
    MUTEX_GUARD(&t.mutex) { t.ready = true; pthread_cond_signal(&t.cond); }
// clang-format on

static ThreadSync stdin_threadsync    = THREADSYNC();
static pthread_rwlock_t clubar_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t gui_mutex      = PTHREAD_MUTEX_INITIALIZER;
static const struct timespec ts       = {.tv_nsec = 1e6 * (1000 / 120.)};

#define CLUBAR_RDGUARD                                                         \
    GUARD(pthread_rwlock_rdlock(&clubar_rwlock),                               \
          pthread_rwlock_unlock(&clubar_rwlock))
#define CLUBAR_WRGUARD                                                         \
    GUARD(pthread_rwlock_wrlock(&clubar_rwlock),                               \
          pthread_rwlock_unlock(&clubar_rwlock))

#define CLEAR_AND_RENDER_WITH(blktype)                                         \
    for (int _c = (pthread_mutex_lock(&gui_mutex), gui_clear(blktype), 1); _c; \
         _c     = (gui_draw(blktype), pthread_mutex_unlock(&gui_mutex), 0))

typedef struct LineReader {
    char buffer[4096];
    int start, end;
} LineReader;
#define LINE_READER() (LineReader){.start = 0, .end = 0};

static inline char *readline(LineReader *lr)
{
    static const int size = sizeof(lr->buffer) - 1;
    if (lr->start > 0 && lr->buffer[lr->start - 1] == 0) {
        memmove(lr->buffer, lr->buffer + lr->start, lr->end -= lr->start);
        lr->start = 0;
    }
    if (lr->start == lr->end) {
        if (lr->end == size)
            lr->start = lr->end = 0;
        ssize_t n;
        if ((n = read(STDIN_FILENO, lr->buffer + lr->end, size - lr->end)) > 0)
            lr->end += n;
    }
    for (; lr->start < lr->end; ++lr->start)
        if (lr->buffer[lr->start] == '\n' && !(lr->buffer[lr->start++] = 0))
            return lr->buffer;
    return NULL;
}

static void *stdin_thread_handler(__attribute__((unused)) void *_)
{
    THREADSYNC_WAIT(stdin_threadsync);
    struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
    LineReader reader = LINE_READER();
    char *line;

    for (bool running = RUNNING; running;) {
        if (poll(&pfd, 1, 420) > 0) {
            if (IS_SET(pfd.revents, POLLERR | POLLHUP)) {
                CLUBAR_WRGUARD { RUNNING = false; }
            }
            if (IS_SET(pfd.revents, POLLIN)) {
                if ((line = readline(&reader)) == NULL) {
                    nanosleep(&ts, NULL); // clubar </dev/zero
                } else if (*line) {
                    CLEAR_AND_RENDER_WITH(Stdin)
                    {
                        CLUBAR_WRGUARD
                        {
                            clubar_update_blks(clubar, Stdin, line);
                        }
                    }
                }
            }
        }
        CLUBAR_RDGUARD { running = RUNNING; }
    }
    pthread_exit(0);
}

static void *sig_thread_handler(void *arg)
{
    sigset_t *sig_set = (sigset_t *)arg;
    for (int sig, running = RUNNING; running;) {
        switch (sigwait(sig_set, &sig), sig) {
        case SIGCHLD: {
            while (wait(NULL) > 0)
                (void)0;
        } break;
        case SIGQUIT: // fallthrough.
        case SIGINT:  // fallthrough.
        case SIGHUP:  // fallthrough.
        case SIGTERM: {
            CLUBAR_WRGUARD { RUNNING = false; }
        } break;
        case SIGUSR1: {
            MUTEX_GUARD(&gui_mutex) { gui_toggle(); }
        } break;
        case SIGUSR2: {
            CLUBAR_WRGUARD { clubar_load_external_configs(clubar); }
            MUTEX_GUARD(&gui_mutex) { gui_load(); }
        } break;
        default: break;
        }
        CLUBAR_RDGUARD { running = RUNNING; }
    }
    pthread_exit(0);
}

int main(int argc, char const **argv)
{
    pthread_t stdin_thread, sig_thread;
    char buffer[BLK_BUFFER_SIZE];
    XEvent e;
    sigset_t sig_set;

    cli_args->argc = argc;
    cli_args->argv = argv;

    clubar_init(clubar);
    gui_init();

    {
        sigemptyset(&sig_set);
        sigaddset(&sig_set, SIGCHLD);
        sigaddset(&sig_set, SIGQUIT);
        sigaddset(&sig_set, SIGINT);
        sigaddset(&sig_set, SIGHUP);
        sigaddset(&sig_set, SIGTERM);
        sigaddset(&sig_set, SIGUSR1);
        sigaddset(&sig_set, SIGUSR2);
        pthread_sigmask(SIG_BLOCK, &sig_set, NULL);
    }

    // spawn thread and wait (for the window to load).
    pthread_create(&stdin_thread, NULL, stdin_thread_handler, NULL);
    pthread_create(&sig_thread, NULL, sig_thread_handler, &sig_set);

    clubar_load_external_configs(clubar);
    gui_load();

    for (bool running = RUNNING; running; (void)nanosleep(&ts, NULL)) {
        if (XPending(dpy())) {
            switch (XNextEvent(dpy(), &e), e.type) {
            case Expose: {
                onExpose();
                CLEAR_AND_RENDER_WITH(Custom) { (void)0; }
                CLEAR_AND_RENDER_WITH(Stdin) { (void)0; }
            } break;
            case MapNotify: {
                onMapNotify(&e, buffer);
                THREADSYNC_SIGNAL(stdin_threadsync);
            } break;
            // root window events.
            case PropertyNotify: {
                if (onPropertyNotify(&e, buffer)) {
                    CLEAR_AND_RENDER_WITH(Custom)
                    {
                        CLUBAR_WRGUARD
                        {
                            clubar_update_blks(clubar, Custom, buffer);
                        }
                    }
                }
            } break;
            case ButtonPress: {
                onButtonPress(&e);
            } break;
            }
        }
        CLUBAR_RDGUARD { running = RUNNING; }
    }

    pthread_join(stdin_thread, NULL);
    pthread_join(sig_thread, NULL);
    gui_destroy();

    return 0;
}
