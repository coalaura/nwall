if [[ -n "${_NWALL_COMMON:-}" ]]; then
	return 0
fi

_NWALL_COMMON=1

NWALL_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NWALL_BUILD="${NWALL_BUILD:-$NWALL_ROOT/.build}"
NWALL_DEV="${NWALL_DEV:-$NWALL_ROOT/.dev}"
NGINX_VER="${NGINX_VER:-1.30.4}"
NGINX_TARBALL="nginx-$NGINX_VER.tar.gz"
NGINX_URL="https://nginx.org/download/$NGINX_TARBALL"
NGINX_SRC="$NWALL_BUILD/nginx-$NGINX_VER"
NGINX_BIN="$NGINX_SRC/objs/nginx"
NWALL_SO="$NGINX_SRC/objs/ngx_http_nwall_module.so"
NWALL_CONF="$NWALL_ROOT/dev/nginx.conf"
NWALL_RULES="$NWALL_ROOT/rules/nwall.rules"

die() {
	echo "error: $*" >&2
	exit 1
}

need() {
	command -v "$1" >/dev/null 2>&1 || die "missing $1"
}

jobs() {
	nproc 2>/dev/null || echo 1
}

prepare_dev_prefix() {
	mkdir -p "$NWALL_DEV"/{logs,html,modules}

	if [[ ! -f "$NWALL_DEV/html/index.html" ]]; then
		printf 'ok\n' >"$NWALL_DEV/html/index.html"
	fi
}

copy_module() {
	[[ -f "$NWALL_SO" ]] || die "module not built: $NWALL_SO (run scripts/build.sh)"

	prepare_dev_prefix

	cp "$NWALL_SO" "$NWALL_DEV/modules/ngx_http_nwall_module.so"
}

require_tree() {
	[[ -d "$NGINX_SRC" && -f "$NGINX_SRC/Makefile" ]] || die "nginx $NGINX_VER is not set up (run scripts/setup-dev.sh)"
}

download_nginx() {
	mkdir -p "$NWALL_BUILD"

	if [[ -d "$NGINX_SRC" ]]; then
		return 0
	fi

	need curl
	need tar

	echo "downloading nginx $NGINX_VER"

	curl -fL --retry 3 -o "$NWALL_BUILD/$NGINX_TARBALL" "$NGINX_URL"
	tar -xf "$NWALL_BUILD/$NGINX_TARBALL" -C "$NWALL_BUILD"

	[[ -d "$NGINX_SRC" ]] || die "unpack failed: expected $NGINX_SRC"
}

configure_nginx() {
	need make
	need gcc

	cd "$NGINX_SRC"

	./configure \
		--prefix="$NWALL_DEV" \
		--with-compat \
		--add-dynamic-module="$NWALL_ROOT"
}