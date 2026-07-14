<overview>
Use XML tags in skill bodies for stable structure and predictable behavior.
</overview>

<required_tags>
Every skill should define:
- `<objective>`: mission and scope
- `<quick_start>`: first usable path
- `<success_criteria>` or `<when_successful>`: done definition
</required_tags>

<optional_tags>
Add as needed:
- `<context>`
- `<workflow>` or `<process>`
- `<validation>`
- `<examples>`
- `<anti_patterns>`
- `<reference_guides>`
- `<testing>`
</optional_tags>

<tag_selection>
Choose tags by complexity:
- simple: required tags only
- medium: required plus process and validation
- complex: add context, examples, anti-patterns, references
</tag_selection>

<nesting_rules>
- close every tag
- keep nesting shallow and readable
- use semantic names, not generic placeholders
</nesting_rules>

<format_rules>
Do not rely on markdown headings for section identity in skill bodies. Use XML tags for structure.
</format_rules>

<example>
```xml
<objective>
Validate and publish API docs safely.
</objective>

<quick_start>
Run the validator, fix findings, rerun until clean.
</quick_start>

<success_criteria>
All checks pass and docs are consistent with implementation.
</success_criteria>
```
</example>
