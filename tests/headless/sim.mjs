// Runs the real game headless and checks the balls: inside the box, no NaN, overlap. ASCII front and top views.
import { start, frame, draws } from "./fake.mjs";
const W = 16, H = 9, D = 9; // world box, matches game.c: x 0..W, y 0..H, z -D/2..D/2

await start();

function plot(label, balls) {
  const COLS = 40, ROWS = 18;
  const front = Array.from({ length: ROWS }, () => Array(COLS).fill("."));
  const top = Array.from({ length: ROWS }, () => Array(COLS).fill("."));
  let outside = 0, nan = 0, lowest = 1e9;
  for (const b of balls) {
    if (![b.x, b.y, b.z].every(Number.isFinite)) { nan++; continue; }
    const e = 0.001;
    if (b.x - b.r < -e || b.x + b.r > W + e || b.y - b.r < -e || b.y + b.r > H + e || b.z - b.r < -D / 2 - e || b.z + b.r > D / 2 + e) outside++;
    lowest = Math.min(lowest, b.y - b.r);
    const col = Math.min(COLS - 1, Math.floor(b.x / W * COLS));
    front[Math.min(ROWS - 1, Math.floor((1 - b.y / H) * ROWS))][col] = "o";
    top[Math.min(ROWS - 1, Math.floor((b.z + D / 2) / D * ROWS))][col] = "o";
  }
  let worst = 0, overlapping = 0;
  for (let i = 0; i < balls.length; i++) for (let j = i + 1; j < balls.length; j++) {
    const a = balls[i], b = balls[j];
    const o = a.r + b.r - Math.hypot(a.x - b.x, a.y - b.y, a.z - b.z);
    if (o > worst) worst = o;
    if (o > 0.01) overlapping++;
  }
  console.log(`--- ${label}: ${balls.length} balls, ${outside} outside the box, NaN ${nan}, lowest ball bottom ${lowest.toFixed(3)} m`);
  console.log(`    overlap: worst ${worst.toFixed(3)} m, pairs overlapping > 1 cm: ${overlapping}`);
  console.log("    front (x, y)" + " ".repeat(COLS - 10) + "top (x, z)");
  for (let r = 0; r < ROWS; r++) console.log("    " + front[r].join("") + "   " + top[r].join(""));
}

const FRAMES = +process.env.FRAMES || 900; // FRAMES=60 node sim.mjs for a quick run
for (let f = 1; f <= FRAMES; f++) {
  const balls = frame();
  if (f === 2 || f === 30 || f === 120 || f === 400 || f === 900 || f === FRAMES) plot("frame " + f, balls);
}
console.log("draw calls seen (vertices,instances):", [...new Set(draws)].join(" "), "x", draws.length);
