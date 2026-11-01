# deploy

Put hunter2 online at hunter2.lucasreher.com.

## State
- Host: Cloudflare Pages, project `hunter2`, production branch main. Not created yet.
- `npm run deploy` runs ./deploy.sh: refuses unless on main, clean, and equal to origin/main. Then ./build.sh, stages `git archive HEAD web` plus web/game.wasm (minus web/package.json), and runs `wrangler pages deploy` with the commit hash and message.
- wrangler 4.156.0, pinned dev dependency (~220 MB node_modules). Log in once per machine: `npx wrangler login`.

## Decisions
- Cloudflare Pages, not the playlister droplet (user, 2026-10-31). hunter2 is static files only: no server process, off the droplet (which hit OOM with playlister), served from the edge, free HTTPS on the lucasreher.com zone, atomic deploys with rollback. Playlister ruled Pages out only for its SQLite file.
- Build locally, direct upload. Pages build image v3 has no clang or lld (checked the docs 2026-10-31), so no git integration builds. Note: a direct upload project cannot switch to git integration later.
- Deploy only a clean, pushed main (user, 2026-10-31): the live site always matches a commit on GitHub.
- Upload only git tracked web/ files: web/photo.png (gitignored, may be a real person) never leaves the machine.
- Repo is public (user).

## Open
- First deploy: wrangler login, create the project, deploy, add custom domain hunter2.lucasreher.com in the dashboard (Pages > hunter2 > Custom domains).
- After the first deploy, check headers with curl -I: game.wasm served as application/wasm, cache headers sane (Pages default should revalidate). Add web/_headers only if needed.
- Windows: deploy.sh is sh, run it from the same shell that runs ./build.sh there.

## Log
- 2026-10-31: focus created. Looked at playlister's deploy (droplet + Cloudflare Tunnel + ssh deploy script), chose Pages instead. Added wrangler, deploy.sh, npm run deploy, gitignore for node_modules/ and .wrangler/.
