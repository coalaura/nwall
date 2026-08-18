#!/usr/bin/env bash

set -euo pipefail

. "$(dirname "$0")/common.sh"

PIDFILE="$NWALL_DEV/logs/nginx.pid"
NGINX_PID=

[[ -f "$NWALL_CONF" ]] || die "missing $NWALL_CONF"
[[ -f "$NWALL_RULES" ]] || die "missing $NWALL_RULES"

require_tree

[[ -x "$NGINX_BIN" ]] || die "missing $NGINX_BIN (run scripts/setup-dev.sh)"

running_pid() {
	local pid

	[[ -f "$PIDFILE" ]] || return 1

	pid="$(cat "$PIDFILE" 2>/dev/null || true)"

	[[ -n "$pid" ]] || return 1

	kill -0 "$pid" 2>/dev/null || return 1

	echo "$pid"
}

cleanup() {
	trap - INT TERM EXIT

	local pid="${NGINX_PID:-}"

	if [[ -z "$pid" ]]; then
		pid="$(running_pid || true)"
	fi

	if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
		echo
		echo "stopping nginx (pid $pid)"

		kill -QUIT "$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true

		wait "$pid" 2>/dev/null || true
	fi
}

if pid="$(running_pid)"; then
	die "already running pid=$pid (Ctrl+C that dev.sh, or kill $pid)"
fi

rm -f "$PIDFILE"

copy_module

"$NGINX_BIN" -p "$NWALL_DEV" -c "$NWALL_CONF" -t

echo "nwall-dev  :18080 (nwall on)  :18081 (nwall off)"
echo "prefix     $NWALL_DEV"
echo "module     $NWALL_DEV/modules/ngx_http_nwall_module.so"
echo "Ctrl+C to stop"
echo

trap cleanup INT TERM EXIT

"$NGINX_BIN" -p "$NWALL_DEV" -c "$NWALL_CONF" &

NGINX_PID=$!

wait "$NGINX_PID" || true

NGINX_PID=

trap - INT TERM EXIT