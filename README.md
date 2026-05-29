# gammadsp-patch-engine

Minimal GammaDSP PatchEngine runtime extracted from the larger GammaDSP workbench repository.

This repo is the small, machine-checkable home for the working PatchEngine layer: SWIG-backed GammaDSP classes, reflective node construction, JSON/DSL patch documents, automation lanes, examples, tests, and a thin CLI runner.

## Current architecture

```text
GammaDSP C++ / SWIG backend
  -> GammaDSP.py / _GammaDSP*.so
  -> swig_introspect.py
  -> patch_engine.py
  -> patch_json.py / patch_dsl.py
  -> patch_switch.py
  -> scripts, agents, tests, and data workflows
```

The CLI is a runner, not a patch language. JSON and DSL are the authored patch formats. The PatchEngine remains reflective internally: it resolves live SWIG classes, constructs objects from `ctor_args`, applies setter-backed `params`, builds graphs/chains, and renders or processes audio buffers.

## What a patch is

A GammaDSP patch is a declarative audio document with:

1. identity and metadata,
2. an input/output contract,
3. reflective GammaDSP nodes,
4. graph or chain topology,
5. public controls,
6. optional automation lanes.

Two patch modes are currently first-class:

```text
generator patch  -> render()  -> output audio
processor patch  -> process(input_buffer) -> output audio
```

Use `render` for generator-backed patches such as oscillators. Use `process` for external-input FX patches. Do not model external input as a fake SWIG node; use graph input names such as `input` in topology.

## Current truth gates

Run these from the repo root after building `_GammaDSP`:

```bash
source ~/dsp/bin/activate
PYTHON=python sh GammaDSP.sh

PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_cli.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_json_dsl_runner.py
```

Passing state as of the current planning baseline:

- SWIG smoke gate passes.
- SWIG introspection PatchEngine gate passes.
- JSON PatchEngine gate passes.
- Patch switch CLI gate passes.
- JSON/DSL runner gate passes.

## Repository map

```text
GammaDSP.cpp / GammaDSP.hpp / GammaDSP.i / GammaDSP.sh
  SWIG backend source and build script.

swig_introspect.py
  Reflective class discovery, constructor dispatch, setter mapping, and capability inspection.

patch_engine.py
  Runtime PatchEngine: nodes, params, chains, graphs, render/process paths, runtime contract.

patch_json.py
  JSON patch loader/saver and LoadedJSONPatch render/process wrapper.

patch_dsl.py
  Compact authoring layer for patch documents.

automation.py
  Block-rate automation lanes used by render/process paths.

patch_switch.py
  Thin CLI runner for JSON/DSL patch documents.

docs/
  Architecture, roadmap, milestones, handoff prompts, and agent instructions.

examples/
  Small generator and processor patch fixtures.

tests/
  Machine-checkable runtime and runner gates.
```

## Working rules

- Keep JSON and DSL as the patch languages.
- Keep `patch_switch.py` thin: operation + files + output format.
- Do not duplicate PatchEngine logic in CLI code.
- Do not invent hardcoded DSP commands for things the engine can express through JSON/DSL.
- Do not add generated outputs to git: `_GammaDSP*.so`, `GammaDSP.py`, `GammaDSP_wrap.cxx`, `*.gch`, audio files, or caches.
- Add files only when a build, runtime path, test, or documented design goal proves they belong in this repo.
- Package repo changes as patch ZIPs with staged scripts.

## Patch package workflow

Patch ZIPs should include:

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

Use chained commands only when the final command does not end with a dangling `&&` or trailing backslash.

## Current checkpoints

Known checkpoint tags:

```text
checkpoint-minimal-patch-engine-json-pass
checkpoint-patch-switch-cli-render-01
checkpoint-json-dsl-runner-process-01
```

The current planning direction is documented in:

- `docs/MEMORY.md`
- `docs/MILESTONES.md`
- `docs/ROADMAP.md`
- `docs/PATCH_DESIGN_GUIDE.md`
- `docs/CHATGPT_HANDOFF.md`
- `AGENTS.md`

## Near-term goal

Design patches as reusable audio behaviors, not raw SWIG call logs. The next work should strengthen patch authoring conventions: metadata, I/O contracts, controls, automation, examples, and schema checks.
