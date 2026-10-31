# brain

How we work. Read this first, every session.

## Files
- brain.md: how we work. Always loaded.
- focus/<task>.md: one per task. Tracks work done, decisions, and where we left off. Claude keeps these up to date without asking.
- handover/<task>.md: the prompt that starts the next session on a task.
- Every handover prompt starts with: "Read brain.md first."
- Ending a session: when the user says so, Claude updates the focus files, then rewrites handover/<task>.md with where we are and the next step. The next session starts from it.

## Rules
1. Keep language simple and clear. Don't overexplain.
2. Save tokens. Do what was asked, nothing more. Read and search narrowly (the part of a file you need). Keep replies short. Don't run benchmarks, screenshots or browser checks on your own, but do suggest a test or check where it would help and let the user decide. Before anything long (many steps, long runs), say what and why in one line, then ask.
3. Prefer instrumentation (logs, debug output, counters) over guessing from reading code.
4. Never use em dashes. Ever.
5. Small back-and-forths, not big dumps.
6. We build the engine and game ourselves. Low level OS stuff (window, input, audio) can use a library.
7. Before proposing any third party library, weigh benefit vs cost out loud and ask first.
8. Brain changes: always ask first, the user reviews them. Focus files: Claude manages them freely.
9. The user checks every change in the browser. Claude runs quick headless tests first and only mentions them when it matters (a failure, a surprise). Don't bombard the user with info they don't need.
10. Code conventions live in focus/architecture.md and can change. Before following one, ask if it still holds, with a small example ("Use the v2_add / v2_added pair style for the new box math?").
11. Git: never credit Claude. No Co-Authored-By lines, no mention of Claude in commits, PRs or code.
