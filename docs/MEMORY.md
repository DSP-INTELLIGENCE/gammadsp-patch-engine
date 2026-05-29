# Project memory

This file is repository memory for humans and agents. It is not ChatGPT account memory.

## Current project intent

`gammadsp-patch-engine` is the small extracted runtime for the working GammaDSP PatchEngine. The larger `GammaDSP` repo remains the workbench/research repo.

The goal is not to expose every SWIG method as a handcrafted CLI command. The goal is to author patches as JSON/DSL documents and execute them through a reflective PatchEngine.

## Locked design decisions

- JSON and DSL are the patch languages.
- CLI is a thin runner and automation boundary.
- PatchEngine remains reflective internally.
- SWIG classes are resolved from the live `GammaDSP` module.
- `ctor_args` are for constructor-only values.
- `params` are for setter-backed values only.
- External input is topology, not a fake node type.
- `render` is for generator patches.
- `process` is for processor patches with caller-supplied input data.
- Tests must be non-interactive: no audio playback, no graph windows.
- Generated outputs stay out of git.
- Patch changes are packaged as ZIPs with staged scripts.

## Known good checkpoints

```text
checkpoint-minimal-patch-engine-json-pass
checkpoint-patch-switch-cli-render-01
checkpoint-json-dsl-runner-process-01
```

## Important lessons learned

- `python3-config` can point at pyenv and mismatch the active virtualenv. `GammaDSP.sh` should use the active `PYTHON=python` path and derive Python/NumPy include paths from that interpreter.
- NumPy headers come from `numpy.get_include()`.
- `SoftClip` does not expose a `level` setter in the current SWIG surface. Do not put unsupported params in examples.
- Output-file assertions inside temporary directories must happen before the temp directory context exits.
- A command line ending in `&&` or a trailing backslash leaves the shell waiting at a `>` continuation prompt.

## Current gates

Run from repo root after building:

```bash
source ~/dsp/bin/activate
PYTHON=python sh GammaDSP.sh
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_cli.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_json_dsl_runner.py
```

## Next best work

The next meaningful branch should focus on patch authoring conventions and schema-lite example validation, not on expanding CLI vocabulary.
