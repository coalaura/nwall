#ifndef _NGX_HTTP_NWALL_MODULE_H_INCLUDED_
#define _NGX_HTTP_NWALL_MODULE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NWALL_TARGET_UA     0
#define NWALL_TARGET_PATH   1

#define NWALL_OP_PREFIX     0
#define NWALL_OP_SUFFIX     1
#define NWALL_OP_EXACT      2
#define NWALL_OP_CONTAINS   3
#define NWALL_OP_EMPTY      4

typedef struct {
    ngx_uint_t  target;
    ngx_uint_t  op;
    ngx_str_t   pattern;   /* UA patterns stored lowercase */
    ngx_str_t   kind;      /* e.g. "ua_contains" - for logs */
} ngx_http_nwall_rule_t;

typedef struct {
    ngx_array_t  *rules;   /* ngx_http_nwall_rule_t */
} ngx_http_nwall_ruleset_t;

typedef struct {
    ngx_str_t                  path;
    ngx_http_nwall_ruleset_t  *ruleset;
} ngx_http_nwall_main_conf_t;

typedef struct {
    ngx_flag_t  enable;
} ngx_http_nwall_srv_conf_t;

char *ngx_http_nwall_load_rules(ngx_conf_t *cf, ngx_str_t *path, ngx_http_nwall_ruleset_t **out);

ngx_http_nwall_rule_t *ngx_http_nwall_match(ngx_http_request_t *r, ngx_http_nwall_ruleset_t *rs);

#endif /* _NGX_HTTP_NWALL_MODULE_H_INCLUDED_ */