#!/usr/bin/env bash
# tag-release.sh [--dry-run] [--skip-benchmark] <major.minor.patch> — release simdfix at the current main.
#
#   .github/tag-release.sh 0.2.0
#
# Checks that main is clean, pushed, and green in CI, that CHANGELOG.md has
# Unreleased notes, and that no benchmark is more than 3% slower than in the
# previous release (bench-compare.sh, on this machine: GitHub's shared runners are
# too noisy for it). Then writes the number to VERSION, turns the Unreleased notes
# into the release's section, commits just those two files as "Release <version>",
# pushes main, and tags and pushes v<version>. The tag starts the release workflow.
#
# --dry-run         runs the checks and changes nothing
# --skip-benchmark  leaves out the benchmark comparison, e.g. on a busy machine

set -euo pipefail

DRY_RUN=false
BENCHMARK=true
RELEASE=""
for argument in "$@"; do
    case "$argument" in
        --dry-run) DRY_RUN=true ;;
        --skip-benchmark) BENCHMARK=false ;;
        *) RELEASE="$argument" ;;
    esac
done

fail() {
    echo "tag-release: $*" >&2
    exit 1
}

[[ "$RELEASE" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "usage: $0 [--dry-run] [--skip-benchmark] <major.minor.patch>"

cd "$(git rev-parse --show-toplevel)"
TAG="v$RELEASE"
CURRENT="$(cat VERSION)"

# Only main is released, exactly as CI tested it.
[[ "$(git rev-parse --abbrev-ref HEAD)" == "main" ]] || fail "not on main"
[[ -z "$(git status --porcelain)" ]] || fail "the working tree has changes"
git fetch --quiet origin main --tags
[[ "$(git rev-parse HEAD)" == "$(git rev-parse origin/main)" ]] || fail "main differs from origin/main: push or pull first"
if git rev-parse --quiet --verify "refs/tags/$TAG" >/dev/null; then
    fail "$TAG already exists"
fi

# The new version must come after the current one.
if [[ "$RELEASE" == "$CURRENT" || "$(printf '%s\n%s\n' "$CURRENT" "$RELEASE" | sort -V | tail -n 1)" != "$RELEASE" ]]; then
    fail "$RELEASE does not come after VERSION $CURRENT"
fi

# The Unreleased section becomes the release notes, so it must say something.
NOTES="$(awk '/^## \[Unreleased\]/ { found = 1; next } found && /^## \[/ { exit } found' CHANGELOG.md | grep -v '^[[:space:]]*$' || true)"
[[ -n "$NOTES" ]] || fail "CHANGELOG.md has no Unreleased notes"

# CI must have passed on this very commit.
command -v gh >/dev/null || fail "the GitHub CLI (gh) is needed to check CI"
CI="$(gh run list --workflow ci.yml --commit "$(git rev-parse HEAD)" --json status,conclusion \
        --jq '.[0] | if . == null then "not run" else "\(.status) \(.conclusion)" end')"
[[ "$CI" == "completed success" ]] || fail "CI on $(git rev-parse --short HEAD) is '$CI', not 'completed success'"

# Decoding speed against the previous release, on this machine.
PREVIOUS="$(git describe --tags --abbrev=0 --match 'v*' HEAD 2>/dev/null || true)"
if ! $BENCHMARK; then
    echo "tag-release: skipping the benchmark comparison"
elif [[ -z "$PREVIOUS" ]]; then
    echo "tag-release: no previous release to compare benchmarks with"
else
    .github/bench-compare.sh "$PREVIOUS" ||
        fail "a benchmark is slower than in $PREVIOUS; if the machine was busy, run again or pass --skip-benchmark"
fi

if $DRY_RUN; then
    echo "tag-release: ready to release $RELEASE (dry run, nothing changed)"
    exit 0
fi

echo "$RELEASE" > VERSION
awk -v heading="## [$RELEASE] - $(date +%Y-%m-%d)" '
    /^## \[Unreleased\]/ && !done { print; print ""; print heading; done = 1; next }
    { print }
' CHANGELOG.md > CHANGELOG.md.new
mv CHANGELOG.md.new CHANGELOG.md

git commit --quiet -m "Release $RELEASE" -- VERSION CHANGELOG.md
git push origin main
git tag -a "$TAG" -m "simdfix $RELEASE"
git push origin "$TAG"
echo "tag-release: pushed $TAG; follow the release workflow with: gh run watch"
