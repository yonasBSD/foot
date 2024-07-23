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
    char *id;
    char *title;
    char *body;
    char *icon;
    char *xdg_token;
    enum notify_when when;
    enum notify_urgency urgency;
    bool focus;
    bool report;

    pid_t pid;
    int stdout_fd;
};

bool notify_notify(const struct terminal *term, struct notification *notif);
void notify_free(struct terminal *term, struct notification *notif);
