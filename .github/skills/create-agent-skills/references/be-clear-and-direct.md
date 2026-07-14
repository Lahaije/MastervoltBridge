<golden_rule>
Give instructions that a teammate can execute without hidden assumptions.
</golden_rule>

<overview>
Direct, concrete instructions improve Copilot execution quality and reduce rework.
</overview>

<context_guidance>
State why the task matters and who the output is for. Context should shape decisions, not add noise.
</context_guidance>

<specificity>
Prefer precise directives over broad requests.

Vague: "Improve this report"
Specific: "Produce a markdown report with sections: summary, findings, risks, next steps"
</specificity>

<sequential_steps>
Use ordered steps for operational work:
1. Inspect inputs
2. Apply transformation
3. Validate output
4. Report result
</sequential_steps>

<show_examples>
When format matters, show one input and one expected output. Copilot follows demonstrated patterns more reliably than abstract descriptions.
</show_examples>

<remove_ambiguity>
Avoid uncertain language:
- Replace "try to" with "always" or "if X then Y"
- Replace "generally" with explicit exception rules
- Replace "consider" with measurable criteria
</remove_ambiguity>

<edge_cases>
Define behavior for:
- empty input
- malformed data
- duplicates
- partial failures
</edge_cases>

<success_definition>
Include explicit completion criteria so Copilot can stop confidently:
- output exists
- validation passes
- required sections present
</success_definition>
