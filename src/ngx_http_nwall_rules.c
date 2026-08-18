#include "ngx_http_nwall_module.h"

typedef struct {
    u_char      *p;
    u_char      *end;
    ngx_uint_t   line;
    ngx_str_t   *file;
    ngx_pool_t  *pool;
    ngx_conf_t  *cf;
} ngx_http_nwall_parser_t;

static ngx_int_t ngx_http_nwall_contains(ngx_str_t *hay, ngx_str_t *needle);
static ngx_int_t ngx_http_nwall_component(ngx_str_t *path, ngx_str_t *component);
static void ngx_http_nwall_skip(ngx_http_nwall_parser_t *ps);
static ngx_int_t ngx_http_nwall_ident(ngx_http_nwall_parser_t *ps, ngx_str_t *dst);
static ngx_int_t ngx_http_nwall_value(ngx_http_nwall_parser_t *ps, ngx_str_t *dst);
static ngx_int_t ngx_http_nwall_expect_semi(ngx_http_nwall_parser_t *ps);
static ngx_int_t ngx_http_nwall_rule_kind(ngx_str_t *ident, ngx_uint_t *target, ngx_uint_t *op, ngx_uint_t *needs_value);
static char *ngx_http_nwall_parse_buf(ngx_http_nwall_parser_t *ps, ngx_http_nwall_ruleset_t *rs);

static ngx_int_t ngx_http_nwall_contains(ngx_str_t *hay, ngx_str_t *needle)
{
    size_t  i, last;

    if (needle->len == 0) {
        return 1;
    }

    if (hay->len < needle->len) {
        return 0;
    }

    last = hay->len - needle->len;

    for (i = 0; i <= last; i++) {
        if (ngx_strncasecmp(hay->data + i, needle->data, needle->len) == 0) {
            return 1;
        }
    }

    return 0;
}

static ngx_int_t ngx_http_nwall_component(ngx_str_t *path, ngx_str_t *component)
{
    size_t  i, last, end;

    if (component->len == 0 || path->len < component->len) {
        return 0;
    }

    last = path->len - component->len;

    for (i = 0; i <= last; i++) {
        if (i != 0 && path->data[i - 1] != '/') {
            continue;
        }

        end = i + component->len;

        if (end != path->len && path->data[end] != '/') {
            continue;
        }

        if (ngx_strncasecmp(path->data + i, component->data, component->len) == 0) {
            return 1;
        }
    }

    return 0;
}

ngx_http_nwall_rule_t * ngx_http_nwall_match(ngx_http_request_t *r, ngx_http_nwall_ruleset_t *rs)
{
    ngx_str_t              ua, *subject;
    ngx_uint_t             i;
    ngx_http_nwall_rule_t *rule, *rules;

    if (rs == NULL || rs->rules == NULL || rs->rules->nelts == 0) {
        return NULL;
    }

    if (r->headers_in.user_agent != NULL) {
        ua = r->headers_in.user_agent->value;
    } else {
        ngx_str_null(&ua);
    }

    rules = rs->rules->elts;

    for (i = 0; i < rs->rules->nelts; i++) {
        rule = &rules[i];

        if (rule->target == NWALL_TARGET_UA) {
            subject = &ua;

            if (rule->op == NWALL_OP_EMPTY) {
                if (ua.len == 0) {
                    return rule;
                }

                continue;
            }

        } else {
            subject = &r->uri;
        }

        switch (rule->op) {
        case NWALL_OP_EXACT:
            if (subject->len == rule->pattern.len && (subject->len == 0 || ngx_strncasecmp(subject->data, rule->pattern.data, subject->len) == 0)) {
                return rule;
            }

            break;
        case NWALL_OP_PREFIX:
            if (subject->len >= rule->pattern.len && ngx_strncasecmp(subject->data, rule->pattern.data, rule->pattern.len) == 0) {
                return rule;
            }

            break;
        case NWALL_OP_SUFFIX:
            if (subject->len >= rule->pattern.len && ngx_strncasecmp(subject->data + (subject->len - rule->pattern.len), rule->pattern.data, rule->pattern.len) == 0) {
                return rule;
            }

            break;
        case NWALL_OP_CONTAINS:
            if (ngx_http_nwall_contains(subject, &rule->pattern)) {
                return rule;
            }

            break;
        case NWALL_OP_COMPONENT:
            if (ngx_http_nwall_component(subject, &rule->pattern)) {
                return rule;
            }

            break;
        default:
            break;
        }
    }

    return NULL;
}

static void ngx_http_nwall_skip(ngx_http_nwall_parser_t *ps)
{
    for ( ;; ) {
        while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\r'))
        {
            ps->p++;
        }

        if (ps->p < ps->end && *ps->p == '\n') {
            ps->p++;
            ps->line++;

            continue;
        }

        if (ps->p < ps->end && *ps->p == '#') {
            while (ps->p < ps->end && *ps->p != '\n') {
                ps->p++;
            }

            continue;
        }

        return;
    }
}

static ngx_int_t ngx_http_nwall_ident(ngx_http_nwall_parser_t *ps, ngx_str_t *dst)
{
    u_char  *start;

    ngx_http_nwall_skip(ps);

    if (ps->p >= ps->end) {
        return NGX_DONE;
    }

    start = ps->p;

    if (!((*ps->p >= 'a' && *ps->p <= 'z') || (*ps->p >= 'A' && *ps->p <= 'Z') || *ps->p == '_'))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: expected directive name", ps->file, ps->line);
        return NGX_ERROR;
    }

    ps->p++;

    while (ps->p < ps->end && ((*ps->p >= 'a' && *ps->p <= 'z') || (*ps->p >= 'A' && *ps->p <= 'Z') || (*ps->p >= '0' && *ps->p <= '9') || *ps->p == '_'))
    {
        ps->p++;
    }

    dst->data = start;
    dst->len = ps->p - start;

    return NGX_OK;
}

static ngx_int_t ngx_http_nwall_value(ngx_http_nwall_parser_t *ps, ngx_str_t *dst)
{
    u_char  *start, *q, ch;

    ngx_http_nwall_skip(ps);

    if (ps->p >= ps->end) {
        ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: expected value", ps->file, ps->line);

        return NGX_ERROR;
    }

    if (*ps->p == '"') {
        ps->p++;

        start = ps->p;

        while (ps->p < ps->end && *ps->p != '"' && *ps->p != '\n') {
            if (*ps->p == '\\' && ps->p + 1 < ps->end) {
                ps->p += 2;

                continue;
            }

            ps->p++;
        }

        if (ps->p >= ps->end || *ps->p != '"') {
            ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: unterminated string", ps->file, ps->line);

            return NGX_ERROR;
        }

        dst->len = ps->p - start;
        dst->data = ngx_pnalloc(ps->pool, dst->len + 1);

        if (dst->data == NULL) {
            return NGX_ERROR;
        }

        /* unescape \n \t \" \\ - enough for rules files */
        q = dst->data;

        while (start < ps->p) {
            if (*start == '\\' && start + 1 < ps->p) {
                start++;

				ch = *start++;

                switch (ch) {
                case 'n':
					*q++ = '\n';
					break;
                case 't':
					*q++ = '\t';
					break;
                case 'r':
					*q++ = '\r';
					break;
                default:
					*q++ = ch;
					break;
                }
            } else {
                *q++ = *start++;
            }
        }

        dst->len = q - dst->data;

		*q = '\0';

		ps->p++; /* closing quote */

        return NGX_OK;
    }

    start = ps->p;

    while (ps->p < ps->end && *ps->p != ';' && *ps->p != ' ' && *ps->p != '\t' && *ps->p != '\r' && *ps->p != '\n' && *ps->p != '#')
    {
        ps->p++;
    }

    if (ps->p == start) {
        ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: empty value", ps->file, ps->line);

        return NGX_ERROR;
    }

    dst->len = ps->p - start;
    dst->data = ngx_pnalloc(ps->pool, dst->len + 1);

	if (dst->data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(dst->data, start, dst->len);

	dst->data[dst->len] = '\0';

    return NGX_OK;
}

static ngx_int_t ngx_http_nwall_expect_semi(ngx_http_nwall_parser_t *ps)
{
    ngx_http_nwall_skip(ps);

    if (ps->p >= ps->end || *ps->p != ';') {
        ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: expected \";\"", ps->file, ps->line);

        return NGX_ERROR;
    }

    ps->p++;

    return NGX_OK;
}

static ngx_int_t ngx_http_nwall_rule_kind(ngx_str_t *ident, ngx_uint_t *target, ngx_uint_t *op, ngx_uint_t *needs_value)
{
    *needs_value = 1;

    if (ident->len == 8 && ngx_strncmp(ident->data, "ua_empty", 8) == 0) {
        *target = NWALL_TARGET_UA;
		*op = NWALL_OP_EMPTY;
        *needs_value = 0;
        return NGX_OK;
    }

    if (ident->len > 3 && ngx_strncmp(ident->data, "ua_", 3) == 0) {
        *target = NWALL_TARGET_UA;

        ident->data += 3;
        ident->len -= 3;
    } else if (ident->len > 5 && ngx_strncmp(ident->data, "path_", 5) == 0) {
        *target = NWALL_TARGET_PATH;

        ident->data += 5;
        ident->len -= 5;
    } else {
        return NGX_DECLINED;
    }

    if (ident->len == 6 && ngx_strncmp(ident->data, "prefix", 6) == 0) {
        *op = NWALL_OP_PREFIX;

        return NGX_OK;
    }

    if (ident->len == 6 && ngx_strncmp(ident->data, "suffix", 6) == 0) {
        *op = NWALL_OP_SUFFIX;

        return NGX_OK;
    }

    if (ident->len == 5 && ngx_strncmp(ident->data, "exact", 5) == 0) {
        *op = NWALL_OP_EXACT;

        return NGX_OK;
    }

    if (ident->len == 8 && ngx_strncmp(ident->data, "contains", 8) == 0) {
        *op = NWALL_OP_CONTAINS;

        return NGX_OK;
    }

    if (*target == NWALL_TARGET_PATH && ident->len == 9 && ngx_strncmp(ident->data, "component", 9) == 0) {
        *op = NWALL_OP_COMPONENT;

        return NGX_OK;
    }

    return NGX_DECLINED;
}

static char * ngx_http_nwall_parse_buf(ngx_http_nwall_parser_t *ps, ngx_http_nwall_ruleset_t *rs)
{
    ngx_int_t              rc;
    ngx_str_t              ident, value, kind;
    ngx_uint_t             i, target, op, needs_value;
    ngx_http_nwall_rule_t *rule;

    for ( ;; ) {
        rc = ngx_http_nwall_ident(ps, &ident);
        if (rc == NGX_DONE) {
            return NGX_CONF_OK;
        }

        if (rc != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        kind = ident;

        if (ngx_http_nwall_rule_kind(&ident, &target, &op, &needs_value) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: unknown directive \"%V\"", ps->file, ps->line, &kind);
            return NGX_CONF_ERROR;
        }

        ngx_str_null(&value);

        if (needs_value) {
            if (ngx_http_nwall_value(ps, &value) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            if (value.len == 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: empty pattern", ps->file, ps->line);

                return NGX_CONF_ERROR;
            }

            if (op == NWALL_OP_COMPONENT) {
                if (value.data[0] == '/' || value.data[value.len - 1] == '/') {
                    ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: path_component value must not begin or end with \"/\"", ps->file, ps->line);

                    return NGX_CONF_ERROR;
                }

                for (i = 1; i < value.len; i++) {
                    if (value.data[i - 1] == '/' && value.data[i] == '/') {
                        ngx_conf_log_error(NGX_LOG_EMERG, ps->cf, 0, "nwall: %V:%ui: path_component value must not contain \"//\"", ps->file, ps->line);

                        return NGX_CONF_ERROR;
                    }
                }
            }
        }

        if (ngx_http_nwall_expect_semi(ps) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        rule = ngx_array_push(rs->rules);
        if (rule == NULL) {
            return NGX_CONF_ERROR;
        }

        rule->target = target;
        rule->op = op;
        rule->pattern = value;
        rule->kind.len = kind.len;
        rule->kind.data = ngx_pnalloc(ps->pool, kind.len);

        if (rule->kind.data == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memcpy(rule->kind.data, kind.data, kind.len);

        if (rule->pattern.len) {
            ngx_strlow(rule->pattern.data, rule->pattern.data, rule->pattern.len);
        }
    }
}

char * ngx_http_nwall_load_rules(ngx_conf_t *cf, ngx_str_t *path, ngx_http_nwall_ruleset_t **out)
{
    ssize_t                    n;
    ngx_fd_t                   fd;
    ngx_str_t                  full;
    ngx_file_info_t            fi;
    size_t                     size;
    u_char                    *buf;
    ngx_http_nwall_parser_t    ps;
    ngx_http_nwall_ruleset_t  *rs;

    full = *path;

    if (ngx_conf_full_name(cf->cycle, &full, 1) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    fd = ngx_open_file(full.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno, "nwall: " ngx_open_file_n " \"%V\" failed", &full);

		return NGX_CONF_ERROR;
    }

    if (ngx_fd_info(fd, &fi) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno, "nwall: " ngx_fd_info_n " \"%V\" failed", &full);

		goto failed;
    }

    size = (size_t) ngx_file_size(&fi);

    buf = ngx_pnalloc(cf->temp_pool, size + 1);
    if (buf == NULL) {
        goto failed;
    }

    n = ngx_read_fd(fd, buf, size);
    if (n == -1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno, "nwall: " ngx_read_fd_n " \"%V\" failed", &full);

        goto failed;
    }

    if ((size_t) n != size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "nwall: " ngx_read_fd_n " \"%V\" returned only %z of %uz", &full, n, size);

		goto failed;
    }

    buf[size] = '\0';

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_ALERT, cf, ngx_errno, "nwall: " ngx_close_file_n " \"%V\" failed", &full);
    }

    fd = NGX_INVALID_FILE;

    rs = ngx_pcalloc(cf->pool, sizeof(ngx_http_nwall_ruleset_t));
    if (rs == NULL) {
        return NGX_CONF_ERROR;
    }

    rs->rules = ngx_array_create(cf->pool, 64, sizeof(ngx_http_nwall_rule_t));
    if (rs->rules == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(&ps, sizeof(ngx_http_nwall_parser_t));

    ps.p = buf;
    ps.end = buf + size;
    ps.line = 1;
    ps.file = &full;
    ps.pool = cf->pool;
    ps.cf = cf;

    if (ngx_http_nwall_parse_buf(&ps, rs) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "nwall: loaded %ui rules from \"%V\"", rs->rules->nelts, &full);

    *out = rs;

    return NGX_CONF_OK;

failed:

    if (fd != NGX_INVALID_FILE) {
        (void) ngx_close_file(fd);
    }

    return NGX_CONF_ERROR;
}