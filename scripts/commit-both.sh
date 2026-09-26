#!/bin/sh
# commit-both.sh - commit this project and its private sibling together.
#
# A project whose operating docs live in ../<project>_private/ is two
# repositories over one piece of work. Every operating-doc name is in this
# repository's .gitignore, so a change to one of them leaves `git status` here
# perfectly clean: forgetting to commit the private side produces no warning at
# all, and the two drift apart silently.
#
# This refuses that. Given a message it commits every side that has changes,
# with the same subject on each, so the pair can be found again from either log.
#
#   scripts/commit-both.sh -m "what changed"
#   scripts/commit-both.sh -F message-file
#   scripts/commit-both.sh                    just show both statuses
#
# It never pushes. The public side is pushed by hand once its gate is green; the
# private side has no remote on purpose.
#
# Harmless when the project keeps its operating docs in its own tree: with no
# private sibling repository there is nothing to pair with, and it says so.
set -eu

pub=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
priv="$(dirname "$pub")/$(basename "$pub")_private"

test -d "$pub/.git" || { echo "commit-both: $pub is not a git repository" >&2; exit 1; }

msg=""; msgfile=""
while test $# -gt 0; do
    case $1 in
        -m) msg=${2:-}; shift 2 ;;
        -F) msgfile=${2:-}; shift 2 ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "commit-both: unknown argument: $1" >&2; exit 2 ;;
    esac
done

if ! test -d "$priv/.git"; then
    echo "commit-both: no private sibling repository at $priv - nothing to pair with."
    echo "commit-both: commit this project the usual way."
    exit 0
fi

# A message on stdin (-F -) has to be captured first: it can only be read once,
# and this commits twice. Without this the second commit died on an empty
# message after the first had already been made - half the pair, which is the
# exact failure this script exists to prevent.
if test "$msgfile" = "-"; then
    _stdin_msg=$(mktemp) || { echo "commit-both: cannot create a temporary file" >&2; exit 1; }
    cat > "$_stdin_msg"
    msgfile=$_stdin_msg
    trap 'rm -f "$_stdin_msg"' EXIT INT TERM
fi

dirty() { test -n "$(git -C "$1" status --porcelain)"; }

echo "--- public  $pub"
git -C "$pub" status -s || true
echo "--- private $priv"
git -C "$priv" status -s || true
echo "---"

pub_dirty=0; priv_dirty=0
dirty "$pub"  && pub_dirty=1
dirty "$priv" && priv_dirty=1

if test "$pub_dirty" = 0 && test "$priv_dirty" = 0; then
    echo "commit-both: both sides are clean, nothing to do"
    exit 0
fi

if test -z "$msg" && test -z "$msgfile"; then
    echo "commit-both: the changes above are uncommitted. Re-run with -m or -F to commit them."
    exit 1
fi

commit_one() {
    repo=$1
    git -C "$repo" add -A
    if test -n "$msgfile"; then
        git -C "$repo" commit -q -F "$msgfile"
    else
        git -C "$repo" commit -q -m "$msg"
    fi
    echo "committed: $repo  $(git -C "$repo" log --oneline -1)"
}

test "$pub_dirty"  = 1 && commit_one "$pub"
test "$priv_dirty" = 1 && commit_one "$priv"

echo "commit-both: done. Push the public side when its gate is green; never push the private one."
