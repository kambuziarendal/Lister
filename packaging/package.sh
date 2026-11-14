#!/bin/sh
set -eu
[ "$(uname -s)" = Haiku ] || { echo 'Build and package inside Haiku.' >&2; exit 1; }
version=${1:?version required}
metadata_version=$(awk '$1 == "version" { print $2 }' PackageInfo)
[ "$metadata_version" = "$version-1" ] || { echo 'Package version mismatch.' >&2; exit 1; }
set -- src/objects.*-release/Listi
[ "$#" = 1 ] && [ -x "$1" ] || { echo 'Expected one linked release binary.' >&2; exit 1; }
mkdir -p build dist
stage=$(mktemp -d build/package.XXXXXX)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
mkdir -p "$stage/apps" "$stage/data/licenses" "$stage/data/deskbar/menu/Applications" "$stage/documentation/packages/lister"
cp "$1" "$stage/apps/Listi"
cp PackageInfo "$stage/.PackageInfo"
cp LICENSE "$stage/data/licenses/MIT"
cp README.md docs/*.md "$stage/documentation/packages/lister/"
ln -s /boot/system/apps/Listi "$stage/data/deskbar/menu/Applications/Listi"
# Normalize mtimes. Haiku package also records filesystem atime/creation time,
# so package-container bytes are not guaranteed identical across rebuilds.
export SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || echo 1794600000)}
python3 - "$stage" <<'PY'
import os,sys
root=sys.argv[1];stamp=int(os.environ['SOURCE_DATE_EPOCH'])
for base,dirs,files in os.walk(root,topdown=False):
 for name in files+dirs:os.utime(os.path.join(base,name),(stamp,stamp),follow_symlinks=False)
os.utime(root,(stamp,stamp))
PY
package create -C "$stage" "dist/Listi-$version-1-x86_64.hpkg"
