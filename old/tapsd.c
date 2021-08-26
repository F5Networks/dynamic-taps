/*
 * This is an experiment to see how a tapsd daemon would work, which would
 * monitor the /etc/tapsd/ directory, maintain the inventory in RAM, and answer
 * queries asking for a library.
 *
 * Given that getting a preconnection is asynchronous, simply reading it off
 * disk is probably good enough.
 */

#include <errno.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include "taps_internals.h"

#define TAPS_CONF_PATH "/etc/taps"

tapsProtocol protocols[255];

#if 0
static void handler(int signum)
#endif

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * ( EVENT_SIZE + 16))

int main(int argc, char *argv[])
{
    int fd, wd; /* file descriptor, watch descriptor */
    int length, i;
    char buffer[EVENT_BUF_LEN];
    pid_t pid;

    /* Populate the protocol table */
    tapsReadProtocols(protocols);
    fd = inotify_init();
    if (fd < 0) {
        syslog(LOG_USER | LOG_CRIT, "tapsd: inotify_init failed: %s",
                strerror(errno));
        closelog();
        return 0;
    }
    wd = inotify_add_watch(fd, TAPS_CONF_PATH, IN_CREATE | IN_DELETE |
            IN_CLOSE_WRITE); // not sure about the last one: file only?
    if (wd < 0) {
        syslog(LOG_USER | LOG_CRIT, "tapsd: could not watch /etc/taps: %s",
                strerror(errno));
        closelog();
        return 0;
    }
/* handlers for the library
    signal();
    signal();
*/
    /* Is this the old style? */
    pid = fork();
    if (pid == -1) {
        return 0;
    }
    if (pid > 0) {
        return 1; /* parent process */
    }
    /* child process */
    if (setsid() == -1) {
        return 0;
    }
    while(1) {
        length = read(fd, buffer, EVENT_BUF_LEN); // THIS IS BLOCKING!
        if (length < 0) {
            syslog(LOG_USER | LOG_CRIT, "tapsd: could not read directory "
                    "events: %s", strerror(errno));
            return 0;
        }
        while (i < length ) {
            struct inotify_event *event = ( struct inotify_event * ) &buffer[i];
            if ( event->len ) {
                if (event->mask & IN_CREATE) {
                }
                i += EVENT_SIZE + event->len;
            }
        }
    }
    return 1;
}
