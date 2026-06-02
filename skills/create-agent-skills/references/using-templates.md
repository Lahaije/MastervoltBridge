<overview>
Templates provide repeatable structure for outputs that must stay consistent.
</overview>

<placement>
Store templates in a skill-local templates folder.
</placement>

<example_layout>
```text
skills/your-skill/
  templates/
    plan-template.md
    report-template.md
```
</example_layout>

<placeholder_style>
Use a consistent placeholder style such as:
- {{PLACEHOLDER}}
- [PLACEHOLDER]
</placeholder_style>

<workflow_usage>
Workflows should state:
1. which template to read
2. which placeholders to fill
3. what validation checks to run before final output
</workflow_usage>

<quality_rules>
- keep templates structural, not verbose
- avoid excessive sample prose that may be copied blindly
- include required sections only
</quality_rules>
