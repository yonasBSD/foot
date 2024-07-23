#include "notify.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_MODULE "notify"
#define LOG_ENABLE_DBG 1
#include "log.h"
#include "config.h"
#include "spawn.h"
#include "terminal.h"
#include "util.h"
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
    free(notif->icon_id);
    free(notif->icon_symbolic_name);
    free(notif->icon_data);
    free(notif->xdg_token);
    free(notif->stdout_data);
}

static void
consume_stdout(struct notification *notif, bool eof)
{
    char *data = notif->stdout_data;
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

    memmove(notif->stdout_data, data, left);
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
            if (notif->stdout_data == NULL) {
                xassert(notif->stdout_sz == 0);
                notif->stdout_data = xmemdup(buf, count);
            } else {
                notif->stdout_data = xrealloc(notif->stdout_data, notif->stdout_sz + count);
                memcpy(&notif->stdout_data[notif->stdout_sz], buf, count);
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
    xassert(notif->stdout_data == NULL);

    notif->pid = -1;
    notif->stdout_fd = -1;

    /* Use body as title, if title is unset */
    const char *title = notif->title != NULL ? notif->title : notif->body;
    const char *body = notif->title != NULL && notif->body != NULL ? notif->body : "";

    /* Icon: use symbolic name from notification, if present,
       otherwise fallback to the application ID */
    const char *icon_name_or_path = term->app_id != NULL
        ? term->app_id
        : term->conf->app_id;

    if (notif->icon_id != NULL) {
        for (size_t i = 0; i < ALEN(term->notification_icons); i++) {
            const struct notification_icon *icon = &term->notification_icons[i];

            if (icon->id != NULL && strcmp(icon->id, notif->icon_id) == 0) {
                icon_name_or_path = icon->symbolic_name != NULL
                    ? icon->symbolic_name
                    : icon->tmp_file_on_disk;
                break;
            }
        }
    }

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
            (const char *[]){
                "app-id", "window-title", "icon", "title", "body", "urgency"},
            (const char *[]){
                term->app_id ? term->app_id : term->conf->app_id,
                term->window_title, icon_name_or_path, title, body, urgency_str},
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

static void
add_icon(struct notification_icon *icon, const char *id, const char *symbolic_name,
         const uint8_t *data, size_t data_sz)
{
    icon->id = xstrdup(id);
    icon->symbolic_name = symbolic_name != NULL ? xstrdup(symbolic_name) : NULL;
    icon->tmp_file_on_disk = NULL;

    if (data_sz > 0) {
        char name[64] = "/tmp/foot-notification-icon-cache-XXXXXX";
        int fd = mkstemp(name);

        if (fd < 0) {
            LOG_ERRNO("failed to create temporary file for icon cache");
            return;
        }

        write(fd, data, data_sz);
        close(fd);

        LOG_DBG("wrote icon data to %s", name);
        icon->tmp_file_on_disk = xstrdup(name);
    }

    LOG_DBG("added icon to cache: %s: sym=%s, file=%s",
            icon->id, icon->symbolic_name, icon->tmp_file_on_disk);
}

void
notify_icon_add(struct terminal *term, const char *id,
                const char *symbolic_name, const uint8_t *data, size_t data_sz)
{
#if defined(_DEBUG)
    for (size_t i = 0; i < ALEN(term->notification_icons); i++) {
        struct notification_icon *icon = &term->notification_icons[i];
        if (icon->id != NULL && strcmp(icon->id, id) == 0) {
            BUG("notification icon cache already contains \"%s\"", id);
        }
    }
#endif

    for (size_t i = 0; i < ALEN(term->notification_icons); i++) {
        struct notification_icon *icon = &term->notification_icons[i];
        if (icon->id == NULL) {
            add_icon(icon, id, symbolic_name, data, data_sz);
            return;
        }
    }

    /* Cache full - throw out first entry, add new entry last */
    notify_icon_free(&term->notification_icons[0]);
    memmove(&term->notification_icons[0],
            &term->notification_icons[1],
            ((ALEN(term->notification_icons) - 1) *
             sizeof(term->notification_icons[0])));

    add_icon(
        &term->notification_icons[ALEN(term->notification_icons) - 1],
        id, symbolic_name, data, data_sz);
}

void
notify_icon_del(struct terminal *term, const char *id)
{
    for (size_t i = 0; i < ALEN(term->notification_icons); i++) {
        struct notification_icon *icon = &term->notification_icons[i];

        if (icon->id == NULL || strcmp(icon->id, id) != 0)
            continue;

        notify_icon_free(icon);
        return;
    }
}

void
notify_icon_free(struct notification_icon *icon)
{
    if (icon->tmp_file_on_disk != NULL)
        unlink(icon->tmp_file_on_disk);

    free(icon->id);
    free(icon->symbolic_name);
    free(icon->tmp_file_on_disk);

    icon->id = NULL;
    icon->symbolic_name = NULL;
    icon->tmp_file_on_disk = NULL;
}
