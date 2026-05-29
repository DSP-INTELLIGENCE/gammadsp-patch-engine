# Patch design guide

A GammaDSP patch should be designed as an audio behavior, not as a raw SWIG call log.

## Patch layers

A complete patch should have six conceptual layers:

1. identity,
2. I/O contract,
3. nodes,
4. topology,
5. controls,
6. automation.

The current runtime can execute nodes, topology, params, and automation. Metadata, I/O contracts, and controls should be added as conventions first, then enforced later with schema-lite tests.

## Identity

Recommended metadata:

```json
{
  "version": "0.1",
  "name": "Warm Filtered Saw Delay",
  "description": "Saw oscillator through low-pass filtering and modulated delay.",
  "author": "DSP-INTELLIGENCE",
  "tags": ["generator", "oscillator", "filter", "delay"]
}
```

## I/O contract

Generator patch:

```json
{
  "io": {
    "mode": "generator",
    "inputs": [],
    "outputs": ["main"]
  }
}
```

Processor patch:

```json
{
  "io": {
    "mode": "processor",
    "inputs": ["input"],
    "outputs": ["fx"]
  }
}
```

## Nodes

Node types should be real SWIG class names or names that `SWIGResolver` can resolve.

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

- `ctor_args` are constructor-only values.
- `params` must map to available setter-like methods.
- Do not add unsupported params just because they are musically useful.
- Do not add pseudo-nodes like `input` to `nodes`.

## Topology

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
      ["lp", "delay"],
      ["delay", "clip"]
    ],
    "outputs": ["clip"]
  },
  "out": "fx"
}
```

## Controls

Controls are the public patch interface. They should be separate from raw node params.

Conceptual example:

```json
{
  "controls": [
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
  ]
}
```

This is a design target. Do not require every current example to implement controls until the schema-lite phase.

## Automation

Low-level automation currently targets node params:

```json
{
  "automation": [
    {
      "node": "lp",
      "param": "freq",
      "type": "linear",
      "points": [[0.0, 300], [1.0, 2000], [2.0, 800]]
    }
  ]
}
```

Future automation can target public controls:

```json
{
  "automation": [
    {
      "control": "tone",
      "type": "linear",
      "points": [[0.0, 0.2], [1.0, 0.9], [2.0, 0.4]]
    }
  ]
}
```

## Good patch archetypes

- Instrument / generator: produces audio from no input.
- FX / processor: transforms caller-supplied input.
- Utility / analysis: produces measurements or control signals.
- Macro patch: reusable named behavior with a public control surface.

## Design rule

A patch is a named audio behavior with a declared I/O contract, a graph of reflective GammaDSP nodes, a public control surface, and optional automation.
