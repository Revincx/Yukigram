#!/usr/bin/env bash

set -Eeuo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${repo_root}/out"
deps_dir="${output_dir}/deps"
tdlib_build_dir="${output_dir}/tde2e-build"
tdlib_source_dir="${TDLIB_SOURCE_DIR:-${repo_root}/../yurigram-pkgbuild/src/telegram-tdlib}"
mold_linker_flag="-fuse-ld=mold"

bail() {
	echo "$*" >&2
	exit 1
}

validate_api() {
	[[ "${API_ID:-}" =~ ^[1-9][0-9]*$ ]] \
		|| bail "API_ID must be a positive number"
	[[ "${API_HASH:-}" =~ ^[0-9a-f]{32}$ ]] \
		|| bail "API_HASH must contain 32 lowercase hexadecimal digits"
}

command -v cmake >/dev/null || bail "cmake is required"
command -v ninja >/dev/null || bail "ninja is required"
command -v mold >/dev/null || bail "mold is required"
[[ -f "${repo_root}/CMakeLists.txt" ]] || bail "Not a Telegram Desktop checkout: ${repo_root}"
[[ -f "${tdlib_source_dir}/CMakeLists.txt" ]] \
	|| bail "TDLib source not found: ${tdlib_source_dir} (set TDLIB_SOURCE_DIR)"

validate_api

cmake -S "${tdlib_source_dir}" -B "${tdlib_build_dir}" -G Ninja \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_INSTALL_PREFIX="${deps_dir}" \
	-DTD_E2E_ONLY=ON \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DCMAKE_EXE_LINKER_FLAGS="${mold_linker_flag}" \
	-Wno-dev

cmake --build "${tdlib_build_dir}" --parallel
cmake --install "${tdlib_build_dir}"

cmake -S "${repo_root}" -B "${output_dir}" -G Ninja \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_PREFIX_PATH="${deps_dir}" \
	-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
	-DCMAKE_EXE_LINKER_FLAGS="${mold_linker_flag}" \
	-DTDESKTOP_API_ID="${API_ID}" \
	-DTDESKTOP_API_HASH="${API_HASH}" \
	-DDESKTOP_APP_DISABLE_AUTOUPDATE=ON \
	-DDESKTOP_APP_USE_PACKAGED=ON \
    -DDESKTOP_APP_USE_PANGO=OFF \
	-DCMAKE_JOB_POOLS="compile=10;link=12" \
	-DCMAKE_JOB_POOL_COMPILE=compile \
	-DCMAKE_JOB_POOL_LINK=link \
	-Wno-dev

cmake --build "${output_dir}" --target Telegram --parallel

echo "Debug binary: ${output_dir}/yurigram"
