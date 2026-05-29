# AGENTS.md

Instructions for ChatGPT, Codex, and other agents working in this repository.

## Project summary

`gammadsp-patch-engine` is the extracted, minimal GammaDSP PatchEngine runtime. The larger `GammaDSP` repo remains the workbench/research repo.

This repo should stay small, machine-checkable, and patch-document-centered.

## Architecture rules

- JSON and DSL are the patch languages.
- `patch_switch.py` is a thin runner, not a third patch language.
- `patch_engine.py` owns runtime behavior.
- `swig_introspect.py` owns reflective SWIG discovery and setter mapping.
- `patch_json.py` and `patch_dsl.py` own patch document loading/authoring.
- Do not duplicate PatchEngine logic inside CLI code.
- Do not add hardcoded commands like `process-sine` or `process-noise` unless they are explicitly test fixtures, and prefer data files over special commands.

## Patch rules

- A patch should be a named audio behavior, not a raw SWIG call log.
- Use real SWIG class names or resolver-compatible names for node types.
- Use `ctor_args` for constructor-only values.
- Use `params` only for values backed by available setter methods.
- External input belongs in topology, not in `nodes`.
- `render` is for generator patches.
- `process` is for processor patches with caller-supplied input data.

## File rules

Do not commit generated artifacts:

```text
GammaDSP.py
GammaDSP_wrap.cxx
_GammaDSP*.so
*.gch
__pycache__/
*.wav
patches/
```

Do not bulk-copy old systems from the workbench repo unless a build or test proves they are needed.

## Required test gates

Run from the repo root:

```bash
source ~/dsp/bin/activate
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_cli.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_json_dsl_runner.py
```

If the SWIG extension is missing or stale, rebuild first:

```bash
source ~/dsp/bin/activate
PYTHON=python sh GammaDSP.sh
```

## Patch package workflow

All assistant-generated repo changes should be packaged as ZIPs with:

```text
<patch-name>.patch
changed-files.txt
README.md
apply_patch.sh
scripts/
  00_install.sh
  01_apply.sh
  02_test.sh
  03_verify.sh
  04_commit.sh
  05_push.sh
  06_pr.sh
  07_merge.sh
```

Scripts must be safe to run from the repo root and should stop on errors.

## Git workflow

Recommended branch flow:

```bash
git checkout main
git pull --ff-only origin main
git checkout -b feature/<short-name>
# apply changes
git status --short
git diff --stat
# run tests
git add ...
git commit -m "<message>"
git push -u origin HEAD
gh pr create --title "<title>" --body "<body>"
```

Merge only after gates pass.

## Current next recommendation

Focus on patch authoring conventions and schema-lite example validation before adding more CLI surface area.
