#pragma once
#include <stdbool.h>
#include <stdint.h>
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

    char *icon_id;
    char *icon_symbolic_name;
    uint8_t *icon_data;
    size_t icon_data_sz;

    enum notify_when when;
    enum notify_urgency urgency;
    bool focus;
    bool report_activated;
    bool report_closed;

    /*
     * Used internally by notify
     */

    uint32_t external_id;  /* Daemon assigned notification ID */
    bool activated;        /* User 'activated' the notification */
    char *xdg_token;       /* XDG activation token, from daemon */

    pid_t pid;             /* Notifier command PID */
    int stdout_fd;         /* Notifier command's stdout */

    char *stdout_data;     /* Data we've reado from command's stdout */
    size_t stdout_sz;
};

struct notification_icon {
    char *id;
    char *symbolic_name;
    char *tmp_file_name;
    int tmp_file_fd;
};

bool notify_notify(struct terminal *term, struct notification *notif);
void notify_close(struct terminal *term, const char *id);
void notify_free(struct terminal *term, struct notification *notif);

void notify_icon_add(struct terminal *term, const char *id,
                     const char *symbolic_name, const uint8_t *data,
                     size_t data_sz);
void notify_icon_del(struct terminal *term, const char *id);
void notify_icon_free(struct notification_icon *icon);
