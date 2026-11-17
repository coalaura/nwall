#!/usr/bin/env bash
# Usage: scripts/smoke.sh <nginx-bin> <prefix> <conf>

set -euo pipefail

nginx=${1:?nginx binary}
prefix=${2:?prefix}
conf=${3:?nginx.conf}

[[ -x "$nginx" ]] || {
	echo "not executable: $nginx" >&2

	exit 1
}

[[ -f "$conf" ]] || {
	echo "missing $conf" >&2

	exit 1
}

mkdir -p "$prefix"/{logs,html,modules}

[[ -f "$prefix/modules/ngx_http_nwall_module.so" ]] || {
	echo "missing $prefix/modules/ngx_http_nwall_module.so" >&2

	exit 1
}

[[ -f "$prefix/html/index.html" ]] || printf 'ok\n' >"$prefix/html/index.html"

"$nginx" -p "$prefix" -c "$conf" -t

master=""
fixtures=$(mktemp -d "$prefix/ua-empty.XXXXXX")

stop_nginx() {
	if [[ -n "${master:-}" ]] && kill -0 "$master" 2>/dev/null; then
		kill -QUIT "$master" 2>/dev/null || kill -TERM "$master" 2>/dev/null || true

		wait "$master" 2>/dev/null || true
	fi
	master=""
}

cleanup() {
	stop_nginx
	rm -rf "$fixtures"
}

trap cleanup EXIT

start_nginx() {
	"$nginx" -p "$prefix" -c "$1" &
	master=$!

	for _ in $(seq 1 50); do
		still_up

		if curl -fsS --max-time 1 http://127.0.0.1:18080/ >/dev/null; then
			return
		fi

		sleep 0.1
	done

	echo "nginx did not become ready" >&2
	exit 1
}

expect_body() {
	local want=$1

	shift

	local got

	got="$(curl -fsS --max-time 5 "$@")"

	if [[ "$got" != "$want" ]]; then
		printf 'expected %q, got %q (%s)\n' "$want" "$got" "$*" >&2

		exit 1
	fi

	echo "pass: $* -> $want"
}

# NGX_HTTP_CLOSE: empty reply / connection reset, not an HTTP status
expect_drop() {
	local label=$1

	shift

	local rc=0

	curl -sS --max-time 5 "$@" >/dev/null || rc=$?

	if [[ $rc -ne 52 && $rc -ne 56 ]]; then
		echo "expected drop: $label" >&2

		exit 1
	fi

	echo "pass: dropped ($rc): $label"
}

still_up() {
	kill -0 "$master" 2>/dev/null || {
		echo "nginx crashed" >&2

		wait "$master" || true

		exit 1
	}
}

# nwall on - allowed
start_nginx "$conf"
expect_body "ok" http://127.0.0.1:18080/
expect_body "ok" -A "Mozilla/5.0" http://127.0.0.1:18080/
expect_body "ok" http://127.0.0.1:18080/about
expect_body "ok" http://127.0.0.1:18080/chosen.php
expect_body "ok" http://127.0.0.1:18080/wp-admin
expect_body "ok" http://127.0.0.1:18080/wp-activate.php
expect_body "ok" http://127.0.0.1:18080/xmlrpc.php
expect_body "ok" http://127.0.0.1:18080/environment

# nwall off - never filtered
expect_body "off" http://127.0.0.1:18081/
expect_body "off" -A sqlmap http://127.0.0.1:18081/
expect_body "off" http://127.0.0.1:18081/secret.env
expect_body "off" http://127.0.0.1:18081/secret.ENV
expect_body "off" http://127.0.0.1:18081/.git/HEAD
expect_body "off" http://127.0.0.1:18081/.GiT/head

still_up

# nwall on - UA
expect_drop "ua sqlmap" -A sqlmap http://127.0.0.1:18080/
expect_drop "empty ua" -A "" http://127.0.0.1:18080/

# nwall on - path (matches current rules/nwall.rules)
expect_drop "path_suffix .env" http://127.0.0.1:18080/secret.env
expect_drop "path_contains /.env" http://127.0.0.1:18080/api/.env.local
expect_drop "path_suffix .env (uppercase)" http://127.0.0.1:18080/secret.ENV
expect_drop "path /.env" http://127.0.0.1:18080/.env
expect_drop "path_suffix /.git" http://127.0.0.1:18080/.git
expect_drop "path_contains /.git/" http://127.0.0.1:18080/.git/HEAD
expect_drop "path /repo/.git" http://127.0.0.1:18080/repo/.git
expect_drop "path /repo/.GIT" http://127.0.0.1:18080/repo/.GIT
expect_drop "path_suffix /wp-admin.php" http://127.0.0.1:18080/wp-admin.php
expect_drop "path /foo/wp-admin.php" http://127.0.0.1:18080/foo/wp-admin.php
expect_drop "path component alfacgiapi" http://127.0.0.1:18080/alfa_data/alfacgiapi/
expect_drop "known shell filename" http://127.0.0.1:18080/wp-content/shell20211028.php
expect_drop "misplaced WordPress shell" http://127.0.0.1:18080/wp-includes/ixr/chosen.php
expect_drop "typo-squatted WordPress endpoint" http://127.0.0.1:18080/wp-activat.php
expect_drop "typo-squatted XML-RPC endpoint" http://127.0.0.1:18080/cgi-bin/xmrlpc.php

still_up

expect_body "ok" http://127.0.0.1:18080/

stop_nginx
sed -e 's|nwall_rules .*;|nwall_rules test.rules;|' -e 's/nwall off;/nwall monitor;/' "$conf" >"$fixtures/nginx.conf"
printf 'ua_empty warn;\npath_exact /blocked;\n' >"$fixtures/test.rules"
start_nginx "$fixtures/nginx.conf"

for header in 'User-Agent:' 'User-Agent;'; do # missing, explicitly empty
	response=$(curl -sS --max-time 5 -H "$header" -w '%{http_code} %{content_type}' http://127.0.0.1:18080/)
	if [[ "$response" != $'Empty user-agent requests are blocked, set a user-agent and retry.\n403 text/plain' ]]; then
		printf 'unexpected warning response: %q\n' "$response" >&2
		exit 1
	fi
done

expect_drop "path drop before UA warning" -A "" http://127.0.0.1:18080/blocked
expect_body "off" -A "" http://127.0.0.1:18081/ # monitor passes through

stop_nginx
printf 'path_exact /blocked;\n' >"$fixtures/test.rules"
start_nginx "$fixtures/nginx.conf"
expect_body "ok" -A "" http://127.0.0.1:18080/
expect_drop "path drop without ua_empty" -A "" http://127.0.0.1:18080/blocked

echo "smoke: ok"
