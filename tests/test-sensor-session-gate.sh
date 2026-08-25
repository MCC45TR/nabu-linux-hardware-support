#!/usr/bin/bash
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
helper=${project_dir}/systemd/nabu-sensor-session-gate
test_root=$(mktemp -d)
trap 'rm -rf "${test_root}"' EXIT

cat >"${test_root}/busctl" <<'EOF'
#!/usr/bin/bash
state=${NABU_SENSOR_GATE_TEST_STATE:?}
if [[ -e ${state}/restarted ]]; then
	echo 'b true'
else
	echo 'b false'
fi
EOF
cat >"${test_root}/systemctl" <<'EOF'
#!/usr/bin/bash
state=${NABU_SENSOR_GATE_TEST_STATE:?}
printf '%s\n' "$*" >>"${state}/systemctl.log"
touch "${state}/restarted"
EOF
cat >"${test_root}/sleep" <<'EOF'
#!/usr/bin/bash
:
EOF
chmod +x "${test_root}/busctl" "${test_root}/systemctl" "${test_root}/sleep"

NABU_SENSOR_GATE_TEST_STATE=${test_root} \
NABU_SENSOR_GATE_BUSCTL=${test_root}/busctl \
NABU_SENSOR_GATE_SYSTEMCTL=${test_root}/systemctl \
NABU_SENSOR_GATE_SLEEP=${test_root}/sleep \
NABU_SENSOR_GATE_INITIAL_ATTEMPTS=1 \
NABU_SENSOR_GATE_RECOVERY_ATTEMPTS=1 \
"${helper}" >"${test_root}/success.log" 2>&1

grep -Fxq 'restart iio-sensor-proxy.service' "${test_root}/systemctl.log"
grep -Fq 'became ready after one bounded restart' "${test_root}/success.log"

rm -f "${test_root}/restarted" "${test_root}/systemctl.log"
cat >"${test_root}/systemctl" <<'EOF'
#!/usr/bin/bash
exit 1
EOF
chmod +x "${test_root}/systemctl"

NABU_SENSOR_GATE_TEST_STATE=${test_root} \
NABU_SENSOR_GATE_BUSCTL=${test_root}/busctl \
NABU_SENSOR_GATE_SYSTEMCTL=${test_root}/systemctl \
NABU_SENSOR_GATE_SLEEP=${test_root}/sleep \
NABU_SENSOR_GATE_INITIAL_ATTEMPTS=1 \
NABU_SENSOR_GATE_RECOVERY_ATTEMPTS=1 \
"${helper}" >"${test_root}/fallback.log" 2>&1

grep -Fq 'continuing without blocking the desktop' "${test_root}/fallback.log"
echo 'sensor session gate tests passed'
