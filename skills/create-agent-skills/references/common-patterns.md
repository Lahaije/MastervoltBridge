<overview>
Reusable patterns for writing high-signal Copilot skills.
</overview>

<pattern_default_with_escape_hatch>
Give one default method and one clear exception path.
</pattern_default_with_escape_hatch>

<pattern_examples>
Use short input-output examples when output format is strict.
</pattern_examples>

<pattern_consistent_terms>
Choose one term per concept and keep it consistent across all files.
</pattern_consistent_terms>

<pattern_progressive_disclosure>
Place essentials in SKILL.md and move deep details to references.
</pattern_progressive_disclosure>

<pattern_validation_loop>
For risky changes, instruct: run validator, fix issues, rerun, then proceed.
</pattern_validation_loop>

<pattern_reference_depth>
Keep references one level from SKILL.md when possible for discoverability.
</pattern_reference_depth>

<anti_patterns>
Avoid:
- too many alternative tools in quick_start
- ambiguous obligation language
- vague output definitions
- hidden assumptions about environment paths
</anti_patterns>

<example_commit_message_pattern>
```xml
<examples>
<example number="1">
<input>Add caching to API client</input>
<output>feat(api): add response caching to reduce repeated calls</output>
</example>
</examples>
```
</example_commit_message_pattern>
