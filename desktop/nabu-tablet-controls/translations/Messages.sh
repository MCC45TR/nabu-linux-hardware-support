#!/usr/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
pot="$source_root/translations/plasma_applet_org.senemos.nabu.flashlight.pot"
cd -- "$source_root"
mapfile -d '' sources < <(find kcm plasma/contents \
    -type f \( -name '*.cpp' -o -name '*.qml' \) -print0 | sort -z)

xgettext --from-code=UTF-8 --language=C++ --add-comments=TRANSLATORS \
    --no-location --no-git \
    --keyword=i18n:1 --keyword=i18nc:1c,2 --keyword=i18nd:2 --keyword=i18np:1,2 \
    --package-name=plasma-nabu-kcm --package-version=1.0.0 \
    --output="$pot" "${sources[@]}"
