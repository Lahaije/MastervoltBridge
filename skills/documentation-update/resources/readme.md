# Resource: README.md

**What it documents**: Project entry point — one-paragraph description, hardware summary, API endpoint table (quick ref), two-sentence WiFi architecture overview, build/upload commands, and the documentation index with links.

**Source of truth**: The project as a whole; everything here is a summary that links to a canonical source.

**Single-source-of-truth rules**:
- API table: duplicated here intentionally for quick reference; full schemas live in `docs/API_REFERENCE.md`.
- WiFi architecture: brief summary only (2 sentences + link to `AGENTS.md`). Do NOT expand with full dwell/auto details here.
- Build commands: duplicated here for discoverability; full procedure in `docs/SETUP_README.md`.
- Hardware pin table: NOT here — lives in `docs/WIRING_README.md` only.
- Architecture module list: NOT here — lives in `AGENTS.md` only.

**Update when**:
- Hardware changes (board, Ethernet chip, IP, port).
- API surface changes (add/remove endpoint).
- Build commands change.
- A new documentation file is added (update the index).

**Do NOT update for**: WiFi strategy details, performance numbers, wiring, upload procedure, or internal firmware changes.
