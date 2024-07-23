#include "osc.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/epoll.h>

#define LOG_MODULE "osc"
#define LOG_ENABLE_DBG 0
#include "log.h"
#include "base64.h"
#include "config.h"
#include "grid.h"
#include "macros.h"
#include "notify.h"
#include "render.h"
#include "selection.h"
#include "terminal.h"
#include "uri.h"
#include "util.h"
#include "vt.h"
#include "xmalloc.h"
#include "xsnprintf.h"

#define UNHANDLED() LOG_DBG("unhandled: OSC: %.*s", (int)term->vt.osc.idx, term->vt.osc.data)

static void
osc_to_clipboard(struct terminal *term, const char *target,
                 const char *base64_data)
{
    bool to_clipboard = false;
    bool to_primary = false;

    if (target[0] == '\0')
        to_clipboard = true;

    for (const char *t = target; *t != '\0'; t++) {
        switch (*t) {
        case 'c':
            to_clipboard = true;
            break;

        case 's':
        case 'p':
            to_primary = true;
            break;

        default:
            LOG_WARN("unimplemented: clipboard target '%c'", *t);
            break;
        }
    }

    /* Find a seat in which the terminal has focus */
    struct seat *seat = NULL;
    tll_foreach(term->wl->seats, it) {
        if (it->item.kbd_focus == term) {
            seat = &it->item;
            break;
        }
    }

    if (seat == NULL) {
        LOG_WARN("OSC52: client tried to write to clipboard data while window was unfocused");
        return;
    }

    char *decoded = base64_decode(base64_data, NULL);
    if (decoded == NULL) {
        if (errno == EINVAL)
            LOG_WARN("OSC: invalid clipboard data: %s", base64_data);
        else
            LOG_ERRNO("base64_decode() failed");

        if (to_clipboard)
            selection_clipboard_unset(seat);
        if (to_primary)
            selection_primary_unset(seat);
        return;
    }

    LOG_DBG("decoded: %s", decoded);

    if (to_clipboard) {
        char *copy = xstrdup(decoded);
        if (!text_to_clipboard(seat, term, copy, seat->kbd.serial))
            free(copy);
    }

    if (to_primary) {
        char *copy = xstrdup(decoded);
        if (!text_to_primary(seat, term, copy, seat->kbd.serial))
            free(copy);
    }

    free(decoded);
}

struct clip_context {
    struct seat *seat;
    struct terminal *term;
    uint8_t buf[3];
    int idx;
};

static void
from_clipboard_cb(char *text, size_t size, void *user)
{
    struct clip_context *ctx = user;
    struct terminal *term = ctx->term;

    xassert(ctx->idx >= 0 && ctx->idx <= 2);

    const char *t = text;
    size_t left = size;

    if (ctx->idx > 0) {
        for (size_t i = ctx->idx; i < 3 && left > 0; i++, t++, left--)
            ctx->buf[ctx->idx++] = *t;

        xassert(ctx->idx <= 3);
        if (ctx->idx == 3) {
            char *chunk = base64_encode(ctx->buf, 3);
            xassert(chunk != NULL);
            xassert(strlen(chunk) == 4);

            term_paste_data_to_slave(term, chunk, 4);
            free(chunk);

            ctx->idx = 0;
        }
    }

    if (left == 0)
        return;

    xassert(ctx->idx == 0);

    int remaining = left % 3;
    for (int i = remaining; i > 0; i--)
        ctx->buf[ctx->idx++] = text[size - i];
    xassert(ctx->idx == remaining);

    char *chunk = base64_encode((const uint8_t *)t, left / 3 * 3);
    xassert(chunk != NULL);
    xassert(strlen(chunk) % 4 == 0);
    term_paste_data_to_slave(term, chunk, strlen(chunk));
    free(chunk);
}

static void
from_clipboard_done(void *user)
{
    struct clip_context *ctx = user;
    struct terminal *term = ctx->term;

    if (ctx->idx > 0) {
        char res[4];
        base64_encode_final(ctx->buf, ctx->idx, res);
        term_paste_data_to_slave(term, res, 4);
    }

    if (term->vt.osc.bel)
        term_paste_data_to_slave(term, "\a", 1);
    else
        term_paste_data_to_slave(term, "\033\\", 2);

    term->is_sending_paste_data = false;

    /* Make sure we send any queued up non-paste data */
    if (tll_length(term->ptmx_buffers) > 0)
        fdm_event_add(term->fdm, term->ptmx, EPOLLOUT);

    free(ctx);
}

static void
osc_from_clipboard(struct terminal *term, const char *source)
{
    /* Find a seat in which the terminal has focus */
    struct seat *seat = NULL;
    tll_foreach(term->wl->seats, it) {
        if (it->item.kbd_focus == term) {
            seat = &it->item;
            break;
        }
    }

    if (seat == NULL) {
        LOG_WARN("OSC52: client tried to read clipboard data while window was unfocused");
        return;
    }

    /* Use clipboard if no source has been specified */
    char src = source[0] == '\0' ? 'c' : 0;
    bool from_clipboard = src == 'c';
    bool from_primary = false;

    for (const char *s = source;
         *s != '\0' && !from_clipboard && !from_primary;
         s++)
    {
        if (*s == 'c' || *s == 'p' || *s == 's') {
            src = *s;

            switch (src) {
            case 'c':
                from_clipboard = selection_clipboard_has_data(seat);
                break;

            case 's':
            case 'p':
                from_primary = selection_primary_has_data(seat);
                break;
            }
        } else
            LOG_WARN("unimplemented: clipboard source '%c'", *s);
    }

    if (!from_clipboard && !from_primary)
        return;

    if (term->is_sending_paste_data) {
        /* FIXME: we should wait for the paste to end, then continue
           with the OSC-52 reply */
        term_to_slave(term, "\033]52;", 5);
        term_to_slave(term, &src, 1);
        term_to_slave(term, ";", 1);
        if (term->vt.osc.bel)
            term_to_slave(term, "\a", 1);
        else
            term_to_slave(term, "\033\\", 2);
        return;
    }

    term->is_sending_paste_data = true;

    term_paste_data_to_slave(term, "\033]52;", 5);
    term_paste_data_to_slave(term, &src, 1);
    term_paste_data_to_slave(term, ";", 1);

    struct clip_context *ctx = xmalloc(sizeof(*ctx));
    *ctx = (struct clip_context) {.seat = seat, .term = term};

    if (from_clipboard) {
        text_from_clipboard(
            seat, term, &from_clipboard_cb, &from_clipboard_done, ctx);
    }

    if (from_primary) {
        text_from_primary(
            seat, term, &from_clipboard_cb, &from_clipboard_done, ctx);
    }
}

static void
osc_selection(struct terminal *term, char *string)
{
    char *p = string;
    bool clipboard_done = false;

    /* The first parameter is a string of clipbard sources/targets */
    while (*p != '\0' && !clipboard_done) {
        switch (*p) {
        case ';':
            clipboard_done = true;
            *p = '\0';
            break;
        }

        p++;
    }

    LOG_DBG("clipboard: target = %s data = %s", string, p);

    if (p[0] == '?' && p[1] == '\0')
        osc_from_clipboard(term, string);
    else
        osc_to_clipboard(term, string, p);
}

static void
osc_flash(struct terminal *term)
{
    /* Our own private - flash */
    term_flash(term, 50);
}

static bool
parse_legacy_color(const char *string, uint32_t *color, bool *_have_alpha,
                   uint16_t *_alpha)
{
    bool have_alpha = false;
    uint16_t alpha = 0xffff;

    if (string[0] == '[') {
        /* e.g. \E]11;[50]#00ff00 */
        const char *start = &string[1];

        errno = 0;
        char *end;
        unsigned long percent = strtoul(start, &end, 10);

        if (errno != 0 || *end != ']')
            return false;

        have_alpha = true;
        alpha = (0xffff * min(percent, 100) + 50) / 100;

        string = end + 1;
    }

    if (string[0] != '#')
        return false;

    string++;
    const size_t len = strlen(string);

    if (len % 3 != 0)
        return false;

    const int digits = len / 3;

    int rgb[3];
    for (size_t i = 0; i < 3; i++) {
        rgb[i] = 0;
        for (size_t j = 0; j < digits; j++) {
            size_t idx = i * digits + j;
            char c = string[idx];
            rgb[i] <<= 4;

            if (!isxdigit(c))
                rgb[i] |= 0;
            else
                rgb[i] |= c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10;
        }

        /* Values with less than 16 bits represent the *most
         * significant bits*. I.e. the values are *not* scaled */
        rgb[i] <<= 16 - (4 * digits);
    }

    /* Re-scale to 8-bit */
    uint8_t r = 256 * (rgb[0] / 65536.);
    uint8_t g = 256 * (rgb[1] / 65536.);
    uint8_t b = 256 * (rgb[2] / 65536.);

    LOG_DBG("legacy: %02x%02x%02x (alpha=%04x)", r, g, b,
            have_alpha ? alpha : 0xffff);

    *color = r << 16 | g << 8 | b;

    if (_have_alpha != NULL)
        *_have_alpha = have_alpha;
    if (_alpha != NULL)
        *_alpha = alpha;
    return true;
}

static bool
parse_rgb(const char *string, uint32_t *color, bool *_have_alpha,
          uint16_t *_alpha)
{
    size_t len = strlen(string);
    bool have_alpha = len >= 4 && strncmp(string, "rgba", 4) == 0;

    /* Verify we have the minimum required length (for "") */
    if (have_alpha) {
        if (len < STRLEN("rgba:x/x/x/x"))
            return false;
    } else {
        if (len < STRLEN("rgb:x/x/x"))
            return false;
    }

    /* Verify prefix is "rgb:" or "rgba:" */
    if (have_alpha) {
        if (strncmp(string, "rgba:", 5) != 0)
            return false;
        string += 5;
        len -= 5;
    } else {
        if (strncmp(string, "rgb:", 4) != 0)
            return false;
        string += 4;
        len -= 4;
    }

    int rgb[4];
    int digits[4];

    for (size_t i = 0; i < (have_alpha ? 4 : 3); i++) {
        for (rgb[i] = 0, digits[i] = 0;
             len > 0 && *string != '/';
             len--, string++, digits[i]++)
        {
            char c = *string;
            rgb[i] <<= 4;

            if (!isxdigit(c))
                rgb[i] |= 0;
            else
                rgb[i] |= c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10;
        }

        if (i >= (have_alpha ? 3 : 2))
            break;

        if (len == 0 || *string != '/')
            return false;
        string++; len--;
    }

    /* Re-scale to 8-bit */
    uint8_t r = 256 * (rgb[0] / (double)(1 << (4 * digits[0])));
    uint8_t g = 256 * (rgb[1] / (double)(1 << (4 * digits[1])));
    uint8_t b = 256 * (rgb[2] / (double)(1 << (4 * digits[2])));

    uint16_t alpha = 0xffff;
    if (have_alpha)
        alpha = 65536 * (rgb[3] / (double)(1 << (4 * digits[3])));

    if (have_alpha)
        LOG_DBG("rgba: %02x%02x%02x (alpha=%04x)", r, g, b, alpha);
    else
        LOG_DBG("rgb: %02x%02x%02x", r, g, b);

    if (_have_alpha != NULL)
        *_have_alpha = have_alpha;
    if (_alpha != NULL)
        *_alpha = alpha;

    *color = r << 16 | g << 8 | b;
    return true;
}

static void
osc_set_pwd(struct terminal *term, char *string)
{
    LOG_DBG("PWD: URI: %s", string);

    char *scheme, *host, *path;
    if (!uri_parse(string, strlen(string), &scheme, NULL, NULL, &host, NULL, &path, NULL, NULL)) {
        LOG_ERR("OSC7: invalid URI: %s", string);
        return;
    }

    if (streq(scheme, "file") && hostname_is_localhost(host)) {
        LOG_DBG("OSC7: pwd: %s", path);
        free(term->cwd);
        term->cwd = path;
    } else
        free(path);

    free(scheme);
    free(host);
}

static void
osc_uri(struct terminal *term, char *string)
{
    /*
     * \E]8;<params>;URI\e\\
     *
     * Params are key=value pairs, separated by ':'.
     *
     * The only defined key (as of 2020-05-31) is 'id', which is used
     * to group split-up URIs:
     *
     * ╔═ file1 ════╗
     * ║          ╔═ file2 ═══╗
     * ║http://exa║Lorem ipsum║
     * ║le.com    ║ dolor sit ║
     * ║          ║amet, conse║
     * ╚══════════║ctetur adip║
     *            ╚═══════════╝
     *
     * This lets a terminal emulator highlight both parts at the same
     * time (e.g. when hovering over one of the parts with the mouse).
     */

    char *params = string;
    char *params_end = strchr(params, ';');
    if (params_end == NULL)
        return;

    *params_end = '\0';
    const char *uri = params_end + 1;
    uint64_t id = (uint64_t)rand() << 32 | rand();

    char *ctx = NULL;
    for (const char *key_value = strtok_r(params, ":", &ctx);
         key_value != NULL;
         key_value = strtok_r(NULL, ":", &ctx))
    {
        const char *key = key_value;
        char *operator = strchr(key_value, '=');

        if (operator == NULL)
            continue;
        *operator = '\0';

        const char *value = operator + 1;

        if (streq(key, "id"))
            id = sdbm_hash(value);
    }

    LOG_DBG("OSC-8: URL=%s, id=%" PRIu64, uri, id);

    if (uri[0] == '\0')
        term_osc8_close(term);
    else
        term_osc8_open(term, id, uri);
}

static void
osc_notify(struct terminal *term, char *string)
{
    /*
     * The 'notify' perl extension
     * (https://pub.phyks.me/scripts/urxvt/notify) is very simple:
     *
     * #!/usr/bin/perl
     *
     * sub on_osc_seq_perl {
     *   my ($term, $osc, $resp) = @_;
     *   if ($osc =~ /^notify;(\S+);(.*)$/) {
     *     system("notify-send '$1' '$2'");
     *   }
     * }
     *
     * As can be seen, the notification text is not encoded in any
     * way. The regex does a greedy match of the ';' separator. Thus,
     * any extra ';' will end up being part of the title. There's no
     * way to have a ';' in the message body.
     *
     * I've changed that behavior slightly in; we split the title from
     * body on the *first* ';', allowing us to have semicolons in the
     * message body, but *not* in the title.
     */
    char *ctx = NULL;
    const char *title = strtok_r(string, ";", &ctx);
    const char *msg = strtok_r(NULL, "\x00", &ctx);

    if (title == NULL)
        return;

    if (mbsntoc32(NULL, title, strlen(title), 0) == (char32_t)-1) {
        LOG_WARN("%s: notification title is not valid UTF-8, ignoring", title);
        return;
    }

    if (msg != NULL && mbsntoc32(NULL, msg, strlen(msg), 0) == (char32_t)-1) {
        LOG_WARN("%s: notification message is not valid UTF-8, ignoring", msg);
        return;
    }

    notify_notify(term, &(struct notification){
        .title = (char *)title,
        .body = (char *)msg});
}

static void
kitty_notification(struct terminal *term, char *string)
{
    /* https://sw.kovidgoyal.net/kitty/desktop-notifications */

    char *payload = strchr(string, ';');
    if (payload == NULL)
        return;

    char *parameters = string;
    *payload = '\0';
    payload++;

    char *id = xstrdup("0");       /* The 'i' parameter */
    char *icon_id = NULL;          /* The 'g' parameter */
    char *symbolic_icon = NULL;    /* The 'n' parameter */
    bool focus = true;             /* The 'a' parameter */
    bool report = false;           /* The 'a' parameter */
    bool done = true;              /* The 'd' parameter */
    bool base64 = false;           /* The 'e' parameter */

    size_t payload_size;
    enum {
        PAYLOAD_TITLE,
        PAYLOAD_BODY,
        PAYLOAD_ICON,
    } payload_type = PAYLOAD_TITLE; /* The 'p' parameter */

    enum notify_when when = NOTIFY_ALWAYS;
    enum notify_urgency urgency = NOTIFY_URGENCY_NORMAL;

    bool have_a = false;
    bool have_o = false;
    bool have_u = false;

    char *ctx = NULL;
    for (char *param = strtok_r(parameters, ":", &ctx);
         param != NULL;
         param = strtok_r(NULL, ":", &ctx))
    {
        /* All parameters are on the form X=value, where X is always
           exactly one character */
        if (param[0] == '\0' || param[1] != '=')
            continue;

        char *value = &param[2];

        switch (param[0]) {
        case 'a': {
            /* notification activation action: focus|report|-focus|-report */
            have_a = true;
            char *a_ctx = NULL;

            for (const char *v = strtok_r(value, ",", &a_ctx);
                 v != NULL;
                 v = strtok_r(NULL, ",", &a_ctx))
            {
                bool reverse = v[0] == '-';
                if (reverse)
                    v++;

                if (strcmp(v, "focus") == 0)
                    focus = !reverse;
                else if (strcmp(v, "report") == 0)
                    report = !reverse;
            }

            break;
        }

        case 'd':
            /* done: 0|1 */
            if (value[0] == '0' && value[1] == '\0')
                done = false;
            else if (value[0] == '1' && value[1] == '\0')
                done = true;
            break;

        case 'e':
            /* base64: 0=utf8, 1=base64(utf8) */
            if (value[0] == '0' && value[1] == '\0')
                base64 = false;
            else if (value[0] == '1' && value[1] == '\0')
                base64 = true;
            break;

        case 'i':
            /* id */
            free(id);
            id = xstrdup(value);
            break;

        case 'p':
            /* payload content: title|body */
            if (strcmp(value, "title") == 0)
                payload_type = PAYLOAD_TITLE;
            else if (strcmp(value, "body") == 0)
                payload_type = PAYLOAD_BODY;
            else if (strcmp(value, "icon") == 0)
                payload_type = PAYLOAD_ICON;
            break;

        case 'o':
            /* honor when: always|unfocused|invisible */
            have_o = true;
            if (strcmp(value, "always") == 0)
                when = NOTIFY_ALWAYS;
            else if (strcmp(value, "unfocused") == 0)
                when = NOTIFY_UNFOCUSED;
            else if (strcmp(value, "invisible") == 0)
                when = NOTIFY_INVISIBLE;
            break;

        case 'u':
            /* urgency: 0=low, 1=normal, 2=critical */
            have_u = true;
            if (value[0] == '0' && value[1] == '\0')
                urgency = NOTIFY_URGENCY_LOW;
            else if (value[0] == '1' && value[1] == '\0')
                urgency = NOTIFY_URGENCY_NORMAL;
            else if (value[0] == '2' && value[1] == '\0')
                urgency = NOTIFY_URGENCY_CRITICAL;
            break;

        case 'g':
            free(icon_id);
            icon_id = xstrdup(value);
            break;

        case 'n':
            free(symbolic_icon);
            symbolic_icon = xstrdup(value);
            break;
        }
    }

    if (base64) {
        payload = base64_decode(payload, &payload_size);
        if (payload == NULL)
            goto out;
    } else {
        payload = xstrdup(payload);
        payload_size = strlen(payload);
    }

    LOG_DBG("id=%s, done=%d, focus=%d, report=%d, base64=%d, icon=%s, payload: %s, "
            "honor: %s, urgency: %s, %s: %s",
            id, done, focus, report, base64, icon != NULL ? icon : "<not set>",
            payload_is_title ? "title" : "body",
            (when == NOTIFY_ALWAYS
                ? "always"
                : when == NOTIFY_UNFOCUSED
                    ? "unfocused"
                    : "invisible"),
            (urgency == NOTIFY_URGENCY_LOW
                ? "low" : urgency == NOTIFY_URGENCY_NORMAL
                    ? "normal"
                    : "critical"),
            payload_is_title ? "title" : "body", payload);

    /* Search for an existing (d=0) notification to update */
    struct notification *notif = NULL;
    tll_foreach(term->kitty_notifications, it) {
        if (strcmp(it->item.id, id) == 0) {
            /* Found existing notification */
            notif = &it->item;
            break;
        }
    }

    if (notif == NULL) {
        tll_push_front(term->kitty_notifications, ((struct notification){
            .id = id,
            .when = when,
            .urgency = urgency,
            .focus = focus,
            .report = report,
            .stdout_fd = -1,
        }));

        id = NULL;   /* Prevent double free */
        notif = &tll_front(term->kitty_notifications);
    }

    if (notif->pid > 0) {
        /* Notification has already been completed, ignore new metadata */
        goto out;
    }

    /* Update notification metadata */
    if (have_a) {
        notif->focus = focus;
        notif->report = report;
    }

    if (have_o)
        notif->when = when;
    if (have_u)
        notif->urgency = urgency;

    if (icon_id != NULL) {
        free(notif->icon_id);
        notif->icon_id = icon_id;
        icon_id = NULL;  /* Prevent double free */
    }

    if (symbolic_icon != NULL) {
        free(notif->icon_symbolic_name);
        notif->icon_symbolic_name = symbolic_icon;
        symbolic_icon = NULL;
    }

    /* Handled chunked payload - append to existing metadata */
    switch (payload_type) {
    case PAYLOAD_TITLE:
    case PAYLOAD_BODY: {
        char **ptr = payload_type == PAYLOAD_TITLE
            ? &notif->title
            : &notif->body;

        if (*ptr == NULL) {
            *ptr = payload;
            payload = NULL;
        } else {
            char *old = *ptr;
            *ptr = xstrjoin(old, payload);
            free(old);
        }
        break;
    }

    case PAYLOAD_ICON:
        if (notif->icon_data == NULL) {
            notif->icon_data = (uint8_t *)payload;
            notif->icon_data_sz = payload_size;
            payload = NULL;
        } else {
            notif->icon_data = xrealloc(
                notif->icon_data, notif->icon_data_sz + payload_size);
            memcpy(&notif->icon_data[notif->icon_data_sz], payload, payload_size);
            notif->icon_data_sz += payload_size;
        }
        break;
    }

    if (done) {
        /* Update icon cache, if necessary */
        if (notif->icon_id != NULL &&
            (notif->icon_symbolic_name != NULL || notif->icon_data != NULL))
        {
            notify_icon_del(term, notif->icon_id);
            notify_icon_add(term, notif->icon_id,
                            notif->icon_symbolic_name,
                            notif->icon_data, notif->icon_data_sz);

            /* Don't need this anymore */
            free(notif->icon_symbolic_name);
            free(notif->icon_data);
            notif->icon_symbolic_name = NULL;
            notif->icon_data = NULL;
            notif->icon_data_sz = 0;
        }

        /*
         * Show notification.
         *
         * The checks for title|body is to handle notifications that
         * only load icon data into the icon cache
         */
        if (notif->title != NULL || notif->body != NULL) {
            notify_notify(term, notif);
        }

        tll_foreach(term->kitty_notifications, it) {
            if (&it->item == notif) {
                notify_free(term, &it->item);
                tll_remove(term->kitty_notifications, it);
                break;
            }
        }
    }

out:
    free(id);
    free(icon_id);
    free(symbolic_icon);
    free(payload);
}

void
osc_dispatch(struct terminal *term)
{
    unsigned param = 0;
    int data_ofs = 0;

    for (size_t i = 0; i < term->vt.osc.idx; i++, data_ofs++) {
        char c = term->vt.osc.data[i];

        if (c == ';') {
            data_ofs++;
            break;
        }

        if (!isdigit(c)) {
            UNHANDLED();
            return;
        }

        param *= 10;
        param += c - '0';
    }

    LOG_DBG("OSC: %.*s (param = %d)",
            (int)term->vt.osc.idx, term->vt.osc.data, param);

    char *string = (char *)&term->vt.osc.data[data_ofs];

    switch (param) {
    case 0: term_set_window_title(term, string); break;  /* icon + title */
    case 1: break;                                       /* icon */
    case 2: term_set_window_title(term, string); break;  /* title */

    case 4: {
        /* Set color<idx> */

        string--;
        if (*string != ';')
            break;

        xassert(*string == ';');

        for (const char *s_idx = strtok(string, ";"), *s_color = strtok(NULL, ";");
             s_idx != NULL && s_color != NULL;
             s_idx = strtok(NULL, ";"), s_color = strtok(NULL, ";"))
        {
            /* Parse <idx> parameter */
            unsigned idx = 0;
            for (; *s_idx != '\0'; s_idx++) {
                char c = *s_idx;
                idx *= 10;
                idx += c - '0';
            }

            if (idx >= ALEN(term->colors.table)) {
                LOG_WARN("invalid OSC 4 color index: %u", idx);
                break;
            }

            /* Client queried for current value */
            if (s_color[0] == '?' && s_color[1] == '\0') {
                uint32_t color = term->colors.table[idx];
                uint8_t r = (color >> 16) & 0xff;
                uint8_t g = (color >>  8) & 0xff;
                uint8_t b = (color >>  0) & 0xff;
                const char *terminator = term->vt.osc.bel ? "\a" : "\033\\";

                char reply[32];
                size_t n = xsnprintf(
                    reply, sizeof(reply),
                    "\033]4;%u;rgb:%02hhx%02hhx/%02hhx%02hhx/%02hhx%02hhx%s",
                    idx, r, r, g, g, b, b, terminator);
                term_to_slave(term, reply, n);
            }

            else {
                uint32_t color;
                bool color_is_valid = s_color[0] == '#' || s_color[0] == '['
                    ? parse_legacy_color(s_color, &color, NULL, NULL)
                    : parse_rgb(s_color, &color, NULL, NULL);

                if (!color_is_valid)
                    continue;

                LOG_DBG("change color definition for #%u from %06x to %06x",
                        idx, term->colors.table[idx], color);

                term->colors.table[idx] = color;
                term_damage_color(term, COLOR_BASE256, idx);
            }
        }

        break;
    }

    case 7:
        /* Update terminal's understanding of PWD */
        osc_set_pwd(term, string);
        break;

    case 8:
        osc_uri(term, string);
        break;

    case 9:
        /* iTerm2 Growl notifications */
        osc_notify(term, string);
        break;

    case 10:    /* fg */
    case 11:    /* bg */
    case 12:    /* cursor */
    case 17:    /* highlight (selection) fg */
    case 19: {  /* highlight (selection) bg */
        /* Set default foreground/background/highlight-bg/highlight-fg color */

        /* Client queried for current value */
        if (string[0] == '?' && string[1] == '\0') {
            uint32_t color = param == 10
                ? term->colors.fg
                : param == 11
                    ? term->colors.bg
                    : param == 12
                        ? term->colors.cursor_bg
                        : param == 17
                            ? term->colors.selection_bg
                            : term->colors.selection_fg;

            uint8_t r = (color >> 16) & 0xff;
            uint8_t g = (color >>  8) & 0xff;
            uint8_t b = (color >>  0) & 0xff;
            const char *terminator = term->vt.osc.bel ? "\a" : "\033\\";

            /*
             * Reply in XParseColor format
             * E.g. for color 0xdcdccc we reply "\033]10;rgb:dc/dc/cc\033\\"
             */
            char reply[32];
            size_t n = xsnprintf(
                reply, sizeof(reply),
                "\033]%u;rgb:%02hhx%02hhx/%02hhx%02hhx/%02hhx%02hhx%s",
                param, r, r, g, g, b, b, terminator);

            term_to_slave(term, reply, n);
            break;
        }

        uint32_t color;
        bool have_alpha = false;
        uint16_t alpha = 0xffff;

        if (string[0] == '#' || string[0] == '['
            ? !parse_legacy_color(string, &color, &have_alpha, &alpha)
            : !parse_rgb(string, &color, &have_alpha, &alpha))
        {
            break;
        }

        LOG_DBG("change color definition for %s to %06x",
                param == 10 ? "foreground" :
                param == 11 ? "background" :
                param == 12 ? "cursor" :
                param == 17 ? "selection background" :
                              "selection foreground",
                color);

        switch (param) {
        case 10:
            term->colors.fg = color;
            term_damage_color(term, COLOR_DEFAULT, 0);
            break;

        case 11:
            term->colors.bg = color;
            if (have_alpha) {
                const bool changed = term->colors.alpha != alpha;
                term->colors.alpha = alpha;

                if (changed) {
                    wayl_win_alpha_changed(term->window);
                    term_font_subpixel_changed(term);
                }
            }
            term_damage_color(term, COLOR_DEFAULT, 0);
            term_damage_margins(term);
            break;

        case 12:
            term->colors.cursor_bg = 1u << 31 | color;
            term_damage_cursor(term);
            break;

        case 17:
            term->colors.selection_bg = color;
            term->colors.use_custom_selection = true;
            break;

        case 19:
            term->colors.selection_fg = color;
            term->colors.use_custom_selection = true;
            break;
        }

        break;
    }

    case 22:  /* Set mouse cursor */
        term_set_user_mouse_cursor(term, string);
        break;

    case 30:  /* Set tab title */
        break;

    case 52:  /* Copy to/from clipboard/primary */
        osc_selection(term, string);
        break;

    case 99:  /* Kitty notifications */
        kitty_notification(term, string);
        break;

    case 104: {
        /* Reset Color Number 'c' (whole table if no parameter) */

        if (string[0] == '\0') {
            LOG_DBG("resetting all colors");
            for (size_t i = 0; i < ALEN(term->colors.table); i++)
                term->colors.table[i] = term->conf->colors.table[i];
            term_damage_view(term);
        }

        else {
            for (const char *s_idx = strtok(string, ";");
                 s_idx != NULL;
                 s_idx = strtok(NULL, ";"))
            {
                unsigned idx = 0;
                for (; *s_idx != '\0'; s_idx++) {
                    char c = *s_idx;
                    idx *= 10;
                    idx += c - '0';
                }

                if (idx >= ALEN(term->colors.table)) {
                    LOG_WARN("invalid OSC 104 color index: %u", idx);
                    continue;
                }

                LOG_DBG("resetting color #%u", idx);
                term->colors.table[idx] = term->conf->colors.table[idx];
                term_damage_color(term, COLOR_BASE256, idx);
            }

        }
        break;
    }

    case 105: /* Reset Special Color Number 'c' */
        break;

    case 110: /* Reset default text foreground color */
        LOG_DBG("resetting foreground color");
        term->colors.fg = term->conf->colors.fg;
        term_damage_color(term, COLOR_DEFAULT, 0);
        break;

    case 111: /* Reset default text background color */
        LOG_DBG("resetting background color");
        term->colors.bg = term->conf->colors.bg;
        term->colors.alpha = term->conf->colors.alpha;
        term_damage_color(term, COLOR_DEFAULT, 0);
        term_damage_margins(term);
        break;

    case 112:
        LOG_DBG("resetting cursor color");
        term->colors.cursor_fg = term->conf->cursor.color.text;
        term->colors.cursor_bg = term->conf->cursor.color.cursor;
        term_damage_cursor(term);
        break;

    case 117:
        LOG_DBG("resetting selection background color");
        term->colors.selection_bg = term->conf->colors.selection_bg;
        term->colors.use_custom_selection = term->conf->colors.use_custom.selection;
        break;

    case 119:
        LOG_DBG("resetting selection foreground color");
        term->colors.selection_fg = term->conf->colors.selection_fg;
        term->colors.use_custom_selection = term->conf->colors.use_custom.selection;
        break;

    case 133:
        /*
         * Shell integration; see
         * https://iterm2.com/documentation-escape-codes.html (Shell
         * Integration/FinalTerm)
         *
         * [PROMPT]prompt% [COMMAND_START] ls -l
         * [COMMAND_EXECUTED]
         * -rw-r--r-- 1 user group 127 May 1 2016 filename
         * [COMMAND_FINISHED]
         */
        switch (string[0]) {
        case 'A':
            LOG_DBG("FTCS_PROMPT: %dx%d",
                     term->grid->cursor.point.row,
                    term->grid->cursor.point.col);

            term->grid->cur_row->shell_integration.prompt_marker = true;
            break;

        case 'B':
            LOG_DBG("FTCS_COMMAND_START");
            break;

        case 'C':
            LOG_DBG("FTCS_COMMAND_EXECUTED: %dx%d",
                    term->grid->cursor.point.row,
                    term->grid->cursor.point.col);
            term->grid->cur_row->shell_integration.cmd_start = term->grid->cursor.point.col;
            break;

        case 'D':
            LOG_DBG("FTCS_COMMAND_FINISHED: %dx%d",
                    term->grid->cursor.point.row,
                    term->grid->cursor.point.col);
            term->grid->cur_row->shell_integration.cmd_end = term->grid->cursor.point.col;
            break;
        }
        break;

    case 176:
        if (string[0] == '?' && string[1] == '\0') {
            const char *terminator = term->vt.osc.bel ? "\a" : "\033\\";
            char *reply = xasprintf(
                "\033]176;%s%s",
                term->app_id != NULL ? term->app_id : term->conf->app_id,
                terminator);

            term_to_slave(term, reply, strlen(reply));
            free(reply);
            break;
        }

        term_set_app_id(term, string);
        break;

    case 555:
        osc_flash(term);
        break;

    case 777: {
        /*
         * OSC 777 is an URxvt generic escape used to send commands to
         * perl extensions. The generic syntax is: \E]777;<command>;<string>ST
         *
         * We only recognize the 'notify' command, which is, if not
         * well established, at least fairly well known.
         */

        char *param_brk = strchr(string, ';');
        if (param_brk == NULL) {
            UNHANDLED();
            return;
        }

        if (strncmp(string, "notify", param_brk - string) == 0)
            osc_notify(term, param_brk + 1);
        else
            UNHANDLED();
        break;
    }

    default:
        UNHANDLED();
        break;
    }
}

bool
osc_ensure_size(struct terminal *term, size_t required_size)
{
    if (likely(required_size <= term->vt.osc.size))
        return true;

    const size_t pow2_max = ~(SIZE_MAX >> 1);
    if (unlikely(required_size > pow2_max)) {
        LOG_ERR("required OSC buffer size (%zu) exceeds limit (%zu)",
            required_size, pow2_max);
        return false;
    }

    size_t new_size = max(term->vt.osc.size, 4096);
    while (new_size < required_size) {
        new_size <<= 1;
    }

    uint8_t *new_data = realloc(term->vt.osc.data, new_size);
    if (new_data == NULL) {
        LOG_ERRNO("failed to increase size of OSC buffer");
        return false;
    }

    LOG_DBG("resized OSC buffer: %zu", new_size);
    term->vt.osc.data = new_data;
    term->vt.osc.size = new_size;
    return true;
}
