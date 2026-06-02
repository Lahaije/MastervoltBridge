<overview>
Use executable scripts when repeated logic must be reliable, auditable, and efficient.
</overview>

<why_scripts>
Scripts reduce prompt bloat, eliminate regenerated code drift, and improve repeatability.
</why_scripts>

<execution_model>
When Copilot runs a script, only script output enters context. The script source does not need to be re-explained each run.
</execution_model>

<organization>
Place scripts in a skill-local scripts folder:

- skills/skill-name/scripts/main.py
- skills/skill-name/scripts/validate.py
- skills/skill-name/scripts/helpers.ps1
</organization>

<authoring_guidelines>
1. Validate input arguments
2. Return clear exit codes
3. Print actionable errors
4. Keep side effects explicit
5. Make reruns safe when possible
</authoring_guidelines>

<error_handling>
Handle common failure modes in code instead of pushing diagnosis into prompts.
</error_handling>

<configuration>
Document key constants with short rationale. Avoid unexplained magic values.
</configuration>

<dependencies>
Declare required packages in skill docs and provide one install command.

For this repo, package installs should use:
- `uv pip install <package>`
</dependencies>

<mcp_tools>
When a skill depends on MCP tools, refer to fully qualified tool names to avoid ambiguity.
</mcp_tools>
