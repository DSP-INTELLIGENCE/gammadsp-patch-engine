# ChatGPT handoff prompts

Use this file to restart or continue work without losing project context.

## General continuation prompt

```text
Continue work in ~/Downloads/gammadsp-patch-engine. The repo is the extracted GammaDSP PatchEngine runtime. JSON and DSL are the patch languages; patch_switch.py is only a thin runner. PatchEngine remains reflective through SWIG. Package all changes as ZIPs with staged scripts: install, apply, test, verify, commit, push, PR, merge.
```

## Current status prompt

```text
Audit the current gammadsp-patch-engine repo, read README.md, AGENTS.md, docs/MEMORY.md, docs/ROADMAP.md, docs/MILESTONES.md, docs/PATCH_DESIGN_GUIDE.md, and summarize the current architecture, completed milestones, open milestones, and next branch recommendation. Do not change files until asked. Use web or git/gh tools if freshness matters.
```

## Patch package prompt

```text
Create a patch-only ZIP for gammadsp-patch-engine. Include <patch-name>.patch, changed-files.txt, README.md, apply_patch.sh, and scripts/00_install.sh through scripts/07_merge.sh. The staged scripts must stop on errors. Do not end command chains with dangling && or trailing backslashes. Include test and verify stages.
```

## Patch design prompt

```text
Design the next patch as a JSON/DSL patch authoring improvement, not a new CLI language. Keep patch_switch.py thin. Use JSON/DSL examples and schema-lite tests to clarify metadata, I/O contracts, nodes, topology, controls, and automation.
```

## Debugging prompt

```text
A test failed in gammadsp-patch-engine. First determine whether the failure is a PatchEngine bug, an example/schema bug, a CLI runner bug, or a test bug. Prefer the smallest fix. Rerun all gates before commit. Package the fix as a staged ZIP if not editing manually.
```

## Merge/tag prompt

```text
After a PR merges, checkout main, pull --ff-only, run the full gate stack, then add a checkpoint tag only if the gates pass. Push the tag to origin.
```
