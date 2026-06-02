---
name: documentation-update
description: Decide which docs or skill notes need updating after a code or workflow change. Use when something in the repo changed and the docs need to stay in sync.
---

<objective>
Update only the canonical docs that changed. Keep each fact in one place and link to it everywhere else.
</objective>

<quick_start>
1. Identify what changed in the codebase.
2. Consult the Quick Decision Guide below to find which docs need updating.
3. Read the relevant resource file before editing any documentation.
</quick_start>

<reference_guides>
Each documentation file has a dedicated resource describing what it covers and when it should change.

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
| Change | Files to update |
|---|---|
| GPIO pins or pulse timing in `settings.cpp` | `docs/WIRING_README.md` |
| API endpoint or schema change | `docs/API_REFERENCE.md`, `docs/TEST_README.md`, `skills/api-validation/validate_api.py` |
| WiFi connect strategy change | `AGENTS.md`, `skills/log-analysis/SKILL.md`, `skills/strategy-comparison/SKILL.md` |
| Upload toolchain or FQBN change | `docs/ESP32_UPLOAD_README.md`, `skills/firmware-upload/SKILL.md` |
| New firmware module or restructured code | `AGENTS.md` |
| `[WIFI-CONNECT]` log format change | `skills/log-analysis/SKILL.md`, `skills/strategy-comparison/SKILL.md` |
| New or removed documentation file | `README.md`, `AGENTS.md`, this skill file |
| Test procedure or troubleshooting change | `docs/TEST_README.md` |
| Hardware change | `docs/SETUP_README.md`, `docs/WIRING_README.md`, `README.md`, `AGENTS.md` |
</process>

<context>
Each piece of information has one canonical home. Other files should link to it instead of repeating it.

| Information | Canonical file |
|---|---|
| API endpoint list, count, request/response schemas | `docs/API_REFERENCE.md` |
| WiFi dwell/auto strategy details, timing, performance numbers | `AGENTS.md` |
| Module dependency graph | `AGENTS.md` |
| C++ struct field names vs JSON field name mapping | `AGENTS.md` |
| Hardware pin table | `docs/WIRING_README.md` |
| Software prerequisites and IDE setup | `docs/SETUP_README.md` |
| Upload CLI commands | `docs/ESP32_UPLOAD_README.md` |
| Post-flash validation and troubleshooting | `docs/TEST_README.md` |
| Log analysis usage | `skills/log-analysis/SKILL.md` |
</context>

<anti_patterns>
- **Speculative updates**: changing documentation without a concrete code trigger.
- **Partial sync updates**: updating only one affected file when the decision guide indicates multiple files must be aligned.
- **Duplicating canonical facts**: copying source-of-truth content instead of linking to it.
- **Hardcoding counts outside canonical docs**.
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
