#!/usr/bin/env bash
set -euo pipefail

# Project style checks.
#
# "Option A" enforcement:
#  - FAIL on trailing whitespace
#  - FAIL on '#pragma once' (prefer include guards)
#  - FAIL on '//' comments in headers (prefer /* */)
#  - WARN on lines > 80 columns (informational)

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

fail() {
	echo "[style] ERROR: $*" 1>&2
	exit 1
}

# 1) Trailing whitespace (tabs/spaces).
# Limit to text-like files.
#
# Note: macOS ships Bash 3.2 by default, which lacks 'mapfile'. Avoid it.
if find . \
		-type f \
		\( -name '*.c' -o -name '*.h' -o -name '*.md' -o -name '*.sh' -o -name '*.py' \) \
		-not -path './.github/*' \
		-not -path './.git/*' \
		-print0 \
	| xargs -0 grep -nH -E "[[:space:]]+$" >/tmp/style_trailing_ws.txt 2>/dev/null; then
	echo "[style] Trailing whitespace found:" 1>&2
	cat /tmp/style_trailing_ws.txt 1>&2
	rm -f /tmp/style_trailing_ws.txt
	fail "trailing whitespace"
fi
rm -f /tmp/style_trailing_ws.txt

# 2) No pragma once.
if grep -R -n "^#pragma once" include >/tmp/style_pragma_once.txt 2>/dev/null; then
	echo "[style] '#pragma once' found in headers:" 1>&2
	cat /tmp/style_pragma_once.txt 1>&2
	rm -f /tmp/style_pragma_once.txt
	fail "#pragma once"
fi
rm -f /tmp/style_pragma_once.txt

# 3) No // comments in headers.
if grep -R -n "//" include >/tmp/style_cpp_comments.txt 2>/dev/null; then
	echo "[style] '//' comments found in headers:" 1>&2
	cat /tmp/style_cpp_comments.txt 1>&2
	rm -f /tmp/style_cpp_comments.txt
	fail "// comments in headers"
fi
rm -f /tmp/style_cpp_comments.txt

# 4) Warn on >80 columns.
# (Don't fail; large opcode tables and long format strings sometimes need this.)
WARNED=0
for f in $(find src include -type f \( -name '*.c' -o -name '*.h' \)); do
	awk -v file="$f" 'length($0) > 80 { printf("%s:%d:%d\n", file, NR, length($0)); }' "$f" || true
done > /tmp/style_long_lines.txt

if [ -s /tmp/style_long_lines.txt ]; then
	WARNED=1
	echo "[style] WARNING: lines > 80 columns (info):" 1>&2
	head -n 200 /tmp/style_long_lines.txt 1>&2
	if [ $(wc -l < /tmp/style_long_lines.txt) -gt 200 ]; then
		echo "[style] (truncated; $(wc -l < /tmp/style_long_lines.txt) total)" 1>&2
	fi
fi
rm -f /tmp/style_long_lines.txt

if [ "$WARNED" -eq 0 ]; then
	echo "[style] OK" 1>&2
fi
