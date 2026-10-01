#!/usr/bin/env bash
# Downloads Crytek Sponza (Frank Meinl / Crytek, CC BY 3.0; OBJ version cleaned up by Morgan McGuire)
# into <destination> (default: assets-large/sponza, ignored by git). About 80 MB.
#
#     tools/bench/download_sponza.sh && build/bin/kronk3d_bench assets-large/sponza/sponza.obj
set -euo pipefail

destination="${1:-assets-large/sponza}"
url="https://casual-effects.com/g3d/data10/common/model/crytek_sponza/sponza.zip"

mkdir -p "$destination"
curl -fL --progress-bar -o "$destination/sponza.zip" "$url"
unzip -q -o "$destination/sponza.zip" -d "$destination"
rm "$destination/sponza.zip"
echo "Sponza is in $destination/sponza.obj"
