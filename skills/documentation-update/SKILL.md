---
name: documentation-update
description: Guide agents on which documentation file to update, what it covers, and what codebase change triggers an update. Use when code changes require documentation alignment.
---

<objective>
Prevent over-editing (updating files that don't need change) and under-editing (missing files that must stay in sync) by providing a precise mapping between code changes and documentation files.
</objective>

<quick_start>
1. Identify what changed in the codebase.
2. Consult the Quick Decision Guide below to find which docs need updating.
3. Read the relevant resource file before editing any documentation.
</quick_start>

<reference_guides>
Each documentation file has a dedicated resource describing what it covers, its source of truth, and the exact conditions that require an update.

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
</reference_guides>

<process>
| Something changed in… | Files to update |
|---|---|
| GPIO pins or pulse timing in `settings.cpp` | `docs/WIRING_README.md` |
| API endpoint added/removed/changed | `docs/API_REFERENCE.md`, `docs/TEST_README.md`, `skills/api-validation/validate_api.py` |
| WiFi connect strategy (dwell/auto) parameters or names | `AGENTS.md`, `skills/log-analysis/SKILL.md` |
| Upload toolchain or FQBN | `docs/ESP32_UPLOAD_README.md`, `skills/firmware-upload/SKILL.md` |
| New firmware module or restructured code | `AGENTS.md` (module graph) |
| `[WIFI-CONNECT]` log format changes | `skills/log-analysis/SKILL.md`, `skills/strategy-comparison/SKILL.md` |
| New documentation file added | `README.md` (index), `AGENTS.md` (file map), this skill's file map table |
| Test procedure or troubleshooting knowledge changes | `docs/TEST_README.md` |
| Hardware changes (board, Ethernet chip) | `docs/SETUP_README.md`, `docs/WIRING_README.md`, `README.md`, `AGENTS.md` |
</process>

<context>
Each piece of information has exactly one canonical home. All other files must link to it rather than duplicate it.

| Information | Canonical file |
|---|---|
| API endpoint list, count, request/response schemas | `docs/API_REFERENCE.md` |
| WiFi dwell/auto strategy details, timing, performance numbers | `AGENTS.md` |
| Module dependency graph | `AGENTS.md` |
| C++ struct field names vs JSON field name mapping | `AGENTS.md` |
| Hardware pin table | `docs/WIRING_README.md` |
| Software prerequisites and IDE setup | `docs/SETUP_README.md` |
| Upload CLI commands (full procedure) | `docs/ESP32_UPLOAD_README.md` |
| Full API request/response schemas | `docs/API_REFERENCE.md` |
| Post-flash validation and troubleshooting | `docs/TEST_README.md` |
| Log analysis usage | `skills/log-analysis/SKILL.md` |
</context>

<anti_patterns>
- **Speculative updates**: changing documentation without a concrete code trigger.
- **Partial sync updates**: updating only one affected file when the decision guide indicates multiple files must be aligned.
- **Duplicating canonical facts**: copying source-of-truth content instead of linking to it.
- **Hardcoding counts outside canonical docs** (for example endpoint totals), which drift quickly.
- **Removing dwell/auto strategy docs from `AGENTS.md`** without explicit instruction.
- **Creating new documentation files by default** instead of reusing existing docs.
</anti_patterns>

<success_criteria>
Documentation update is complete when:
- [ ] All files from the decision guide for the change type are updated
- [ ] No information is duplicated — only one canonical source per fact
- [ ] Links to canonical sources are correct
- [ ] No speculative updates to unaffected files
</success_criteria>
