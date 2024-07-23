#include "notify.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_MODULE "notify"
#define LOG_ENABLE_DBG 0
#include "log.h"
#include "config.h"
#include "spawn.h"
#include "terminal.h"
#include "wayland.h"
#include "xmalloc.h"
#include "xsnprintf.h"

void
notify_free(struct terminal *term, struct notification *notif)
{
    fdm_del(term->fdm, notif->stdout_fd);
    free(notif->id);
    free(notif->title);
    free(notif->body);
    free(notif->icon);
    free(notif->xdg_token);
    free(notif->stdout);
}

static void
consume_stdout(struct notification *notif, bool eof)
{
    char *data = notif->stdout;
    const char *line = data;
    size_t left = notif->stdout_sz;

    /* Process stdout, line-by-line */
    while (left > 0) {
        line = data;
        size_t len = left;
        char *eol = memchr(line, '\n', left);

        if (eol != NULL) {
            *eol = '\0';
            len = strlen(line);
            data = eol + 1;
        } else if (!eof)
            break;

        /* Check for 'xdgtoken=xyz' */
        if (len > 9 && memcmp(line, "xdgtoken=", 9) == 0) {
            notif->xdg_token = xstrndup(&line[9], len - 9);
            LOG_DBG("XDG token: \"%s\"", notif->xdg_token);
        }

        left -= len + (eol != NULL ? 1 : 0);
    }

    memmove(notif->stdout, data, left);
    notif->stdout_sz = left;
}

static bool
fdm_notify_stdout(struct fdm *fdm, int fd, int events, void *data)
{
    const struct terminal *term = data;
    struct notification *notif = NULL;


    /* Find notification */
    tll_foreach(term->notifications, it) {
        if (it->item.stdout_fd == fd) {
            notif = &it->item;
            break;
        }
    }

    if (events & EPOLLIN) {
        char buf[512];
        ssize_t count = read(fd, buf, sizeof(buf) - 1);

        if (count < 0) {
            if (errno == EINTR)
                return true;

            LOG_ERRNO("failed to read notification activation token");
            return false;
        }

        if (count > 0 && notif != NULL) {
            if (notif->stdout == NULL) {
                xassert(notif->stdout_sz == 0);
                notif->stdout = xmemdup(buf, count);
            } else {
                notif->stdout = xrealloc(notif->stdout, notif->stdout_sz + count);
                memcpy(&notif->stdout[notif->stdout_sz], buf, count);
            }

            notif->stdout_sz += count;
            consume_stdout(notif, false);
        }
    }

    if (events & EPOLLHUP) {
        fdm_del(fdm, fd);
        if (notif != NULL) {
            notif->stdout_fd = -1;
            consume_stdout(notif, true);
        }
    }

    return true;
}

static void
notif_done(struct reaper *reaper, pid_t pid, int status, void *data)
{
    struct terminal *term = data;

    tll_foreach(term->notifications, it) {
        struct notification *notif = &it->item;
        if (notif->pid != pid)
            continue;

        LOG_DBG("notification %s dismissed", notif->id);

        if (notif->focus) {
            LOG_DBG("focus window on notification activation: \"%s\"", notif->xdg_token);
            wayl_activate(term->wl, term->window, notif->xdg_token);
        }

        if (notif->report) {
            xassert(notif->id != NULL);

            LOG_DBG("sending notification report to client");

            char reply[5 + strlen(notif->id) + 1 + 2 + 1];
            int n = xsnprintf(
                reply, sizeof(reply), "\033]99;%s;\033\\", notif->id);
            term_to_slave(term, reply, n);
        }

        notify_free(term, notif);
        tll_remove(term->notifications, it);
        return;
    }
}

bool
notify_notify(const struct terminal *term, struct notification *notif)
{
    xassert(notif->xdg_token == NULL);
    xassert(notif->pid == 0);
    xassert(notif->stdout_fd == 0);
    xassert(notif->stdout == NULL);

    notif->pid = -1;
    notif->stdout_fd = -1;

    /* Use body as title, if title is unset */
    const char *title = notif->title != NULL ? notif->title : notif->body;
    const char *body = notif->title != NULL && notif->body != NULL ? notif->body : "";

    /* Icon: use symbolic name from notification, if present,
       otherwise fallback to the application ID */
    const char *icon = notif->icon != NULL
        ? notif->icon
        : term->app_id != NULL
            ? term->app_id
            : term->conf->app_id;

    LOG_DBG("notify: title=\"%s\", body=\"%s\"", title, body);

    xassert(title != NULL);
    if (title == NULL)
        return false;

    if ((term->conf->desktop_notifications.inhibit_when_focused ||
         notif->when != NOTIFY_ALWAYS)
        && term->kbd_focus)
    {
        /* No notifications while we're focused */
        return false;
    }

    if (term->conf->desktop_notifications.command.argv.args == NULL)
        return false;

    char **argv = NULL;
    size_t argc = 0;

    const char *urgency_str =
        notif->urgency == NOTIFY_URGENCY_LOW
            ? "low"
            : notif->urgency == NOTIFY_URGENCY_NORMAL
                ? "normal" : "critical";

    if (!spawn_expand_template(
            &term->conf->desktop_notifications.command, 6,
            (const char *[]){"app-id", "window-title", "icon", "title", "body", "urgency"},
            (const char *[]){term->app_id ? term->app_id : term->conf->app_id,
                             term->window_title, icon, title, body, urgency_str},
            &argc, &argv))
    {
        return false;
    }

    LOG_DBG("notify command:");
    for (size_t i = 0; i < argc; i++)
        LOG_DBG("  argv[%zu] = \"%s\"", i, argv[i]);

    int stdout_fds[2] = {-1, -1};
    if ((notif->focus || notif->report) &&
        pipe2(stdout_fds, O_CLOEXEC | O_NONBLOCK) < 0)
    {
        LOG_WARN("failed to create stdout pipe");
        /* Non-fatal */
    }

    if (stdout_fds[0] >= 0) {
        xassert(notif->xdg_token == NULL);
        fdm_add(term->fdm, stdout_fds[0], EPOLLIN,
                &fdm_notify_stdout, (void *)term);
    }

    /* Redirect stdin to /dev/null, but ignore failure to open */
    int devnull = open("/dev/null", O_RDONLY);
    pid_t pid = spawn(
        term->reaper, NULL, argv, devnull, stdout_fds[1], -1,
        &notif_done, (void *)term, NULL);

    if (stdout_fds[1] >= 0) {
        /* Close write-end of stdout pipe */
        close(stdout_fds[1]);
    }

    if (pid < 0 && stdout_fds[0] >= 0) {
        /* Remove FDM callback if we failed to spawn */
        fdm_del(term->fdm, stdout_fds[0]);
    }

    if (devnull >= 0)
        close(devnull);

    for (size_t i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);

    notif->pid = pid;
    notif->stdout_fd = stdout_fds[0];
    return true;
}
