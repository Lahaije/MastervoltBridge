<overview>
Core principles for creating effective Copilot skills.
</overview>

<principle_xml_structure>
Use semantic XML sections in skill bodies for consistency and reliable parsing.

Required sections:
- `<objective>`
- `<quick_start>`
- `<success_criteria>` or `<when_successful>`
</principle_xml_structure>

<principle_conciseness>
Assume Copilot understands common engineering concepts. Keep guidance compact and task-focused.
</principle_conciseness>

<principle_freedom>
Match instruction strictness to risk:
- fragile operations: exact commands
- normal operations: preferred pattern
- exploratory tasks: principles and checkpoints
</principle_freedom>

<principle_progressive_disclosure>
Keep SKILL.md focused and route deep details into references and workflows. Load only what is needed.
</principle_progressive_disclosure>

<principle_operational_reproducibility>
For release and deployment workflows, include:
1. source snapshot step
2. deterministic versioning rule
3. validation and rollback checks
</principle_operational_reproducibility>

<principle_model_awareness>
Skills should remain robust across model variants. Test with realistic prompts and tighten instructions where behavior is inconsistent.
</principle_model_awareness>

<principle_feedback_loop>
Use observed failures to update skills. Do not optimize for hypothetical issues.
</principle_feedback_loop>
