<overview>
Iterative skill development should be evidence-driven: baseline, change, validate, compare.
</overview>

<evaluation_first>
Start with representative tasks and capture baseline behavior before editing a skill.
</evaluation_first>

<loop>
1. Run baseline prompts
2. Record misses and ambiguities
3. Apply minimal skill edits
4. Re-run same prompts
5. Keep edits that improve outcomes
</loop>

<test_matrix>
Include tests for:
- happy path
- missing input
- malformed input
- partial failure
- recovery path
</test_matrix>

<validation>
After each edit, verify:
- required XML sections present
- references resolve correctly
- commands are executable as documented
</validation>

<discovery_testing>
Confirm the skill is discoverable by asking intent-matching prompts in a fresh chat context.
</discovery_testing>

<refinement>
Prioritize fixes from real failures over speculative polish.
</refinement>
