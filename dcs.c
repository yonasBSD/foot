#include "dcs.h"
#include <string.h>

#define LOG_MODULE "dcs"
#define LOG_ENABLE_DBG 0
#include "log.h"
#include "foot-terminfo.h"
#include "sixel.h"
#include "util.h"
#include "vt.h"
#include "xmalloc.h"

static void
bsu(struct terminal *term)
{
    /* https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec */

    size_t n = term->vt.dcs.idx;
    if (unlikely(n > 0))
        LOG_DBG("BSU with unknown params: %.*s)", (int)n, term->vt.dcs.data);

    term_enable_app_sync_updates(term);
}

static void
esu(struct terminal *term)
{
    /* https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec */

    size_t n = term->vt.dcs.idx;
    if (unlikely(n > 0))
        LOG_DBG("ESU with unknown params: %.*s)", (int)n, term->vt.dcs.data);

    term_disable_app_sync_updates(term);
}

/* Decode hex-encoded string *inline*. NULL terminates */
static char *
hex_decode(const char *s, size_t len)
{
    if (len % 2)
        return NULL;

    char *hex = xmalloc(len / 2 + 1);
    char *o = hex;

    /* TODO: error checking */
    for (size_t i = 0; i < len; i += 2) {
        uint8_t nib1 = hex2nibble(*s); s++;
        uint8_t nib2 = hex2nibble(*s); s++;

        if (nib1 == HEX_DIGIT_INVALID || nib2 == HEX_DIGIT_INVALID)
            goto err;

        *o = nib1 << 4 | nib2; o++;
    }

    *o = '\0';
    return hex;

err:
    free(hex);
    return NULL;
}

UNITTEST
{
    /* Verify table is sorted */
    const char *p = terminfo_capabilities;
    size_t left = sizeof(terminfo_capabilities);

    const char *last_cap = NULL;

    while (left > 0) {
        const char *cap = p;
        const char *val = cap + strlen(cap) + 1;

        size_t size = strlen(cap) + 1 + strlen(val) + 1;;
        xassert(size <= left);
        p += size;
        left -= size;

        if (last_cap != NULL)
            xassert(strcmp(last_cap, cap) < 0);

        last_cap = cap;
    }
}

static bool
lookup_capability(const char *name, const char **value)
{
    const char *p = terminfo_capabilities;
    size_t left = sizeof(terminfo_capabilities);

    while (left > 0) {
        const char *cap = p;
        const char *val = cap + strlen(cap) + 1;

        size_t size = strlen(cap) + 1 + strlen(val) + 1;;
        xassert(size <= left);
        p += size;
        left -= size;

        int r = strcmp(cap, name);
        if (r == 0) {
            *value = val;
            return true;
        } else if (r > 0)
            break;
    }

    *value = NULL;
    return false;
}

static void
xtgettcap_reply(struct terminal *term, const char *hex_cap_name, size_t len)
{
    char *name = hex_decode(hex_cap_name, len);
    if (name == NULL)
        goto err;

#if 0
    const struct foot_terminfo_entry *entry =
        bsearch(name, terminfo_capabilities, ALEN(terminfo_capabilities),
                sizeof(*entry), &terminfo_entry_compar);
#endif
    const char *value;
    bool valid_capability = lookup_capability(name, &value);
    xassert(!valid_capability || value != NULL);

    LOG_DBG("XTGETTCAP: cap=%s (%.*s), value=%s",
            name, (int)len, hex_cap_name,
            valid_capability ? value : "<invalid>");

    if (!valid_capability)
        goto err;

    if (value[0] == '\0') {
        /* Boolean */
        term_to_slave(term, "\033P1+r", 5);
        term_to_slave(term, hex_cap_name, len);
        term_to_slave(term, "\033\\", 2);
        goto out;
    }

    /*
     * Reply format:
     *    \EP 1 + r cap=value \E\\
     * Where ‘cap’ and ‘value are hex encoded ascii strings
     */
    char *reply = xmalloc(
        5 +                           /* DCS 1 + r (\EP1+r) */
        len +                         /* capability name, hex encoded */
        1 +                           /* ‘=’ */
        strlen(value) * 2 +           /* capability value, hex encoded */
        2 +                           /* ST (\E\\) */
        1);

    int idx = sprintf(reply, "\033P1+r%.*s=", (int)len, hex_cap_name);

    for (const char *c = value; *c != '\0'; c++) {
        uint8_t nib1 = (uint8_t)*c >> 4;
        uint8_t nib2 = (uint8_t)*c & 0xf;

        reply[idx] = nib1 >= 0xa ? 'A' + nib1 - 0xa : '0' + nib1; idx++;
        reply[idx] = nib2 >= 0xa ? 'A' + nib2 - 0xa : '0' + nib2; idx++;
    }

    reply[idx] = '\033'; idx++;
    reply[idx] = '\\'; idx++;
    term_to_slave(term, reply, idx);

    free(reply);
    goto out;

err:
    term_to_slave(term, "\033P0+r", 5);
    term_to_slave(term, hex_cap_name, len);
    term_to_slave(term, "\033\\", 2);

out:
    free(name);
}

static void
xtgettcap_unhook(struct terminal *term)
{
    size_t left = term->vt.dcs.idx;

    const char *const end = (const char *)&term->vt.dcs.data[left];
    const char *p = (const char *)term->vt.dcs.data;

    while (true) {
        const char *sep = memchr(p, ';', left);
        size_t cap_len;

        if (sep == NULL) {
            /* Last capability */
            cap_len = end - p;
        } else {
            cap_len = sep - p;
        }

        xtgettcap_reply(term, p, cap_len);

        left -= cap_len + 1;
        p += cap_len + 1;

        if (sep == NULL)
            break;
    }
}

void
dcs_hook(struct terminal *term, uint8_t final)
{
    LOG_DBG("hook: %c (intermediate(s): %.2s, param=%d)", final,
            (const char *)&term->vt.private, vt_param_get(term, 0, 0));

    xassert(term->vt.dcs.data == NULL);
    xassert(term->vt.dcs.size == 0);
    xassert(term->vt.dcs.put_handler == NULL);
    xassert(term->vt.dcs.unhook_handler == NULL);

    switch (term->vt.private) {
    case 0:
        switch (final) {
        case 'q': {
            int p1 = vt_param_get(term, 0, 0);
            int p2 = vt_param_get(term, 1,0);
            int p3 = vt_param_get(term, 2, 0);

            sixel_init(term, p1, p2, p3);
            term->vt.dcs.put_handler = &sixel_put;
            term->vt.dcs.unhook_handler = &sixel_unhook;
            break;
        }
        }
        break;

    case '=':
        switch (final) {
        case 's':
            switch (vt_param_get(term, 0, 0)) {
            case 1: term->vt.dcs.unhook_handler = &bsu; return;
            case 2: term->vt.dcs.unhook_handler = &esu; return;
            }
            break;
        }
        break;

    case '+':
        switch (final) {
        case 'q':  /* XTGETTCAP */
            term->vt.dcs.unhook_handler = &xtgettcap_unhook;
            break;
        }
        break;
    }
}

static bool
ensure_size(struct terminal *term, size_t required_size)
{
    if (required_size <= term->vt.dcs.size)
        return true;

    size_t new_size = (required_size + 127) / 128 * 128;
    xassert(new_size > 0);

    uint8_t *new_data = realloc(term->vt.dcs.data, new_size);
    if (new_data == NULL) {
        LOG_ERRNO("failed to increase size of DCS buffer");
        return false;
    }

    term->vt.dcs.data = new_data;
    term->vt.dcs.size = new_size;
    return true;
}

void
dcs_put(struct terminal *term, uint8_t c)
{
    /* LOG_DBG("PUT: %c", c); */

    if (term->vt.dcs.put_handler != NULL)
        term->vt.dcs.put_handler(term, c);
    else {
        if (!ensure_size(term, term->vt.dcs.idx + 1))
            return;
        term->vt.dcs.data[term->vt.dcs.idx++] = c;
    }
}

void
dcs_unhook(struct terminal *term)
{
    if (term->vt.dcs.unhook_handler != NULL)
        term->vt.dcs.unhook_handler(term);

    term->vt.dcs.unhook_handler = NULL;
    term->vt.dcs.put_handler = NULL;

    free(term->vt.dcs.data);
    term->vt.dcs.data = NULL;
    term->vt.dcs.size = 0;
    term->vt.dcs.idx = 0;
}
