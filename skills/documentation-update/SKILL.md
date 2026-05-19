# Skill: Documentation Update

## Purpose

Guide agents and developers on **which documentation file to update, what it covers, and what change in the codebase or project triggers an update**. This prevents over-editing (updating files that do not need to change) and under-editing (missing a file that must stay in sync).

---

## Documentation File Map

Each documentation file has a dedicated resource describing what it covers, its source of truth in the codebase, and the exact conditions that require an update. Read the relevant resource file before editing any documentation.

| File(s) | Resource |
|---|---|
| `docs/SETUP_README.md` | `skills/documentation-update/resources/setup.md` |
| `docs/WIRING_README.md` | `skills/documentation-update/resources/wiring.md` |
| `docs/ESP32_UPLOAD_README.md` | `skills/documentation-update/resources/upload.md` |
| `docs/TEST_README.md` | `skills/documentation-update/resources/test-readme.md` |
| `docs/API_REFERENCE.md` | `skills/documentation-update/resources/api-reference.md` |
| `README.md` | `skills/documentation-update/resources/readme.md` |
| `AGENTS.md` | `skills/documentation-update/resources/agents.md` |
| `skills/log-analysis/SKILL.md` | `skills/documentation-update/resources/skill-log-analysis.md` |
| `skills/strategy-comparison/SKILL.md` | `skills/documentation-update/resources/skill-strategy-comparison.md` |
| `skills/firmware-upload/SKILL.md` | `skills/documentation-update/resources/skill-firmware-upload.md` |
| `skills/firmware-optimization-loop/SKILL.md` | `skills/documentation-update/resources/skill-firmware-optimization.md` |

---

## Quick Decision Guide

| Something changed in… | Files to update |
|---|---|
| GPIO pins or pulse timing in `settings.cpp` | `docs/WIRING_README.md`, `AGENTS.md` |
| API endpoint added/removed/changed | `docs/API_REFERENCE.md`, `docs/TEST_README.md`, `README.md`, `AGENTS.md`, `skills/api-validation/validate_api.py` |
| WiFi connect strategy (dwell/auto) parameters or names | `AGENTS.md`, `skills/log-analysis/SKILL.md` |
| Upload toolchain or FQBN | `docs/ESP32_UPLOAD_README.md`, `skills/firmware-upload/SKILL.md`, `AGENTS.md` |
| New firmware module or restructured code | `AGENTS.md` (module graph) |
| `[WIFI-CONNECT]` log format changes | `skills/log-analysis/SKILL.md`, `skills/strategy-comparison/SKILL.md`, `AGENTS.md` |
| New documentation file added | `README.md` (index), `AGENTS.md` (file map), this skill's file map table |
| Test procedure or troubleshooting knowledge changes | `docs/TEST_README.md` |
| Hardware changes (board, Ethernet chip) | `docs/SETUP_README.md`, `docs/WIRING_README.md`, `README.md`, `AGENTS.md` |

---

## Single-Source-of-Truth Map

Each piece of information has exactly one canonical home. All other files must link to it rather than duplicate it.

| Information | Canonical file |
|---|---|
| WiFi dwell/auto strategy details, timing, performance numbers | `AGENTS.md` |
| Module dependency graph | `AGENTS.md` |
| C++ struct field names vs JSON field name mapping | `AGENTS.md` |
| Hardware pin table | `docs/WIRING_README.md` |
| Software prerequisites and IDE setup | `docs/SETUP_README.md` |
| Upload CLI commands (full procedure) | `docs/ESP32_UPLOAD_README.md` |
| Full API request/response schemas | `docs/API_REFERENCE.md` |
| Post-flash validation and troubleshooting | `docs/TEST_README.md` |
| Log analysis usage | `skills/log-analysis/SKILL.md` |

---

## Rules for Agents

1. **Never update a documentation file speculatively.** Only update when its specific trigger conditions are met.
2. **When a code change triggers a doc update**, update all files listed in the quick-decision table for that change type in the same session.
3. **Never duplicate information.** If content belongs in a canonical file, put it there and add a brief summary + link in other files. Check the single-source-of-truth map before writing.
4. **Preserve the dwell/auto strategy documentation** in `AGENTS.md` — these strategies are in active use for long-term real-world A/B performance measurement. Do not remove or summarize away this information until explicitly instructed.
5. **Do not create new documentation files** unless explicitly asked. Prefer updating an existing file.
