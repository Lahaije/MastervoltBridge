<overview>
Guidance for using executable scripts inside Copilot skills.
</overview>

<placement>
Store scripts under a skill-local scripts folder.
</placement>

<example_layout>
```text
skills/your-skill/
  SKILL.md
  scripts/
    run_task.py
    validate.py
```
</example_layout>

<invocation>
Use workspace-relative commands that run in this repo environment:

```bash
.venv/Scripts/python.exe skills/your-skill/scripts/run_task.py --help
```
</invocation>

<script_requirements>
1. clear inputs
2. safe defaults
3. explicit exit codes
4. actionable errors
5. deterministic output format where possible
</script_requirements>

<security>
Do not hardcode credentials. Load secrets from environment variables.
</security>
