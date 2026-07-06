<overview>
This reference explains secure API usage patterns for Copilot skills so secrets never appear in chat, logs, or committed files.
</overview>

<risk>
Commands that inline tokens can leak secrets into terminal history and model-visible output.
</risk>

<unsafe_pattern>
Do not teach raw commands like this in skill docs:

```bash
curl -H "Authorization: Bearer $API_KEY" https://api.example.com/data
```
</unsafe_pattern>

<safe_pattern>
Use wrapper scripts in the skill folder so credentials are loaded internally:

```bash
.venv/Scripts/python.exe skills/your-skill/scripts/call_api.py list-items
```

Prefer this structure:
- `skills/your-skill/scripts/call_api.py` for operations
- environment variables for credentials
- explicit, non-secret error messages
</safe_pattern>

<credential_sources>
Use one of these sources, in order:
1. Process environment variables
2. Local non-committed file such as `.env` loaded by script
3. OS-level secret manager if available

Never commit secret values.
</credential_sources>

<profile_support>
If multiple accounts are needed, support a profile argument in the wrapper:

```bash
.venv/Scripts/python.exe skills/your-skill/scripts/call_api.py --profile main list-items
```

Store profile-to-env mapping in script code, not in chat prompts.
</profile_support>

<script_contract>
Every API wrapper script should:
1. Validate required environment variables
2. Validate operation names
3. Print only safe diagnostics
4. Return non-zero exit code on failure
5. Avoid printing request headers that include secrets
</script_contract>

<recommended_error_messages>
Use actionable errors:
- Missing variable: "Set YOURSERVICE_API_KEY before running this command"
- Unsupported operation: "Unknown operation 'x'. Valid operations: list, get, create"
- HTTP failure: "Request failed with status 401. Check credentials and scope"
</recommended_error_messages>

<skill_authoring_rules>
1. Do not include secret values in examples
2. Do not require users to paste tokens into chat
3. Prefer wrappers over ad hoc command composition
4. Keep examples runnable with placeholder values only
5. Include one secure happy-path command per operation
</skill_authoring_rules>

<testing>
Validate without exposing secrets:

```bash
.venv/Scripts/python.exe skills/your-skill/scripts/call_api.py healthcheck
```

If needed, test variable presence only:

```bash
.venv/Scripts/python.exe -c "import os; print('ok' if os.getenv('YOURSERVICE_API_KEY') else 'missing')"
```
</testing>
