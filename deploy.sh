#!/bin/sh
# Builds and uploads the site to Cloudflare (Worker hunter2, static files only, hunter2.lucasreher.com).
# Only deploys a clean main that matches origin/main, so the live site is always a commit on GitHub.
# Log in once per machine first: npx wrangler login. Extra args go to wrangler, e.g. ./deploy.sh --dry-run
set -e
cd "$(dirname "$0")"

fail() { echo "deploy: $1" >&2; exit 1; }

[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || fail "not on main"
[ -z "$(git status --porcelain)" ] || fail "uncommitted changes, commit first"
git fetch -q origin main
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || fail "main differs from origin/main, push or pull first"

./build.sh

# dist/ (wrangler.jsonc assets) gets only what git tracks in web/ plus the built wasm, so web/photo.png stays out
rm -rf dist
mkdir dist
trap 'rm -rf dist' EXIT
git archive HEAD:web | tar -x -C dist
cp web/game.wasm dist/
rm dist/package.json # only for node, not part of the site

npx wrangler deploy --message "$(git log -1 --format='%h %s')" "$@"
