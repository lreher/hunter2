Read brain.md first.

Task: hunter2 online at https://hunter2.lucasreher.com. Done and working (checked in Chrome on the Mac, 2026-10-31). Read focus/deploy.md for the setup, decisions and the first deploy incident.

How it works: a Cloudflare Worker with static assets only (wrangler.jsonc, no Worker script). npm run deploy runs ./deploy.sh: refuses unless on a clean main equal to origin/main, then ./build.sh, fills dist/ from git (git archive HEAD:web plus web/game.wasm, minus web/package.json), wrangler deploy, removes dist/. npm run deploy -- --dry-run lists the files without uploading. Deploy only when I ask.

Hard rule: web/photo.png (gitignored, may be a real person) must never be uploaded. Never point wrangler at web/, and never run wrangler commands other than through deploy.sh without checking what they do first: wrangler 4.156 auto configures and deploys on its own (that is how the photo leaked for ~2 min on the first try). Check each wrangler command's output before running the next.

Setup per machine: npm install, then npx wrangler login (opens the browser).

Open: Windows not set up or checked yet (deploy.sh is sh, run it from the shell that runs ./build.sh there).
