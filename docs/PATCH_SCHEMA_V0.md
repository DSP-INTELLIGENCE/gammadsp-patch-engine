# GammaDSP Patch Schema v0

This document defines the practical v0 schema for GammaDSP PatchEngine patch documents.

The schema is intentionally small and executable. A valid v0 patch is still a normal PatchEngine JSON document that can be loaded by `patch_json.py`; the extra design fields describe patch identity, I/O mode, and public controls for humans, agents, UI tooling, and future datasets.

## Design goals

- JSON and DSL are the patch languages.
- `patch_switch.py` is only a runner for patch documents.
- Patch node `type` values should map to real SWIG-exposed GammaDSP classes or resolver aliases.
- `ctor_args` are constructor arguments.
- `params` are setter-backed node parameters.
- External inputs such as `input` are graph sources, not nodes.
- Public `controls` are patch-facing knobs; raw `params` are compiler-facing node settings.

## Top-level shape

```json
{
  "schema": "gammadsp.patch.v0",
  "name": "Patch Name",
  "description": "Short human-readable description.",
  "tags": ["generator", "filter"],
  "sample_rate": 48000,
  "duration": 0.25,
  "io": {
    "mode": "generator",
    "inputs": [],
    "outputs": ["main"]
  },
  "nodes": [],
  "graph": {},
  "out": "main",
  "controls": [],
  "automation": []
}
```

Only the executable PatchEngine fields are required by the current loader. The design fields are recommended for patch libraries and agent workflows.

## Required executable fields

### `sample_rate`

Integer sample rate. Current tests use `48000`.

### `duration`

Default render duration in seconds for generator patches.

### `nodes`

List of SWIG-backed node declarations.

```json
{
  "id": "lp",
  "type": "LowPassFilter",
  "params": {
    "freq": 900,
    "res": 1.2
  }
}
```

Rules:

- `id` must be unique.
- `type` must resolve through `SWIGResolver`.
- `ctor_args` should contain constructor-only values.
- `params` should only contain setter-backed values.
- Do not declare `input` as a node.

### `graph` / `chain`

A patch may use a `chain` for simple serial routing or a `graph` for explicit routing.

Generator chain:

```json
{
  "chain": {
    "name": "main",
    "nodes": ["osc", "lp", "delay"]
  },
  "out": "main"
}
```

Processor graph:

```json
{
  "graph": {
    "name": "fx",
    "connections": [
      ["input", "lp"],
      ["lp", "clip"]
    ],
    "outputs": [["clip", 1.0]]
  },
  "out": "fx"
}
```

## Recommended design fields

### `schema`

Use:

```json
"schema": "gammadsp.patch.v0"
```

### `io`

Declare how the patch is run.

Generator patch:

```json
"io": {
  "mode": "generator",
  "inputs": [],
  "outputs": ["main"]
}
```

Processor patch:

```json
"io": {
  "mode": "processor",
  "inputs": ["input"],
  "outputs": ["fx"]
}
```

Current machine-checkable modes:

- `generator`: can be rendered without input.
- `processor`: requires an input buffer and uses `process`.

### `controls`

Controls are the patch-facing surface for UI, ML, and automation. They are not required for current execution, but every library-quality patch should include them.

```json
{
  "name": "tone",
  "label": "Tone",
  "type": "float",
  "default": 900,
  "min": 80,
  "max": 8000,
  "targets": [
    {"node": "lp", "param": "freq", "scale": "exp"}
  ]
}
```

Rules:

- `name` is stable and machine-friendly.
- `label` is human-readable.
- `targets` map controls to node params.
- Initial v0 tests only validate shape; they do not yet compile controls into params.

### `automation`

Current supported low-level automation targets node params directly:

```json
{
  "node": "lp",
  "param": "freq",
  "type": "linear",
  "points": [[0.0, 250], [0.25, 1800]]
}
```

Supported types:

- `linear`
- `step`
- `hold`

Future schema revisions may add control-level automation:

```json
{"control": "tone", "type": "linear", "points": [[0.0, 0.2], [1.0, 0.9]]}
```

## Patch archetypes

### Generator

Produces audio with no input buffer.

Examples:

- oscillator into filter
- drone into delay
- modulation source into graph

Runner operation:

```bash
python patch_switch.py render examples/patches/generator_basic.json --seconds 0.25 --json
```

### Processor

Transforms a caller-supplied input buffer.

Examples:

- filter/clip/delay effect
- dynamics processor
- spectral or analysis-informed effect

Runner operation:

```bash
python patch_switch.py process examples/patches/processor_basic.json --input-json examples/input_ramp_short.json --json
```

### Automated generator

Produces audio while automation lanes update node params block-by-block.

Example:

- oscillator into filter with automated filter sweep

Runner operation:

```bash
python patch_switch.py render examples/patches/automated_filter_sweep.json --seconds 0.25 --json
```

## v0 validation policy

The v0 tests intentionally validate a practical subset:

- Patch files are valid JSON.
- `schema` is `gammadsp.patch.v0`.
- `io.mode` is coherent with graph/chain behavior.
- `nodes` have `id` and `type`.
- `controls`, if present, have targets.
- Generator examples render non-silent finite audio.
- Processor examples process a deterministic input buffer.
- Automated examples render non-silent finite audio.

This keeps schema validation close to the live PatchEngine instead of inventing a disconnected formal spec.
