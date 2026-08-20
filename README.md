# nwall

nginx dynamic module. One rules file, early hard close on matching User-Agent or URI.

## nginx

```nginx
load_module modules/ngx_http_nwall_module.so;

http {
    nwall_rules /etc/nginx/nwall.rules;   # http{} only, once

    server {
        nwall monitor;                    # off, monitor, or on; default off
        nwall_log basic;                  # off, basic, or full; default basic
    }
}
```

`nginx -s reload` reloads the rules file. `nwall on` without `nwall_rules` is a config error.

## Rules

One directive per line. `#` comments. Quoted or bare values; `\n` `\t` `\r` `\"` `\\` in quotes.

| directive | matches |
|---|---|
| `ua_empty;` | missing or empty `User-Agent` |
| `ua_prefix` / `ua_suffix` / `ua_exact` / `ua_contains` | User-Agent |
| `path_prefix` / `path_suffix` / `path_exact` / `path_contains` | URI path (`r->uri`, decoded, no query) |
| `path_component` | Exact URI path component or component sequence, matched at component boundaries |

All matches are case-insensitive. Empty patterns (`path_contains "";`) are a config error.

`nwall monitor` logs matching requests but allows them. `nwall on` logs and closes matching requests.

`nwall_log basic` logs the client, rule type, and rule value. `full` also logs the original User-Agent and URI. `off` disables per-request nwall logs.

`ua_empty` will also drop health checks and scripted clients that send no User-Agent. Give those probes a UA or put them on a `nwall off` server.

`path_component` values do not include leading or trailing slashes. They may contain multiple components, such as `path_component "telescope/requests";`.

See [`rules/nwall.rules`](rules/nwall.rules).

## Logs

Every nwall line is prefixed `nwall:` (error log). A drop looks like:

```
nwall: drop client:1.2.3.4 rule:path_component value:".git"
```

`ua` / `uri` are the original request values. Missing UA is logged as `-`.

## Layout

```
config                 nginx addon description (--add-dynamic-module)
src/                   module sources
rules/nwall.rules      default rule set
dev/nginx.conf         foreground dev server
scripts/setup-dev.sh   packages + local nginx tree (Arch / pacman)
scripts/build.sh       rebuild the .so
scripts/dev.sh         run the dev server; Ctrl+C stops it
```

Build artifacts stay in `.build/` (nginx source) and `.dev/` (prefix: logs, html, modules). Both are gitignored. Override with `NWALL_BUILD` / `NWALL_DEV`.

## Dev loop

```bash
chmod +x scripts/*.sh
./scripts/setup-dev.sh   # once: pacman deps, nginx 1.30.4, local binary
./scripts/build.sh       # after any C change
./scripts/dev.sh         # foreground; logs on stdout/stderr; Ctrl+C kills nginx
```

- http://127.0.0.1:18080 - `nwall on`
- http://127.0.0.1:18081 - `nwall off`

Pin another nginx with `NGINX_VER=1.28.3 ./scripts/setup-dev.sh`. Dynamic modules must match the exact nginx version they load into.

`setup-dev.sh` uses `pacman` on Arch. Elsewhere it skips packages and expects `gcc`, `make`, `curl`, pcre2 and zlib.

## Production

Build against the same nginx as the host (`nginx -v`), copy the `.so` into that nginx's modules path, `load_module` it. `scripts/build.sh` prints the copy command.
