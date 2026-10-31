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
2. Prefer instrumentation (logs, debug output, counters) over guessing from reading code.
3. Never use em dashes. Ever.
4. Small back-and-forths, not big dumps.
5. We build the engine and game ourselves. Low level OS stuff (window, input, audio) can use a library.
6. Before proposing any third party library, weigh benefit vs cost out loud and ask first.
7. Brain changes: always ask first, the user reviews them. Focus files: Claude manages them freely.
8. The user checks every change in the browser. Claude runs quick headless tests first and only mentions them when it matters (a failure, a surprise). Don't bombard the user with info they don't need.
9. Code conventions live in focus/architecture.md and can change. Before following one, ask if it still holds, with a small example ("Use the v2_add / v2_added pair style for the new box math?").
10. Git: never credit Claude. No Co-Authored-By lines, no mention of Claude in commits, PRs or code.
