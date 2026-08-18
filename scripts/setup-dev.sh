#!/usr/bin/env bash

set -euo pipefail

. "$(dirname "$0")/common.sh"

install_arch_deps() {
	if ! command -v pacman >/dev/null 2>&1; then
		echo "not Arch (no pacman); install a C toolchain, make, curl, pcre2, zlib"

		return 0
	fi

	local pkgs=(base-devel pcre2 zlib openssl curl)
	local missing=()
	local pkg

	for pkg in "${pkgs[@]}"; do
		if ! pacman -Q "$pkg" >/dev/null 2>&1; then
			missing+=("$pkg")
		fi
	done

	if ((${#missing[@]} == 0)); then
		echo "arch packages already installed"
		
		return 0
	fi

	echo "installing: ${missing[*]}"

	sudo pacman -S --needed "${missing[@]}"
}

echo "repo    $NWALL_ROOT"
echo "build   $NWALL_BUILD"
echo "prefix  $NWALL_DEV"
echo "nginx   $NGINX_VER"
echo

install_arch_deps

need curl
need tar
need make
need gcc

download_nginx
configure_nginx

echo "building nginx + nwall"
make -j"$(jobs)"

prepare_dev_prefix
copy_module

echo
echo "dev tree ready"
echo "  binary  $NGINX_BIN"
echo "  module  $NWALL_DEV/modules/ngx_http_nwall_module.so"
echo
echo "next:"
echo "  ./scripts/build.sh   # rebuild the module after edits"
echo "  ./scripts/dev.sh     # start the foreground dev server"