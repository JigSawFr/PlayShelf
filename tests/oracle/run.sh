#!/bin/sh
# Reads the NCAs the recompressor makes with LibHac (MIT; no code shared with
# PlayShelf) and checks the RomFS section comes out as the original bytes.
# Needs g++, liblz4-dev and the .NET 8 SDK (apt install dotnet-sdk-8.0).
# Usage: tests/oracle/run.sh [work folder]
set -eu
cd "$(dirname "$0")/../.."
work="${1:-build/oracle}"
mkdir -p "$work"
g++ -std=c++23 -Isource -Isource/kit -Itests -Wall -Wextra \
    source/kit/util/aes.cpp source/install/rsa.cpp source/install/pfs.cpp source/install/cnmt.cpp source/install/nca.cpp \
    source/kit/util/sha256.cpp source/install/compressed_storage.cpp source/install/ivfc.cpp source/install/bktr.cpp \
    source/install/recompress.cpp tests/oracle/gen.cpp -llz4 -o "$work/gen"
"$work/gen" "$work/cases"
dotnet build -c Release -v q -o "$work/ncadump" tests/oracle/ncadump | tail -n 3
fail=0
for nca in "$work"/cases/*.nca; do
    name=$(basename "$nca" .nca)
    want=$(grep '^sha256=' "$work/cases/$name.expected" | cut -d= -f2)
    got=$(dotnet "$work/ncadump/ncadump.dll" "$nca" "$work/cases/prod.keys" 1 | grep '"sha256"' | sed 's/.*: "\(.*\)".*/\1/')
    if [ "$want" = "$got" ]; then echo "$name: ok"; else echo "$name: FAIL (want $want, got $got)"; fail=1; fi
done

# The oracle must be able to fail: a byte changed inside the RomFS section's stored data is refused or read back different.
cp "$work/cases/system.nca" "$work/tampered.nca"
size=$(wc -c < "$work/tampered.nca")
printf '\377' | dd of="$work/tampered.nca" bs=1 seek=$((size / 2)) conv=notrunc 2>/dev/null
want=$(grep '^sha256=' "$work/cases/system.expected" | cut -d= -f2)
if got=$(dotnet "$work/ncadump/ncadump.dll" "$work/tampered.nca" "$work/cases/prod.keys" 1 2>/dev/null | grep '"sha256"' | sed 's/.*: "\(.*\)".*/\1/') && [ "$got" = "$want" ]; then
    echo "tampered: FAIL (the change went unseen)"; fail=1
else
    echo "tampered: refused, as it must be"
fi
exit $fail
