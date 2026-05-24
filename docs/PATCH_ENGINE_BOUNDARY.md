# PatchEngine boundary

## Purpose

`gammadsp-patch-engine` is the minimal extracted runtime for building, validating, inspecting, and rendering GammaDSP patches.

The larger `GammaDSP` repository can keep experiments, old demo scripts, metadata generators, terminal experiments, audio demos, and research branches. This repo should preserve only the current working PatchEngine path and the files required to operate it from CLI switches.

## Active runtime stack

The preferred runtime stack is:

```text
GammaDSP.hpp / GammaDSP.cpp / GammaDSP.i
  -> SWIG-generated GammaDSP.py and _GammaDSP*.so
  -> swig_introspect.py
  -> patch_engine.py
  -> patch_json.py
  -> patch_dsl.py
  -> patch_switch.py
```

`patch_switch.py` is the intended command-line control surface. It should call the existing Python runtime modules; it should not invent a second patch engine.

## Core rules

1. JSON is a thin compile/load layer over `PatchEngine`.
2. `ctor_args` are positional constructor arguments.
3. `params` are setter calls resolved through SWIG introspection.
4. `FunctionGraph` and `DSPGraph` are explicit graph backends.
5. Function graphs render with `runFunction`, not `runGenerator`.
6. The CLI switch layer should return machine-readable results when requested.
7. Tests must be non-interactive by default.
8. Human audio/graph demo scripts are optional examples, not truth gates.

## Definition of essential

A file is essential only if it is required for at least one of these operations:

- build the SWIG extension
- import `GammaDSP`
- instantiate GammaDSP classes through `SWIGResolver`
- set parameters through live setters
- create chains or graphs
- render or process buffers
- load/validate/save JSON patch files
- run current machine-checkable tests
- expose the runtime over CLI switches

## Explicit non-goals

This repo should not begin as a full mirror of the old workbench repository.

Do not copy the legacy overlay/registry/factory system into the active runtime path unless a later branch deliberately reintroduces it as optional metadata. The current direction is live introspection first.

Avoid copying:

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

## Success condition

The extracted repo is successful when a fresh clone can run:

```bash
sh GammaDSP.sh
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
```

and then execute PatchEngine commands through `patch_switch.py`.
