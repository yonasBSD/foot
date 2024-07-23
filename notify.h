#pragma once
#include <stdbool.h>
#include <unistd.h>

struct terminal;

enum notify_when {
    /* First, so that it can be left out of initializer and still be
       the default */
    NOTIFY_ALWAYS,

    NOTIFY_UNFOCUSED,
    NOTIFY_INVISIBLE
};

enum notify_urgency {
    /* First, so that it can be left out of initializer and still be
       the default */
    NOTIFY_URGENCY_NORMAL,

    NOTIFY_URGENCY_LOW,
    NOTIFY_URGENCY_CRITICAL,
};

struct notification {
    /*
     * Set by caller of notify_notify()
     */
    char *id;
    char *title;
    char *body;
    char *icon;

    enum notify_when when;
    enum notify_urgency urgency;
    bool focus;
    bool report;

    /*
     * Used internally by notify
     */

    char *xdg_token;       /* XDG activation token, from daemon */

    pid_t pid;             /* Notifier command PID */
    int stdout_fd;         /* Notifier command's stdout */

    char *stdout;          /* Data we've reado from command's stdout */
    size_t stdout_sz;
};

bool notify_notify(const struct terminal *term, struct notification *notif);
void notify_free(struct terminal *term, struct notification *notif);
