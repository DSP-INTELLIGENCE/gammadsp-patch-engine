# gammadsp-patch-engine

Minimal, CLI-switchable GammaDSP PatchEngine runtime.

This repository is intended to be the small extracted home for the working GammaDSP PatchEngine layer. The full `GammaDSP` repository remains the workbench/research repository. This repo should contain only the source, wrappers, runtime modules, tests, examples, and docs needed to build and operate the PatchEngine through machine-friendly commands.

## Core idea

```text
GammaDSP C++ / SWIG backend
  -> GammaDSP.py / _GammaDSP.so
  -> swig_introspect.py
  -> patch_engine.py
  -> patch_json.py / patch_dsl.py
  -> patch_switch.py
  -> CLI /switch interface
```

The PatchEngine must be fully usable from command-line switches. Python can remain the implementation backend, but users, agents, scripts, and ML/data pipelines should not need to call internal Python APIs directly.

## Essential runtime boundary

A file belongs in this repo only if it is required to do one of these:

1. Build `_GammaDSP` from source.
2. Import `GammaDSP` successfully.
3. Resolve GammaDSP SWIG classes dynamically.
4. Create DSP nodes with `ctor_args` and setter `params`.
5. Build/render `FunctionGraph` or `DSPGraph` patches.
6. Load/save/validate JSON patch files.
7. Run the current machine-checkable tests.
8. Expose the PatchEngine through CLI switches.

Everything else should stay in the larger GammaDSP workbench repo until a failing build or test proves it is needed here.

## Current source checkpoint

This extraction should begin from the known-good GammaDSP branch/tag:

```bash
git checkout audit/json-introspection-01
git tag --list 'checkpoint-legacy-runner-json-introspection-pass'
```

That checkpoint has confirmed:

- root `GammaDSP.py` imports the root `_GammaDSP*.so`
- `runGenerator`, `runFunction`, `FunctionGraph`, and `DSPGraph` are available
- SWIG introspection tests pass
- JSON PatchEngine tests pass
- legacy demo scripts can be run through a repo-root runner

## First build/test loop

After copying the minimal source set into this repo:

```bash
sh GammaDSP.sh
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
```

If the build fails, copy only the missing include or interface file named by the compiler/SWIG error. Do not bulk-copy the whole old repository.

## Repository docs

- `docs/PATCH_ENGINE_BOUNDARY.md` defines what belongs here.
- `docs/ESSENTIAL_FILES.md` lists the first-copy candidate set.
- `docs/BUILD.md` explains the SWIG/backend build loop.
- `docs/TESTS.md` defines truth gates versus human demo scripts.
- `docs/CLI_SWITCH_GOAL.md` defines the `/switch` target.
- `docs/EXTRACTION_PLAN.md` gives the first manual copy sequence.
- `docs/ROADMAP.md` defines next branches.

## Non-goals

This repo should not inherit the old overlay/registry/factory stack as the primary runtime. Avoid copying these unless explicitly needed for archived reference or a later migration branch:

```text
dynamic_factory.py
dynamic_registry.py
dynamic_patch_engine.py
dynamic_patch_dsl.py
factory.py
metadata.py
registry.py
registry_autogen.py
module_overlays.py
validate_registry.py
generate_metadata.py
Scripts/
Tests/
```

The new runtime should stay small, inspectable, and CLI-switchable.
