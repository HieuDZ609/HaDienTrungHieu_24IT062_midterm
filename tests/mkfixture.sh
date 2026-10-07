#!/bin/sh
# Build the fixture tree that tests/compare.sh expects.
#
#     tests/mkfixture.sh <dir>
#
# Needs no privileges; the character and block device nodes are created only
# when the caller is allowed to mknod(), which is why they are optional.
set -eu

D=${1:?usage: mkfixture.sh <dir>}
rm -rf -- "$D"
mkdir -p -- "$D"
cd -- "$D"

mkdir empty sub sub/inner
printf 'hello\n' > file.txt
printf 'hidden\n' > .hidden
printf 'deep\n'  > sub/inner/deep.txt
printf 'one\n'   > sub/one.txt
truncate -s 3072000 big.bin

printf '#!/bin/sh\n' > exec.sh
chmod 755 exec.sh

: > noperm
chmod 000 noperm
mkdir nopermdir
chmod 000 nopermdir

: > sticky
chmod 1777 sticky
: > setuid1
chmod 4755 setuid1

mkfifo fifo1
ln -s file.txt link_ok
ln -s /nowhere link_dangling
ln -s sub dirlink
mkdir 'dir with space'
printf 'a\n' > "new
line"
printf 'b\n' > 'tab	name'

if command -v python3 >/dev/null 2>&1; then
    python3 - "$PWD/sock1" <<'PY'
import socket, sys
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind(sys.argv[1])
PY
fi

mknod null  c 1 3 2>/dev/null || true
mknod loop0 b 7 0 2>/dev/null || true

# Give every entry a distinct timestamp, deliberately not in name order, so
# that the -t cases exercise the comparator instead of the granularity of the
# filesystem clock (which can be as coarse as a millisecond).
T=2026-01-01
stamp() { touch -h -d "$T $1" -- "$2"; }
stamp 12:00:01 .hidden
stamp 12:00:02 'dir with space'
stamp 12:00:03 big.bin
stamp 12:00:04 exec.sh
stamp 12:00:05 fifo1
stamp 12:00:06 file.txt
stamp 12:00:07 link_dangling
stamp 12:00:08 link_ok
stamp 12:00:09 "new
line"
stamp 12:00:10 noperm
stamp 12:00:11 nopermdir
stamp 12:00:12 setuid1
stamp 12:00:13 sock1
stamp 12:00:14 sticky
stamp 12:00:15 sub
stamp 12:00:16 'tab	name'
stamp 12:00:17 dirlink
stamp 12:00:18 empty
stamp 12:00:19 sub/one.txt
stamp 12:00:20 sub/inner
stamp 12:00:21 sub/inner/deep.txt
if [ -e null ];  then stamp 12:00:22 null;  fi
if [ -e loop0 ]; then stamp 12:00:23 loop0; fi

# Access times need the opposite treatment from the stamp above: on this
# kernel an atime written into the past is rewritten to "now" a couple of
# seconds later, which would hand every entry the same value and turn -u
# into a tie-break comparison instead of a comparison of the sort itself.
# Times just ahead of the clock are left alone, and they stay ahead of both
# the modification and the change time, so relatime never rewrites them.
now=$(date +%s)
i=0
for f in * .hidden sub/one.txt sub/inner sub/inner/deep.txt; do
    [ -e "$f" ] || [ -L "$f" ] || continue
    touch -a -h -d "@$((now + 600 + i))" -- "$f" || true
    i=$((i + 1))
done
