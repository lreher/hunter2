# deploy

Put hunter2 online at hunter2.lucasreher.com.

## State
- Host: Cloudflare Worker `hunter2`, static assets only, custom domain hunter2.lucasreher.com (set in wrangler.jsonc routes), workers.dev off.
- `npm run deploy` runs ./deploy.sh: refuses unless on main, clean, and equal to origin/main. Then ./build.sh, fills dist/ (gitignored) from `git archive HEAD:web` plus web/game.wasm (minus web/package.json), runs `wrangler deploy --message "<hash> <subject>"`, removes dist/. Extra args go to wrangler: `npm run deploy -- --dry-run` lists the files without uploading.
- wrangler.jsonc is committed: assets.directory = dist. Having it also stops wrangler's autoconfig from rewriting the repo.
- wrangler 4.156.0, pinned dev dependency (~220 MB node_modules). Log in once per machine: `npx wrangler login`.

## Decisions
- Cloudflare static hosting, not the playlister droplet (user, 2026-10-31). hunter2 is static files only: no server process, off the droplet (which hit OOM with playlister), served from the edge, free HTTPS on the lucasreher.com zone, atomic deploys with rollback. Playlister ruled Pages out only for its SQLite file.
- Workers static assets, not classic Pages (user, 2026-10-31): wrangler 4.156 folds Pages into Workers, classic Pages only via an opt-out flag and looks phased out.
- Build locally, upload. Cloudflare's build image v3 has no clang or lld (checked the docs 2026-10-31), so no git integration builds.
- Deploy only a clean, pushed main (user, 2026-10-31): the live site always matches a commit on GitHub.
- Upload only git tracked web/ files: web/photo.png (gitignored, may be a real person) never leaves the machine.
- Repo is public (user).

## Open
- Windows: not checked in Chrome there yet.
- Windows: needs npm install and npx wrangler login before its first deploy.
- Windows: deploy.sh is sh, run it from the same shell that runs ./build.sh there.

## Log
- 2026-10-31: focus created. Looked at playlister's deploy (droplet + Cloudflare Tunnel + ssh deploy script), chose Pages instead. Added wrangler, deploy.sh, npm run deploy, gitignore for node_modules/ and .wrangler/.
- 2026-10-31: first deploy went wrong. wrangler 4.156 folds Pages into Workers: `wrangler pages project create` ran Workers autoconfig, which wrote wrangler.jsonc (assets: web), replaced the npm deploy script with `wrangler deploy`, loosened the pin to ^, added .gitignore lines, and deployed a Worker with ALL of web/. `npm run deploy` then ran the rewritten script, so deploy.sh never ran. web/photo.png was public at hunter2.lucasreher.workers.dev for ~2 min, then the Worker was deleted (photo 404 confirmed). Local edits reverted. Lesson: never point any wrangler config at web/; check each wrangler command's output before the next.
- 2026-10-31: Worker setup committed (02c092f) and deployed. Live at https://hunter2.lucasreher.com: 7 files, game.wasm application/wasm, photo.png and package.json 404, workers.dev 404. Cache: public, max-age=0, must-revalidate plus ETag, so browsers revalidate every load and no _headers file is needed.
- 2026-10-31: user checked the live site in Chrome on the Mac: working. Handover written to handover/deploy.md.
