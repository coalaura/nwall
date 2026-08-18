#include "ngx_http_nwall_module.h"

static ngx_int_t ngx_http_nwall_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_nwall_init(ngx_conf_t *cf);
static void *ngx_http_nwall_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_nwall_create_srv_conf(ngx_conf_t *cf);
static char *ngx_http_nwall_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_http_nwall_rules(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t  ngx_http_nwall_commands[] = {
    {
		ngx_string("nwall_rules"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_http_nwall_rules,
		NGX_HTTP_MAIN_CONF_OFFSET,
		0,
		NULL
	},

    {
		ngx_string("nwall"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_SRV_CONF_OFFSET,
		offsetof(ngx_http_nwall_srv_conf_t, enable),
		NULL
	},

	ngx_null_command
};

static ngx_http_module_t  ngx_http_nwall_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_nwall_init,                   /* postconfiguration */

    ngx_http_nwall_create_main_conf,       /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_http_nwall_create_srv_conf,        /* create server configuration */
    ngx_http_nwall_merge_srv_conf,         /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};

ngx_module_t  ngx_http_nwall_module = {
    NGX_MODULE_V1,
    &ngx_http_nwall_module_ctx,            /* module context */
    ngx_http_nwall_commands,               /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_nwall_handler(ngx_http_request_t *r)
{
    ngx_http_nwall_rule_t      *hit;
    ngx_http_nwall_srv_conf_t  *nscf;
    ngx_http_nwall_main_conf_t *nmcf;
    ngx_str_t                   ua;

    /* subrequests share the client UA/URI; only judge the main request */
    if (r != r->main) {
        return NGX_DECLINED;
    }

    nscf = ngx_http_get_module_srv_conf(r, ngx_http_nwall_module);
    if (nscf->enable != 1) {
        return NGX_DECLINED;
    }

    nmcf = ngx_http_get_module_main_conf(r, ngx_http_nwall_module);
    if (nmcf->ruleset == NULL) {
        return NGX_DECLINED;
    }

    hit = ngx_http_nwall_match(r, nmcf->ruleset);
    if (hit == NULL) {
        return NGX_DECLINED;
    }

    if (r->headers_in.user_agent != NULL) {
        ua = r->headers_in.user_agent->value;
    } else {
        ngx_str_set(&ua, "-");
    }

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "nwall: drop %V \"%V\" ua:\"%V\" uri:\"%V\"", &hit->kind, &hit->pattern, &ua, &r->uri);

    return NGX_HTTP_CLOSE;
}

static ngx_int_t ngx_http_nwall_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_nwall_handler;

    return NGX_OK;
}

static void * ngx_http_nwall_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_nwall_main_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_nwall_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}

static void * ngx_http_nwall_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_nwall_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_nwall_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;

    return conf;
}

static char * ngx_http_nwall_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_nwall_srv_conf_t   *prev = parent;
    ngx_http_nwall_srv_conf_t   *conf = child;
    ngx_http_nwall_main_conf_t  *nmcf;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    if (conf->enable == 1) {
        nmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_nwall_module);

        if (nmcf->ruleset == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "nwall on but nwall_rules was not set");

            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

static char * ngx_http_nwall_rules(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                   *value;
    ngx_http_nwall_main_conf_t  *nmcf = conf;

    if (nmcf->ruleset != NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;
    nmcf->path = value[1];

    return ngx_http_nwall_load_rules(cf, &nmcf->path, &nmcf->ruleset);
}