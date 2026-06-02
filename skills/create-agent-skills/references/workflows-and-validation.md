<overview>
Patterns for robust workflows and validation loops in Copilot skills.
</overview>

<workflow_pattern>
For multi-step operations, define explicit ordered phases:
1. inspect
2. plan
3. validate plan
4. execute
5. verify
</workflow_pattern>

<checklist_pattern>
If a flow has many steps, include a checklist block so progress can be resumed safely.
</checklist_pattern>

<validate_fix_repeat>
Use this control loop:
- run validator
- read concrete error
- apply focused fix
- rerun validator
- continue only when clean
</validate_fix_repeat>

<conditional_branches>
For branching operations, provide clear if-then decision points and separate workflows per branch.
</conditional_branches>

<validation_script_quality>
Validation tools should report:
- exact failing field or line
- expected value or format
- valid alternatives when applicable
</validation_script_quality>

<completion_criteria>
A workflow is complete when:
- defined outputs exist
- validation passes
- post-check confirms expected behavior
</completion_criteria>
