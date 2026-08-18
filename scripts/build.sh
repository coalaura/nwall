#!/usr/bin/env bash

set -euo pipefail

. "$(dirname "$0")/common.sh"

need make
need gcc

if [[ ! -d "$NGINX_SRC" || ! -f "$NGINX_SRC/Makefile" ]]; then
	echo "nginx $NGINX_VER not configured; running a module-only setup"

	download_nginx
	configure_nginx
fi

cd "$NGINX_SRC"

echo "building nwall against nginx $NGINX_VER"

make modules -j"$(jobs)"

[[ -f "$NWALL_SO" ]] || die "build produced no $NWALL_SO"

ls -l "$NWALL_SO"

if [[ -d "$NWALL_DEV" ]]; then
	copy_module

	echo "copied -> $NWALL_DEV/modules/ngx_http_nwall_module.so"
fi

echo
echo "production install (must match this nginx $NGINX_VER):"
echo "  sudo cp $NWALL_SO \"\$(nginx -V 2>&1 | sed -n 's/.*--modules-path=\\([^ ]*\\).*/\\1/p')\""