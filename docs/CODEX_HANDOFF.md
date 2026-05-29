# Codex handoff plan

This repo should become easy for Codex or another coding agent to operate safely.

## Codex operating principles

- Read `AGENTS.md` first.
- Treat JSON and DSL as the patch languages.
- Treat `patch_switch.py` as a runner only.
- Do not add hardcoded DSP-specific CLI verbs when JSON/DSL can express the patch.
- Use the staged patch ZIP workflow when generating changes outside a live checkout.
- Run the full gate stack before commit.
- Never commit generated SWIG outputs or audio artifacts.

## Recommended Codex task shape

Each Codex task should include:

```text
Goal:
Files allowed:
Files not allowed:
Expected tests:
Commit message:
PR title:
Merge criteria:
```

## First Codex-ready task

```text
Goal: add schema-lite validation and examples for patch metadata and I/O contracts.
Files allowed:
  docs/PATCH_DESIGN_GUIDE.md
  docs/PATCH_SCHEMA_V0.md
  examples/*.json
  examples/*.dsl
  tests/test_patch_examples_schema.py
Files not allowed:
  GammaDSP.cpp
  GammaDSP.hpp
  GammaDSP.i
  include/**
  patch_engine.py unless a failing test proves it is necessary
Expected tests:
  tests/test_current_swig_smoke.py
  tests/test_introspection.py
  tests/test_patch_json_introspection.py
  tests/test_patch_switch_cli.py
  tests/test_patch_switch_json_dsl_runner.py
  tests/test_patch_examples_schema.py
Commit message:
  Add schema-lite patch design examples
PR title:
  Add schema-lite patch design examples
Merge criteria:
  all gates pass and examples remain non-interactive
```

## Future automation

Later this repo can add:

- `scripts/run_all_gates.sh`
- GitHub Actions for non-interactive tests
- issue and PR templates
- schema checking for examples
- batch render/process validation
