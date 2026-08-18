# nwall

nginx dynamic module that drops requests matching a small rules file (user-agent and path probes).

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

Build artifacts stay in `.build/` (nginx source) and `.dev/` (prefix: logs, html, modules). Both are gitignored. Override with `NWALL_BUILD` / `NWALL_DEV` if you want them elsewhere.

## Dev loop

```bash
chmod +x scripts/*.sh
./scripts/setup-dev.sh   # once: pacman deps, nginx 1.30.4, local binary
./scripts/build.sh       # after any C change
./scripts/dev.sh         # foreground; logs on stdout/stderr; Ctrl+C kills nginx
```

Then iterate: edit `src/` → `./scripts/build.sh` → `./scripts/dev.sh`.

- http://127.0.0.1:18080 - `nwall on`
- http://127.0.0.1:18081 - `nwall off`

Pin another nginx with `NGINX_VER=1.28.3 ./scripts/setup-dev.sh`. Dynamic modules must be built against the same nginx series you load them into.

`setup-dev.sh` uses `pacman` on Arch. Elsewhere it skips the package step and expects `gcc`, `make`, `curl`, pcre2 and zlib to already be present.

## Production

Build against the **same** nginx version as the host (`nginx -v`), then copy the `.so` into that nginx's modules path and `load_module` it. `scripts/build.sh` prints the copy command after a successful build.
