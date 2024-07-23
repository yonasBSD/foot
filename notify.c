#include "notify.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>

#define LOG_MODULE "notify"
#define LOG_ENABLE_DBG 0
#include "log.h"
#include "config.h"
#include "spawn.h"
#include "terminal.h"
#include "xmalloc.h"

void
notify_notify(const struct terminal *term, const char *title, const char *body,
              enum notify_when when, enum notify_urgency urgency)
{
    LOG_DBG("notify: title=\"%s\", msg=\"%s\"", title, body);

    if ((term->conf->notify_focus_inhibit || when != NOTIFY_ALWAYS)
        && term->kbd_focus)
    {
        /* No notifications while we're focused */
        return;
    }

    if (title == NULL || body == NULL)
        return;

    if (term->conf->notify.argv.args == NULL)
        return;

    char **argv = NULL;
    size_t argc = 0;

    const char *urgency_str =
        urgency == NOTIFY_URGENCY_LOW
            ? "low"
            : urgency == NOTIFY_URGENCY_NORMAL
                ? "normal" : "critical";

    if (!spawn_expand_template(
            &term->conf->notify, 5,
            (const char *[]){"app-id", "window-title", "title", "body", "urgency"},
            (const char *[]){term->app_id ? term->app_id : term->conf->app_id,
                             term->window_title, title, body, urgency_str},
            &argc, &argv))
    {
        return;
    }

    LOG_DBG("notify command:");
    for (size_t i = 0; i < argc; i++)
        LOG_DBG("  argv[%zu] = \"%s\"", i, argv[i]);

    /* Redirect stdin to /dev/null, but ignore failure to open */
    int devnull = open("/dev/null", O_RDONLY);
    pid_t pid = spawn(
        term->reaper, NULL, argv, devnull, stdout_fds[1], -1,
        &notif_done, (void *)term, NULL);

    if (stdout_fds[1] >= 0) {

    if (devnull >= 0)
        close(devnull);

    for (size_t i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}
