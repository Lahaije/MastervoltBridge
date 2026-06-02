<overview>
Recommended structure for medium and complex Copilot skills.
</overview>

<directory_layout>
Use this layout when a skill has multiple workflows:

```text
skill-name/
  SKILL.md
  workflows/
  references/
  templates/
  scripts/
```
</directory_layout>

<separation_of_concerns>
- SKILL.md: routing and non-negotiable principles
- workflows/: procedural execution paths
- references/: domain knowledge and examples
- templates/: output skeletons
- scripts/: executable automation
</separation_of_concerns>

<when_to_use_router_pattern>
Use router pattern when:
- there are distinct intents
- each intent needs different context
- loading everything would waste tokens
</when_to_use_router_pattern>

<skill_md_contract>
SKILL.md should include:
1. essential principles
2. intake question
3. intent-to-workflow routing
4. short index of references
</skill_md_contract>

<workflow_contract>
Each workflow should include:
- required reading list
- ordered process
- success criteria
- validation step
</workflow_contract>

<reference_contract>
Reference files should contain stable knowledge only, not step-by-step execution commands that belong in workflows.
</reference_contract>

<scaling_rule>
If SKILL.md exceeds roughly 500 lines, move details into references and templates.
</scaling_rule>
