#!/bin/sh
# Builds and uploads web/ to Cloudflare Pages (project hunter2, hunter2.lucasreher.com).
# Only deploys a clean main that matches origin/main, so the live site is always a commit on GitHub.
# Log in once per machine first: npx wrangler login
set -e
cd "$(dirname "$0")"

fail() { echo "deploy: $1" >&2; exit 1; }

[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || fail "not on main"
[ -z "$(git status --porcelain)" ] || fail "uncommitted changes, commit first"
git fetch -q origin main
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || fail "main differs from origin/main, push or pull first"

./build.sh

# upload only what git tracks in web/ plus the built wasm (keeps web/photo.png out)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
git archive HEAD web | tar -x -C "$out"
cp web/game.wasm "$out/web/"
rm "$out/web/package.json" # only for node, not part of the site

npx wrangler pages deploy "$out/web" --project-name hunter2 --branch main \
  --commit-hash "$(git rev-parse HEAD)" --commit-message "$(git log -1 --format=%s)" --commit-dirty=false
