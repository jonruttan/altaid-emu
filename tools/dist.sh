#!/bin/sh
# SPDX-License-Identifier: MIT

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

fail() {
	echo "[dist] ERROR: $*" 1>&2
	exit 1
}

version_from_source() {
	# Keep this simple and dependency-free.
	# Expect a single #define ALTAID_EMU_VERSION "X.Y.Z" in src/version.c.
	# Use awk (portable across BSD/GNU systems) instead of sed regex extensions.
	awk '
		$1 == "#define" && $2 == "ALTAID_EMU_VERSION" {
			v = $3;
			sub(/^"/, "", v);
			sub(/"$/, "", v);
			print v;
			exit;
		}
	' src/version.c
}

VERSION=$(version_from_source)
if [ -z "$VERSION" ]; then
	fail "could not determine version from src/version.c"
fi

OUTDIR=dist
OUTFILE="$OUTDIR/altaid-emu-$VERSION.tar.xz"

mkdir -p "$OUTDIR"

# Enforce style gate first.
./tools/check_style.sh

# Create a deterministic tarball from tracked content only.
rm -f "$OUTFILE"

if ! command -v git >/dev/null 2>&1; then
	fail "git is required to build a release tarball (uses git archive)"
fi

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	fail "not a git work tree (required for git archive)"
fi

tmp_tar=$(mktemp -t altaid-emu-dist.XXXXXX.tar)
trap 'rm -f "$tmp_tar"' EXIT INT TERM

git archive \
	--format=tar \
	--prefix="altaid-emu-$VERSION/" \
	HEAD >"$tmp_tar"

xz -T0 <"$tmp_tar" >"$OUTFILE"

echo "[dist] Wrote $OUTFILE"
