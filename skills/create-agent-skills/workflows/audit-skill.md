# Workflow: Audit a Skill

<required_reading>
**Read these reference files NOW:**
1. references/recommended-structure.md
2. references/skill-structure.md
3. references/use-xml-tags.md
</required_reading>

<process>
## Step 1: List Available Skills

**DO NOT use AskUserQuestion** - there may be many skills.

Enumerate skills in chat as numbered list:
```powershell
Get-ChildItem d:\git\MastervoltBridge\skills\ -Directory | ForEach-Object { $_.Name }
```

Present as:
```
Available skills:
1. api-validation
2. create-agent-skills
3. documentation-update
4. firmware-optimization-loop
5. firmware-upload
6. log-analysis
7. strategy-comparison
```

Ask: "Which skill would you like to audit? (enter number or name)"

## Step 2: Read the Skill

After user selects, read the full skill structure:
```powershell
# Read main file
Get-Content d:\git\MastervoltBridge\skills\{skill-name}\SKILL.md

# Check for workflows and references
Get-ChildItem d:\git\MastervoltBridge\skills\{skill-name}\
Get-ChildItem d:\git\MastervoltBridge\skills\{skill-name}\workflows\ -ErrorAction SilentlyContinue
Get-ChildItem d:\git\MastervoltBridge\skills\{skill-name}\references\ -ErrorAction SilentlyContinue
```

## Step 3: Run Audit Checklist

Evaluate against each criterion:

### YAML Frontmatter
- [ ] Has `name:` field (lowercase-with-hyphens)
- [ ] Name matches directory name
- [ ] Has `description:` field
- [ ] Description says what it does AND when to use it
- [ ] Description is third person ("Use when...")

### Structure
- [ ] SKILL.md under 500 lines
- [ ] Pure XML structure (no markdown headings # in body)
- [ ] All XML tags properly closed
- [ ] Has required tags: objective OR essential_principles
- [ ] Has success_criteria

### Router Pattern (if complex skill)
- [ ] Essential principles inline in SKILL.md (not in separate file)
- [ ] Has intake question
- [ ] Has routing table
- [ ] All referenced workflow files exist
- [ ] All referenced reference files exist

### Workflows (if present)
- [ ] Each has required_reading section
- [ ] Each has process section
- [ ] Each has success_criteria section
- [ ] Required reading references exist

### Content Quality
- [ ] Principles are actionable (not vague platitudes)
- [ ] Steps are specific (not "do the thing")
- [ ] Success criteria are verifiable
- [ ] No redundant content across files

## Step 4: Generate Report

Present findings as:

```
## Audit Report: {skill-name}

### ✅ Passing
- [list passing items]

### ⚠️ Issues Found
1. **[Issue name]**: [Description]
   → Fix: [Specific action]

2. **[Issue name]**: [Description]
   → Fix: [Specific action]

### 📊 Score: X/Y criteria passing
```

## Step 5: Offer Fixes

If issues found, ask:
"Would you like me to fix these issues?"

Options:
1. **Fix all** - Apply all recommended fixes
2. **Fix one by one** - Review each fix before applying
3. **Just the report** - No changes needed

If fixing:
- Make each change
- Verify file validity after each change
- Report what was fixed
</process>

<audit_anti_patterns>
## Common Anti-Patterns to Flag

**Skippable principles**: Essential principles in separate file instead of inline
**Monolithic skill**: Single file over 500 lines
**Mixed concerns**: Procedures and knowledge in same file
**Vague steps**: "Handle the error appropriately"
**Untestable criteria**: "User is satisfied"
**Markdown headings in body**: Using # instead of XML tags
**Missing routing**: Complex skill without intake/routing
**Broken references**: Files mentioned but don't exist
**Redundant content**: Same information in multiple places
</audit_anti_patterns>

<success_criteria>
Audit is complete when:
- [ ] Skill fully read and analyzed
- [ ] All checklist items evaluated
- [ ] Report presented to user
- [ ] Fixes applied (if requested)
- [ ] User has clear picture of skill health
</success_criteria>
