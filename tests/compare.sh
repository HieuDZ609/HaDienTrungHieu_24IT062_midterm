#!/bin/bash
# Compare ./myls against the system /bin/ls over a fixture tree.
# Usage: compare.sh <path-to-myls> <fixture-dir>
#
# Several divergences from the system ls(1) are deliberate, because the
# assignment is specified by the NetBSD ls(1) manual page rather than by
# the system's coreutils:
#
#   * the default block unit is 512 bytes, not 1024 (see the -s entry of
#     the manual), so BLOCKSIZE is pinned below to make the "total" lines
#     comparable;
#   * the "total" line of -s is only printed when stdout is a terminal
#     ("If the output is to a terminal, a total sum ... is output on a line
#     before the listing"), while GNU ls always prints it;
#   * the rightmost of -k and -h wins ("The rightmost of the -k and -h flags
#     overrides the previous flag"), while GNU ls lets -h survive -k.
#     Those cases are run through `expect_diverge` and reported separately.
set -u
MYLS=${1:?myls path}
FIX=${2:?fixture dir}
export LC_ALL=C TZ=UTC
export BLOCKSIZE=1024
# The listing is done from inside the fixture, so a path given relative to
# the caller's directory has to be made absolute first.
case $MYLS in /*) ;; *) MYLS=$PWD/$MYLS ;; esac
case $FIX in /*) ;; *) FIX=$PWD/$FIX ;; esac
cd "$FIX" || exit 1

# The manual reads "−A ... Always set for the super-user."  The system ls has
# no such rule of its own, so when this harness is run as root the reference
# is handed -A explicitly -- that is what it would print if it implemented
# the manual, and it keeps the two sides comparable.  As a normal user
# nothing is added and the reference runs exactly as shipped.
ref_opts=()
if [ "$(id -u)" = 0 ]; then
    ref_opts=(-A)
fi

# Reading a symlink or a directory refreshes its access time, so the very
# first listing of a freshly built fixture would see stamps that the second
# one does not.  One throwaway pass over the whole tree leaves them stable
# afterwards, and the -u cases then read identical values on both sides.
/bin/ls -lR . >/dev/null 2>&1 || true
"$MYLS" -lR . >/dev/null 2>&1 || true

# ...but some filesystems refresh access times on *every* read (tmpfs without
# relatime does), and then no two runs can ever agree on them.  Read one of
# the fixture's symlinks twice: the first read may legitimately settle the
# stamp, the second one must leave it alone.  If it does not, the -u cases
# are reported as skipped rather than as failures of the program under test.
atime_still=1
if [ -e link_ok ] || [ -L link_ok ]; then
    /bin/ls -l link_ok >/dev/null 2>&1 || true
    atime_before=$(stat -c '%.9X' -- link_ok 2>/dev/null || echo unknown)
    /bin/ls -l link_ok >/dev/null 2>&1 || true
    atime_after=$(stat -c '%.9X' -- link_ok 2>/dev/null || echo unknown)
    [ "$atime_before" = "$atime_after" ] || atime_still=0
fi

pass=0; fail=0; diverge=0; skip=0

# The program name comes from argv[0], so myls says "myls:" and ls says
# "ls:".  Normalise it before comparing; everything else must match.
compare_output() {
    local desc=$1 ref=$2 mine=$3
    if [ "$ref" = "$mine" ]; then
        pass=$((pass+1))
    else
        fail=$((fail+1))
        printf 'DIFF  %s\n' "$desc"
        diff <(printf '%s\n' "$ref") <(printf '%s\n' "$mine") |
            sed 's/^/      /'
    fi
}

check() {
    local desc=$1; shift
    local out_ref out_mine
    out_ref=$(/bin/ls ${ref_opts[@]+"${ref_opts[@]}"} "$@" 2>&1; echo "rc=$?")
    out_mine=$("$MYLS" "$@" 2>&1 | sed 's/^myls:/ls:/'; echo "rc=${PIPESTATUS[0]}")
    compare_output "$desc" "$out_ref" "$out_mine"
}

# A documented difference between the assignment's manual page and GNU ls:
# report it with its reason, but do not count it as a failure.
expect_diverge() {
    local why=$1 desc=$2; shift 2
    local out_ref out_mine
    out_ref=$(/bin/ls ${ref_opts[@]+"${ref_opts[@]}"} "$@" 2>&1; echo "rc=$?")
    out_mine=$("$MYLS" "$@" 2>&1 | sed 's/^myls:/ls:/'; echo "rc=${PIPESTATUS[0]}")
    if [ "$out_ref" = "$out_mine" ]; then
        pass=$((pass+1))
    else
        diverge=$((diverge+1))
        printf 'DIVERGE %s\n      (%s)\n' "$desc" "$why"
    fi
}

for opts in \
    "" "-a" "-A" "-l" "-la" "-lA" "-R" "-lR" "-i" \
    "-lF" "-F" "-n" "-ln" "-lt" "-lS" "-lr" "-S" "-t" "-r" \
    "-d" "-ld" "-lh" "-ls" "-lk" "-lsk" "-lsh" "-l -q" "-i -s -l" \
    "-R -a" "-RA" "-F -a" "-lR -S" "-ltr" "-ln -h" "-ltu" "-lc" "-lu" \
    "-f" "-f -a" "-f -A" "-A -f" "-fr" "-lf" "-S -r" "-t -r" \
    "-lkh" "-lks" "-lh" "-q" \
    "-l link_ok" "-l dirlink" "-lR dirlink" "-F dirlink" "-F link_ok" \
    "-i link_ok" "-i dirlink" "-l link_dangling" "-li dirlink"
do
    if [ "$atime_still" = 0 ] && { [ "$opts" = "-lu" ] || [ "$opts" = "-ltu" ]; }; then
        skip=$((skip+1))
        printf 'SKIP ls %s\n      (access times do not stand still on this filesystem)\n' "$opts"
        continue
    fi
    # shellcheck disable=SC2086
    check "ls $opts" $opts .
done

tty_total='the -s total is printed only on a terminal; GNU always prints it'
rightmost='-k on the right cancels -h, so sizes stay plain; GNU still humanises'
tietie='-t breaks equal timestamps by name; GNU keeps readdir order'
expect_diverge "$tty_total" "ls -s" -s .
expect_diverge "$tty_total" "ls -is" -is .
expect_diverge "$tty_total" "ls -skh" -skh .
expect_diverge "$tty_total; $rightmost" "ls -shk" -shk .
expect_diverge "$rightmost" "ls -lhk" -lhk .
expect_diverge "$tty_total" "ls -s link_ok" -s link_ok
w_raw='the manual makes -w force raw output; the reference takes a width argument'
expect_diverge "$w_raw" "ls -w" -w .
expect_diverge "$w_raw" "ls -q -w" -q -w .

# Operands: files, directories, missing paths, multiple mixed operands.
check "file operand" file.txt
check "dir operand" sub
check "missing operand" no-such-entry
check "mixed operands" file.txt sub dirlink
check "dangling symlink operand" link_dangling
check "symlink operand" link_ok
check "many operands" exec.sh sub file.txt noperm
check "dot operand" .
check "two dirs" sub empty
check "-d on dir" -d sub
check "-d multiple" -d . sub
check "-R on dir" -R sub
check "-R multiple" -R . sub
check "-aR" -aR sub
check "-t operands" -t -d file.txt exec.sh
check "-S operands" -S -d file.txt exec.sh
check "-R unreadable" -R nopermdir

# Deliberate ties.  Both implementations are stable, so -S on two files of
# the same size keeps readdir order.  -t is where the manual and GNU part
# company: the manual breaks the tie lexicographically, GNU keeps readdir
# order.
tie=$(mktemp -d)
trap 'rm -rf "$tie"' EXIT
printf 'x' > "$tie/zeta"
printf 'y' > "$tie/alpha"
touch -d '2026-01-01 12:00:00' "$tie/zeta" "$tie/alpha"
check "ls -S (equal sizes)" -S "$tie"
expect_diverge "$tietie" "ls -ltc" -ltc .
expect_diverge "$tietie" "ls -t (equal mtimes)" -t "$tie"

printf '\n%d passed, %d failed, %d documented divergences' \
    "$pass" "$fail" "$diverge"
[ "$skip" -gt 0 ] && printf ', %d skipped' "$skip"
printf '\n'
exit $((fail > 0))
