# Releasing

Releases are made by pushing a git tag; GitHub Actions
(`.github/workflows/release.yml`) does the rest.

## Versioning

`MAJOR.MINOR.PATCH`, with a pre-release suffix while the game is
incomplete: `0.1.0-beta.1`, `0.1.0-beta.2`, ... then `0.1.0`. A tag with a
`-` is published as a **pre-release**. The tag is the version with a `v`
in front (`v0.1.0-beta.1`).

The version is not stored in any file: `make` takes it from
`git describe` (so a build between tags says e.g. `v0.1.0-beta.1-12-gc22317f`),
or from `make VERSION=x.y.z` when there is no git checkout.
`darklands --version` prints it.

## Making a release

1. Make sure `master` has what you want and CI (`build`) is green.
2. Update the README's status and roadmap if needed.
3. From a clean, up-to-date `master`:

   ```sh
   tools/release.sh 0.1.0-beta.1
   ```

   It checks the tree, builds, asks for confirmation, creates the annotated
   tag and pushes it.
4. The `release` workflow builds on Ubuntu 22.04 (an old glibc, so the
   binary runs on newer systems) and publishes the GitHub Release with:
   - `darklands-<version>-linux-x86_64.tar.gz`: the program, README, docs;
   - `darklands-<version>-src.tar.gz`: the sources *with* libjgame (GitHub's
     automatic archives leave submodules out);
   - `SHA256SUMS`;
   - release notes generated from the merged pull requests / commits.
5. Open the release page, read the notes, edit them if needed.

To try the packaging without releasing, run the workflow by hand
(Actions > release > Run workflow): it builds and attaches the packages
as a workflow artifact, and publishes nothing. Locally: `make dist`.

## Mistakes

A bad release: delete the release and the tag on GitHub
(`git push --delete origin vX.Y.Z`, `git tag -d vX.Y.Z`), fix, and tag again
(or use the next beta number: better, if anybody may have downloaded it).

## What a package contains

The game's data files are never included (copyright): users supply their
own, see the README. The Linux binary links SDL2 dynamically, so users
need `libsdl2` installed (it is in every distribution).

## Other platforms

Only Linux x86_64 is built for now. A new platform is a new entry in the
`build` job (a matrix over `runs-on`); `make dist` already names the
package after `uname`. The `publish` job attaches whatever `build` uploads.

## Before the first public release

The repository has no `LICENSE` file yet: add one (`make dist` includes
`LICENSE*` when present) before publishing.
