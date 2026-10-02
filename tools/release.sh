#!/bin/sh
# Tags a release: tools/release.sh 0.1.0-beta.1
# Checks that the tree is clean, on master and in sync with origin, then
# creates the annotated tag vVERSION and pushes it, which starts the
# release workflow (.github/workflows/release.yml).
set -eu

version=${1:?usage: tools/release.sh <version>  (e.g. 0.1.0-beta.1)}
case "$version" in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "version must look like 0.1.0 or 0.1.0-beta.1" >&2; exit 1 ;;
esac
tag=v$version

[ "$(git rev-parse --abbrev-ref HEAD)" = master ] \
    || { echo "not on master" >&2; exit 1; }
[ -z "$(git status --porcelain)" ] \
    || { echo "the working tree is not clean" >&2; exit 1; }
git fetch origin master --tags
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/master)" ] \
    || { echo "master differs from origin/master" >&2; exit 1; }
git rev-parse -q --verify "refs/tags/$tag" >/dev/null \
    && { echo "$tag already exists" >&2; exit 1; }

make >/dev/null
echo "Tagging $tag at $(git rev-parse --short HEAD):"
git log --oneline -5
printf "Create and push %s? [y/N] " "$tag"
read -r answer
[ "$answer" = y ] || { echo aborted; exit 1; }

git tag -a "$tag" -m "Darklands $version"
git push origin "$tag"
echo "Pushed $tag: follow the 'release' workflow on GitHub."
