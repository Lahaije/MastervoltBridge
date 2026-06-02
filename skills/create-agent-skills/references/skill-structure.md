<overview>
Skill structure has three layers: metadata, execution guidance, and deep references.
</overview>

<layer_1_metadata>
YAML frontmatter defines discoverability:

```yaml
---
name: skill-name
description: What it does and when to use it.
---
```

Use lowercase and hyphens in name.
</layer_1_metadata>

<layer_2_execution>
Main body should contain XML sections that guide execution.

Minimum:
- `<objective>`
- `<quick_start>`
- `<success_criteria>`
</layer_2_execution>

<layer_3_disclosure>
Use progressive disclosure:
- keep primary flow in SKILL.md
- move deep content into references/
- keep operational procedures in workflows/
</layer_3_disclosure>

<naming_conventions>
Prefer verb-noun names:
- create-*
- manage-*
- setup-*
- validate-*
- analyze-*
</naming_conventions>

<anti_patterns>
Avoid:
- vague descriptions
- mismatched directory and skill names
- mixed heading styles inside skill body
- deeply chained references that hide critical details
</anti_patterns>
