# Test guide

## Truth gates

The current machine-checkable gates are:

```bash
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
```

These tests should not require audio hardware, graph windows, or manual listening.

## What each test proves

### `test_current_swig_smoke.py`

Verifies that:

- root `GammaDSP.py` imports
- root `_GammaDSP*.so` imports
- `runGenerator` exists
- `runFunction` exists
- `FunctionGraph` exists
- `DSPGraph` exists
- a simple generator/function/graph path can render finite output

### `test_introspection.py`

Verifies that:

- `SWIGResolver` can resolve classes
- setter discovery works
- direct SWIG block rendering works
- `PatchEngine` can create chains
- `PatchEngine` can build serial/parallel graphs
- external-input graph processing works
- DSL graph cases still work

Expected final line:

```text
ALL SWIG INTROSPECTION PATCH TESTS PASSED
```

### `test_patch_json_introspection.py`

Verifies that:

- JSON dictionaries load correctly
- `ctor_args` map to positional constructors
- `params` map to setter calls
- graph shorthand works
- parallel graph patches work
- external-input FX patches work
- chains can materialize as graphs
- save/load roundtrip works

Expected final line:

```text
ALL INTROSPECTION JSON PATCH TESTS PASSED
```

## Legacy demo tests

The old `Tests/*.py` scripts from the full GammaDSP workbench are human demo scripts. They may play audio, open plots, depend on ALSA/PulseAudio, or require visual/audio confirmation.

Do not treat legacy demo failures as CI failures unless they are deliberately migrated into non-interactive tests.

If legacy demos are copied later, run them through a root-path runner so they import the repo-root wrapper and extension.

## Future ML-friendly tests

Add separate non-interactive tests for data generation and machine learning workflows:

```text
tests/test_render_dataset_smoke.py
tests/test_patch_switch_cli.py
tests/test_json_schema_contract.py
tests/test_deterministic_render_seed.py
```

Rules for ML-friendly tests:

- no audio playback
- no graph windows
- deterministic duration/frame count
- JSON output option for CLI tests
- temporary output files only
- assert finite arrays and expected shapes
- keep generated WAVs out of git
