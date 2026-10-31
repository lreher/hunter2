// Measures how far balls move per frame (pile jitter).
import { start, frame } from "./fake.mjs";
await start();

let prev = null;
for (let f = 1; f <= 1500; f++) {
  const now = frame().map((b) => [b.x, b.y, b.z]);
  if (prev && [200, 400, 800, 1200, 1500].includes(f)) {
    let moved = 0, max = 0;
    now.forEach((p, i) => { const d = Math.hypot(p[0] - prev[i][0], p[1] - prev[i][1], p[2] - prev[i][2]); moved += d; max = Math.max(max, d); });
    console.log(`frame ${f}: mean move ${(moved / now.length * 1000).toFixed(3)} mm/frame, max ${(max * 1000).toFixed(3)} mm/frame`);
  }
  prev = now;
}
