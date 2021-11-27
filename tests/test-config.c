#if !defined(_DEBUG)
 #define _DEBUG
#endif
#undef NDEBUG

#include "../log.h"

#include "../config.c"

#define ALEN(v) (sizeof(v) / sizeof((v)[0]))

/*
 * Stubs
 */

void
user_notification_add_fmt(user_notifications_t *notifications,
                          enum user_notification_kind kind,
                          const char *fmt, ...)
{
}

static void
test_boolean(struct context *ctx, bool (*parse_fun)(struct context *ctx),
             const char *key, bool *conf_ptr)
{
    ctx->key = key;

    static const struct {
        const char *option_string;
        bool value;
        bool invalid;
    } input[] = {
        {"1", true}, {"0", false},
        {"on", true}, {"off", false},
        {"true", true}, {"false", false},
        {"unittest-invalid-boolean-value", false, true},
    };

    for (size_t i = 0; i < ALEN(input); i++) {
        ctx->value = input[i].option_string;

        if (input[i].invalid) {
            if (parse_fun(ctx)) {
                BUG("[%s].%s=%s: did not fail to parse as expected",
                    ctx->section, ctx->key, ctx->value);
            }
        } else {
            if (!parse_fun(ctx)) {
                BUG("[%s].%s=%s: failed to parse",
                    ctx->section, ctx->key, ctx->value);
            }
            if (*conf_ptr != input[i].value)
                BUG("[%s].%s=%s: set value (%s) not the expected one (%s)",
                    ctx->section, ctx->key, ctx->value,
                    *conf_ptr ? "true" : "false",
                    input[i].value ? "true" : "false");
        }
    }
}

static void
test_section_main(void)
{
#define CTX(_key, _value)                                               \
    (struct context){                                                   \
        .conf = &conf, .section = "main", .key = _key, .value = _value, .path = "unittest"}

    struct config conf = {0};
    struct context ctx;

    ctx = CTX("shell", "/bin/bash");
    xassert(parse_section_main(&ctx));
    xassert(strcmp(conf.shell, "/bin/bash") == 0);

    ctx = CTX("term", "foot-unittest");
    xassert(parse_section_main(&ctx));
    xassert(strcmp(conf.term, "foot-unittest") == 0);

    test_boolean(&ctx, &parse_section_main, "login-shell", &conf.login_shell);

    config_free(conf);
#undef CTX
}

int
main(int argc, const char *const *argv)
{
    log_init(LOG_COLORIZE_AUTO, false, 0, LOG_CLASS_ERROR);
    test_section_main();
    log_deinit();
    return 0;
}
