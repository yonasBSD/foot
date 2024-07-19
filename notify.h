#pragma once
#include <stdbool.h>

struct terminal;

enum notify_when {
    NOTIFY_ALWAYS,
    NOTIFY_UNFOCUSED,
    NOTIFY_INVISIBLE
};

enum notify_urgency {
    NOTIFY_URGENCY_LOW,
    NOTIFY_URGENCY_NORMAL,
    NOTIFY_URGENCY_CRITICAL,
};

struct kitty_notification {
    char *id;
    char *title;
    char *body;
    enum notify_when when;
    enum notify_urgency urgency;
    bool focus;
    bool report;
};

void notify_notify(
    const struct terminal *term, const char *title, const char *body,
    enum notify_when when, enum notify_urgency urgency);
