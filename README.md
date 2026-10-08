# myls — a simplified UNIX `ls(1)`

Midterm implementation of the `ls(1)` utility from the NetBSD manual page
supplied with the assignment, written from scratch in C11 with only POSIX
interfaces.

```
make            # builds ./myls
./myls -lRhwA .
tests/compare.sh ./myls <fixture>   # differential test against /bin/ls
```

## Requirements

* the options `AacdFfhiklnqRrSstuw` — exactly the set listed in the SYNOPSIS of
  the manual, no more and no less;
* all of the behaviour described under **DESCRIPTION**, **The Long Format**,
  **ENVIRONMENT** and **EXIT STATUS**;
* GNU `ls` is used as a reference only where the manual is silent.

## Source layout

| file | responsibility |
| --- | --- |
| `ls_types.h` | shared enums, entry/list structures, `LS_EXIT_FAILURE` |
| `options.[ch]` | option parsing, the manual's override rules, usage text |
| `listing.[ch]` | `readdir`/`lstat` of a directory, path joining, diagnostics |
| `statinfo.[ch]` | mode string, device numbers, block counts, humanised sizes |
| `sort.[ch]` | comparison functions and `ls_sort_entries()` |
| `format.[ch]` | column layout, the long format, the `total` line |
| `main.c` | operand handling, the depth-first `-R` walk, exit status |

Build with `-std=c11 -Wall -Wextra -Werror -O2 -g -MMD -MP` plus
`_DEFAULT_SOURCE`/`_POSIX_C_SOURCE=200809L` so that `lstat`, `st_rdev` and
`S_ISWHT`-style macros are visible.  Nothing outside libc is used.

## Options

Each option of the SYNOPSIS is handled in `ls_parse_args()` (`options.c`).

* `-A` / `-a` — dot-file visibility; `-A` implies "everything but `.` and
  `..`", `-a` also adds those two.
* `-c` / `-u` — select ctime or atime; they override each other, the last one
  wins, and the selected timestamp drives both `-t` and the `-l` date.
* `-d` / `-R` — directory handling; the last one wins (override order matters,
  so they are stored as a single `list_mode`).
* `-l` / `-n` — long format; the last one wins, `-n` prints numeric uid/gid.
* `-f` — no sorting; also implies `-a`, because the manual's `-f` entry says
  only "Output is not sorted" and an unsorted list would otherwise hide the
  dot entries a caller of `-f` expects to see.
* `-F` — type suffixes `/ @ % = | *` after the name.
* `-h` / `-k` — size units; **the rightmost of the two wins** (see below).
* `-i` — inode number, in the same field order as the manual's
  "information associated with the −i, −s, and −l options".
* `-q` / `-w` — non-printable characters as `?` or raw; the last one wins,
  with the manual's defaults (`-q` on a terminal, `-w` otherwise).
* `-r` — reverse the result.
* `-s` — block count column, and a `total` line on a terminal.
* `-S` — sort by size, largest first.
* `-t` — sort by the selected time, newest first.
* `-n`, `-R`, `-r`, `-S`, `-s`, `-t`, `-u`, `-w` as above.

## Decisions taken where the manual is silent or contested

The rule used throughout: **follow the manual whenever it speaks, and follow
GNU `ls` only where the manual says nothing.**  Every place where the result
differs from the system `ls` is listed at the end of this file.

### Default block unit is 512 bytes

`−s ... in units of 512 bytes or BLOCKSIZE` and the same sentence in the
long-format section.  NetBSD's `getbsize()` returns 512 when `BLOCKSIZE` is
unset, so the manual is describing real behaviour.  GNU defaults to 1024 and
is the outlier here.  `BLOCKSIZE` still overrides the default, and `-h`/`-k`
suppress it, as ENVIRONMENT requires.

### The `total` line of `-s` is printed only on a terminal

> If the output is to a terminal, a total sum for all the file sizes is
> output on a line before the listing.

NetBSD's `printtotal()` is called by `printlong`, `printcol` and `printacol`
but never by `printscol`, and `printscol` is the single-column path that is
used when stdout is not a terminal.  So the terminal condition in the manual
is real, and it is GNU (which always prints the total) that disagrees.  The
`-l` total is unaffected: it is unconditional, as the long-format section
says.

### Columns on a terminal

The manual never mentions a columnar layout, but it does distinguish output
to a terminal from output that is redirected, and every `ls(1) ever written
fills the terminal horizontally when it can.  So a short listing written to a
terminal is laid out in columns (`ls_print_columns()` in `format.c`), and a
short listing written anywhere else keeps the plain one-entry-per-line form
the harness compares.

The layout is the classic one: entries are filled top-down column by column,
every column is as wide as the widest entry it holds, two spaces separate the
columns, and the last cell of a row is never padded.  Terminal width comes
from `ioctl(TIOCGWINSZ)`, then `$COLUMNS`, then 80.  `-i`, `-s` and `-F` take
part in the width, `-l` never uses columns, and an entry wider than the
screen falls back to one entry per line — which is also what the system `ls`
does.  This is one more case where the layout of the reference implementation
was measured rather than derived, so it follows `ls` and not NetBSD's
`printcol()`.

### `-k` on the right cancels `-h`

> The rightmost of the −k and −h flags overrides the previous flag.

NetBSD's option loop does exactly that — `case 'k'` sets `kflag = 1` and
`f_humanize = 0`, `case 'h'` does the reverse.  GNU `ls` never lets `-k`
switch `-h` off, so `ls -lhk` humanises there while it prints bytes here.
`-k` also forces the `-s` column *and* the `-l` total into 1024-byte units,
because NetBSD keeps a single `blocksize` for both.

### Sizes are rounded up with integers

`humanize_number()` rounds `3072000` up to `3.0M` rather than down to
`2.9M`, and reports `1048575` as `1024K`.  Reproducing that with plain
integer arithmetic avoids floating point entirely:

```
unit   = 1024^u  for the largest u with 1024^(u+1) <= size
quot   = size / unit, rem = size % unit
quot >= 10   -> quot + (rem != 0)
quot <  10   -> one decimal digit: (quot*10 + (rem*10 + unit - 1) / unit)
```

The units are `B K M G T P E`; the result never overflows because
`rem * 10 + unit - 1 < 2^64` for every unit up to `E`.  `-h` humanises the
`total` line as well, and `-lh` prints the size field humanised while the
`-s` column keeps the block count.

### Sorting and `-r`

* `-t` breaks equal timestamps by name, which is what NetBSD's own `modcmp()`
  does — it returns `namecmp(...)` when the two timestamps compare equal.
  GNU `ls` leaves tied entries in `readdir` order instead.
* `-S` has no tie-break at all: the sort is stable, so equal-sized files keep
  the order `readdir` produced.  The manual is silent on `-S` ties.
* Operands (file and directory arguments) go through the *same* comparator as
  directory entries, so `-t` and `-S` order them too.
* `-r` reverses the finished list rather than negating the comparator, so it
  also reverses an unsorted (`-f`) listing, and so that a descending sort
  never becomes a *stable ascending* sort.

Both `-t` and `-S` fall back to a name comparison only where NetBSD's own
`cmp.c` does; size comparisons fall back to nothing because GNU would break
on any fixture with two files of equal size.

### The `-R` walk

`ls_show_directory()` reads one directory, prints it, and then walks the
listing **backwards**, `opendir`ing every sub-directory it finds before
anything descends.  Successful probes are pushed on the walk stack, so they
pop in listing order afterwards.  The consequences match GNU `ls` exactly:

* every "cannot open directory" diagnostic appears immediately after the
  parent listing, rather than half way through the children;
* the diagnostics appear in reverse listing order, as GNU's do;
* `.` and `..` are never queued, and a symbolic link to a directory is never
  probed — only a real directory can be entered.

### `-f` leaves `.` and `..` first

`-f` disables sorting, so the order is whatever `readdir` returns — and ext4
interleaves `.` and `..` with the other entries instead of yielding them
first.  The reference `ls -f` shows them at the top regardless, so
`ls_read_directory()` hoists them there after the read (`ls_hoist_dots()`,
listing.c) and leaves everything else in raw `readdir` order.

### Exit status

> The ls utility exits 0 on success, and >0 if an error occurs.

Both 1 and 2 satisfy that.  GNU `ls` distinguishes them through
`set_exit_status(serious)`: a directory named **on the command line** that
cannot be opened is serious (2), while one discovered half way through the
walk is a minor problem (1).  `myls` does the same, and an invalid option or
a missing operand is also 2, which is what `usage(LS_FAILURE)` gives GNU.

### Diagnostics name `argv[0]`

`ls_set_program_name()` takes the `basename()` of `argv[0]`, so a message
reads `myls: cannot open directory 'x'` however the binary was renamed.
This matches GNU's `program_invocation_short_name` and NetBSD's
`setprogname()`.

### The long format

Written to the letter of **The Long Format**: mode string with `d l s p b c
-` type characters (plus `S/T/t/s/x` permission slots), link count, owner,
group, size — replaced by `major, minor` for character and block devices —
then `%b %e %H:%M` timestamps (or `%b %e %Y` for entries older than six
months, as `ls` does), then `-> target` for symbolic links.  `-n` replaces
owner and group names with their numeric IDs.

## Building and testing

```sh
make          # ./myls
make clean
tests/compare.sh ./myls /path/to/fixture
```

`tests/compare.sh` runs 86 invocations of `myls` and of the system `ls`
side by side over a fixture tree that contains dot files, a nested
sub-directory, a dangling and a valid symbolic link, a directory link, an
empty directory, a directory with no permissions, a FIFO, a socket, a setuid
file, a sticky file, an executable, names containing spaces/newlines/tabs,
and a 3 MB file.  Both sides are pinned to `LC_ALL=C`, `TZ=UTC` and
`BLOCKSIZE=1024`, output is normalised for the program name, and the exit
status of every invocation is compared as well.

Current result: **0 failed**, out of 86 comparisons — 79 of them agree
outright on this machine, 77 on one whose change times tie (see below), and
the rest are reported as documented divergences.

Ten of those 86 are run through `expect_diverge` rather than `check`, because
they are places where the manual and the system `ls` disagree: eight of them
diverge on every run, and two only when the fixture happens to exercise the
difference.

Two fixture details matter for the `-u` cases:

* `tests/mkfixture.sh` stamps the access times just *ahead* of the clock.
  On Linux an atime written into the past is rewritten to "now" a couple of
  seconds later, which would hand every entry the same value and turn `-ltu`
  into a tie-break comparison instead of a comparison of the sort itself.
  A time ahead of the clock, and ahead of both the modification and change
  time, is left alone by `relatime`.
* the fixture therefore has to live on a filesystem that keeps atimes still
  — a relatime ext4 mount does, tmpfs without `relatime` does not.  The
  harness probes one of the fixture's symlinks twice before it starts and,
  if the second read still moves the stamp, reports `-lu` and `-ltu` as
  **skipped** rather than letting an artifact of the filesystem look like a
  failure of the program under test.

### Divergences from GNU `ls`, all deliberate

These eight show up on every run:

| case | reference | `myls` | why |
| --- | --- | --- | --- |
| `-s`, `-is`, `-skh`, `-s link_ok` piped | prints `total …` | omits it | manual: only "if the output is to a terminal" |
| `-shk` | humanises (`3.0M`) | plain (`3000`) | manual: the rightmost of `-k`/`-h` wins |
| `-lhk` | humanises (`3.0M`) | plain (`3000`) | manual: the rightmost of `-k`/`-h` wins |
| `-w`, `-q -w` | refuses (`invalid line width`, status 2) | prints the listing, status 0 | manual: `-w` forces raw output; the reference made `-w` take a width argument |

These are deliberate too, but appear only when the fixture actually
exercises them:

| case | reference | `myls` | why |
| --- | --- | --- | --- |
| `-t` on equal mtimes | `readdir` order | name order | NetBSD's `modcmp()` falls back to `namecmp()` |
| `-ltc` when change times tie | `readdir` order | name order | same `-t` rule, on the ctime field |
| no `BLOCKSIZE`, `-s` | 1024-byte units | 512-byte units | manual: "units of 512 bytes or BLOCKSIZE" |
| `-s` on a single non-directory | no `total` | no `total` | agrees; run through `expect_diverge` only because the manual's terminal rule could have been read the other way |

`compare.sh` pins `BLOCKSIZE=1024` for both sides, so the third row never
appears inside the harness.  The first two appear only when two timestamps
are equal at the resolution the filesystem stores them with: the fixture's
modifications times are distinct by construction, but change times cannot be
set at all and fall inside the same clock tick on some machines, and a
filesystem that interleaves `readdir` with the lexicographic order will make
even distinct times disagree.


One further difference exists only under **root**: the manual says
`−A … Always set for the super-user`, and `myls` obeys it (`options.c`
checks `geteuid()`), while uutils/GNU do not.  The harness is written so
that this stays testable: when it detects that it is running as root it
hands the *reference* `-A` explicitly, which is what the reference would
print if it implemented the manual, and both sides then list the same
entries.  As a normal user nothing is added.

Two more differences exist only when the output is going to a **terminal**,
which the harness never sees because it compares redirected output:

* the reference quotes names for a terminal (`'dir with space'`,
  `'new'$'\n''line'`), while the manual defines `-q` as replacing
  non-printable characters with `?` and calls that "the default when output
  is to a terminal".  `myls` follows the manual.  Piped, the two agree byte
  for byte; on a terminal they do not.
* quoting makes names wider, so a columnar listing can break differently:
  at 80 columns the reference may take five columns where `myls` takes six.
  The layout itself — column major, two spaces between columns, each column
  as wide as its widest entry — is the same in both.

The `ls` on modern distributions is often **uutils coreutils** rather than
GNU coreutils; `myls` is written against the assignment's manual page and
checks its own behaviour against whichever binary `/bin/ls` happens to be.
The places where the reference binary itself disagrees with the manual are
the ones listed above.

Every one of these is a point where the assignment's manual page states
something that the system `ls` does not do.


## Building and testing on NetBSD

Verified with a **NetBSD 10.2 (amd64)** installation, using the system
`gcc` 10.5.0 and `bmake` (installed as `/usr/bin/make`) — the same `make`
command builds on Linux and on NetBSD:

```sh
make            # './myls'
make clean
./myls /some/dir
```

The `tests/compare.sh` harness is written for Linux (it uses `bash`,
`stat -c` and `touch -d`, none of which NetBSD ships), so on the BSD side a
manual differential battery was run instead: `./myls` against `/bin/ls`
over a fixture tree of plain files, directories, symlinks (valid and
dangling), hard links, a FIFO and files with distinct sizes, timestamps and
permissions.  The two programs agreed **byte for byte** on the single-option
cases `a A d F f h i k q r s S t u w` and on `-ast`.

A few divergences remain against NetBSD's own `ls(1)`.  All of them are
cosmetic ordering or spacing, or combinations where `myls` follows the
assignment's manual — which usually lands on GNU's side whenever NetBSD's
`ls` and GNU's `ls` disagree:

| case | NetBSD `/bin/ls` | `myls` | why |
| --- | --- | --- | --- |
| `-l`, `-n` | padded columns (`  1 tester  users  5120 …`) | single-space columns | the manual does not fix the column widths; `myls` uses GNU's spacing |
| `-R` on a single directory | no leading `dir:` header | prints `dir:` | `myls` matches GNU's recursive header; NetBSD prints the header only for sub-directories |
| `-S` with equal sizes | sorts the ties by name | keeps directory order | the manual does not define the tie-break, and GNU does not either |
| `-f -A` / `-fr` | NetBSD's `-f` overrides `-A` (lists every entry, `-r` reverses the unfiltered list) | `-A` stays enabled, `-r` reverses the filtered list | `myls` matches GNU's combination semantics |
| `-1` | one column | rejected | NetBSD extension; the manual lists no `-1`, so it is outside the assignment's option set |

The `-t`/`-r` sort families and `-S` by size alone behaved exactly like the
system `ls`, and `-w` printed the raw unframed listing the manual demands.

Screenshots of the actual NetBSD runs (source fetched straight from this
repository's `main` branch, built with `bmake`/`gcc` 10.5.0, then run against
a fixture tree and compared with `/bin/ls`):

| build from the GitHub tree | long listing of the fixture | differential battery vs `/bin/ls` |
| --- | --- | --- |
| ![NetBSD build](screenshots/1_build_from_github.png) | ![NetBSD run](screenshots/2_run_against_fixture.png) | ![NetBSD battery](screenshots/3_diff_vs_bin_ls.png) |


## Repository

* https://github.com/HieuDZ609/HaDienTrungHieu_24IT062_midterm
