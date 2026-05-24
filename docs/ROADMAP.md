# Roadmap

## Phase 0: Documentation baseline

Goal: make the new repo understandable before copying source.

Files:

```text
README.md
docs/PATCH_ENGINE_BOUNDARY.md
docs/ESSENTIAL_FILES.md
docs/BUILD.md
docs/TESTS.md
docs/CLI_SWITCH_GOAL.md
docs/EXTRACTION_PLAN.md
docs/ROADMAP.md
```

Definition of done:

- docs are committed to `main`
- repo purpose is clear
- essential/non-essential file boundary is explicit

## Phase 1: Minimal runtime extraction

Goal: copy only enough files to build `_GammaDSP` and run the current machine-checkable tests.

Branch suggestion:

```text
extract/minimal-patch-engine-runtime-01
```

Definition of done:

```bash
sh GammaDSP.sh
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
```

## Phase 2: CLI switch surface

Goal: expose the PatchEngine through command-line switches.

Branch suggestion:

```text
feature/patch-switch-cli-01
```

Files:

```text
patch_switch.py
tests/test_patch_switch_cli.py
docs/CLI_SWITCH_REFERENCE.md
```

First commands:

```bash
python patch_switch.py status --json
python patch_switch.py list-classes --json
python patch_switch.py describe-class LowPassFilter --json
python patch_switch.py validate-json examples/hello_osc_filter_delay.json --json
python patch_switch.py render-json examples/hello_osc_filter_delay.json --seconds 0.25 --out /tmp/gammadsp-test.wav --json
```

## Phase 3: Examples and JSON patch catalog

Goal: provide small reproducible patches for scripts and ML pipelines.

Files:

```text
examples/hello_osc_filter_delay.json
examples/external_input_fx.json
examples/parallel_filter_graph.json
examples/chain_materialized_as_graph.json
```

Definition of done:

- every example validates through CLI
- every example can render non-interactively
- no audio playback required

## Phase 4: ML-friendly render/data commands

Goal: generate deterministic patch outputs for ML/data workflows.

Possible commands:

```bash
python patch_switch.py render-json examples/hello_osc_filter_delay.json --seconds 1 --out out.wav --json
python patch_switch.py render-batch examples/*.json --seconds 1 --out-dir /tmp/gammadsp-renders --manifest /tmp/manifest.jsonl
python patch_switch.py inspect-audio /tmp/gammadsp-renders/out.wav --json
```

Rules:

- no graph windows
- no audio playback
- deterministic frame counts
- JSON/JSONL manifests
- all generated data goes to explicit output paths

## Phase 5: Optional legacy demo migration

Goal: keep human audio/graph demos available without confusing them with CI tests.

Possible structure:

```text
demos/audio/
demos/graphs/
demos/legacy/
```

Rules:

- demos may play audio or show graphs
- tests should not
- demos should document required local devices/packages
