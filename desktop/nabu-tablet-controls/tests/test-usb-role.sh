#!/usr/bin/env bash

set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
test_root="$(mktemp -d)"
trap 'rm -rf -- "$test_root"' EXIT

fake_systemctl="$test_root/systemctl"
test_binary="$test_root/nabu-usb-role"
call_log="$test_root/calls"

grep -Fq 'return write_role(port_type, role);' "$source_root/src/nabu-usb-role.c"

printf '%s\n' \
    '#!/usr/bin/env bash' \
    'set -u' \
    'case "$1" in' \
    '    show)' \
    '        printf "%s\\n" "${NABU_TEST_LOAD_STATE:-not-found}"' \
    '        exit "${NABU_TEST_SHOW_STATUS:-0}"' \
    '        ;;' \
    '    stop)' \
    '        printf "%s\\n" stop >>"${NABU_TEST_CALL_LOG:?}"' \
    '        exit "${NABU_TEST_STOP_STATUS:-0}"' \
    '        ;;' \
    '    *) exit 64 ;;' \
    'esac' >"$fake_systemctl"
chmod 0755 "$fake_systemctl"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -D_GNU_SOURCE \
    "-DSYSTEMCTL_PATH=\"$fake_systemctl\"" \
    '-DNABU_EFFECTIVE_UID()=0' \
    -o "$test_binary" "$source_root/src/nabu-usb-role.c"

: >"$call_log"
NABU_TEST_LOAD_STATE=not-found NABU_TEST_CALL_LOG="$call_log" \
    "$test_binary" set mode off
test ! -s "$call_log"

: >"$call_log"
NABU_TEST_LOAD_STATE=loaded NABU_TEST_CALL_LOG="$call_log" \
    "$test_binary" set mode off
grep -Fxq stop "$call_log"

: >"$call_log"
if NABU_TEST_LOAD_STATE=loaded NABU_TEST_STOP_STATUS=1 \
    NABU_TEST_CALL_LOG="$call_log" "$test_binary" set mode off; then
    printf '%s\n' 'loaded gadget stop failure was incorrectly ignored' >&2
    exit 1
fi
grep -Fxq stop "$call_log"

: >"$call_log"
NABU_TEST_LOAD_STATE=loaded NABU_TEST_SHOW_STATUS=1 \
    NABU_TEST_CALL_LOG="$call_log" "$test_binary" set mode off
grep -Fxq stop "$call_log"

printf '%s\n' 'USB role service-state tests passed.'
