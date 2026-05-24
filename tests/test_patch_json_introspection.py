# test_patch_json_introspection.py
from __future__ import annotations

import numpy as np

from patch_json import (
    load_patch_dict,
    load_patch_json,
    make_graph_patch,
    save_patch_json,
)


def assert_audio_ok(name: str, audio: np.ndarray) -> None:
    assert isinstance(audio, np.ndarray), f"{name}: output is not ndarray"
    assert audio.dtype == np.float32, f"{name}: expected float32, got {audio.dtype}"
    assert len(audio) > 0, f"{name}: empty audio"
    assert np.all(np.isfinite(audio)), f"{name}: non-finite samples"

    peak = float(np.max(np.abs(audio)))
    print(f"{name}: peak={peak:.6f}, first10={audio[:10]}")
    assert peak > 1e-6, f"{name}: silent output"


def test_load_dict_serial() -> None:
    data = {
        "sample_rate": 48000,
        "duration": 0.25,
        "nodes": [
            {"id": "osc", "type": "Saw", "ctor_args": [110, 0.0]},
            {"id": "dc", "type": "BlockDC"},
            {"id": "lp", "type": "LowPassFilter", "params": {"freq": 1200, "res": 0.707}},
            {"id": "clip", "type": "SoftClip"},
            {
                "id": "delay",
                "type": "ModDelay",
                "ctor_args": [2.0, 0.25],
                "params": {"feedback": 0.2, "mix": 0.25},
            },
        ],
        "graph": {
            "name": "main",
            "connections": [
                ["osc", "dc"],
                ["dc", "lp"],
                ["lp", "clip"],
                ["clip", "delay"],
            ],
            "outputs": [["delay", 1.0]],
        },
        "out": "main",
    }

    patch = load_patch_dict(data)
    print(patch.debug())

    audio = patch.render()
    assert_audio_ok("load_dict_serial", audio)


def test_load_dict_graph_node_shorthand() -> None:
    data = {
        "sample_rate": 48000,
        "duration": 0.25,
        "nodes": [
            {"id": "osc", "type": "Saw", "ctor_args": [110, 0.0]},
            {"id": "lp", "type": "LowPassFilter", "params": {"freq": 900, "res": 0.707}},
            {
                "id": "delay",
                "type": "ModDelay",
                "ctor_args": [2.0, 0.25],
                "params": {"feedback": 0.25, "mix": 0.25},
            },
        ],
        "graph": {
            "name": "main",
            "nodes": ["osc", "lp", "delay"],
            "outputs": [["delay", 1.0]],
        },
        "out": "main",
    }

    patch = load_patch_dict(data)
    audio = patch.render()
    assert_audio_ok("load_dict_graph_node_shorthand", audio)


def test_load_dict_parallel() -> None:
    data = {
        "sample_rate": 48000,
        "duration": 0.25,
        "nodes": [
            {"id": "osc", "type": "Saw", "ctor_args": [90, 0.0]},
            {"id": "lp", "type": "LowPassFilter", "params": {"freq": 350, "res": 0.707}},
            {"id": "bp", "type": "BandPassFilter", "params": {"freq": 1200, "res": 6.0}},
            {"id": "hp", "type": "HighPassFilter", "params": {"freq": 3000, "res": 0.707}},
        ],
        "graph": {
            "name": "main",
            "normalize": True,
            "connections": [
                ["osc", "lp"],
                ["osc", "bp"],
                ["osc", "hp"],
            ],
            "outputs": [
                ["lp", 1.0],
                ["bp", 1.0],
                ["hp", 1.0],
            ],
        },
        "out": "main",
    }

    patch = load_patch_dict(data)
    print(patch.debug())

    audio = patch.render()
    assert_audio_ok("load_dict_parallel", audio)


def test_external_input_fx() -> None:
    data = {
        "sample_rate": 48000,
        "duration": 0.25,
        "nodes": [
            {"id": "lp", "type": "LowPassFilter", "params": {"freq": 800, "res": 0.707}},
            {"id": "clip", "type": "SoftClip"},
            {
                "id": "delay",
                "type": "ModDelay",
                "ctor_args": [2.0, 0.25],
                "params": {"feedback": 0.2, "mix": 0.25},
            },
        ],
        "graph": {
            "name": "fx",
            "connections": [
                ["input", "lp"],
                ["lp", "clip"],
                ["clip", "delay"],
            ],
            "outputs": [["delay", 1.0]],
        },
        "out": "fx",
    }

    patch = load_patch_dict(data)

    sr = 48000
    n = int(sr * 0.25)
    t = np.arange(n, dtype=np.float32) / sr
    x = (0.3 * np.sin(2.0 * np.pi * 110.0 * t)).astype(np.float32)

    audio = patch.process(x)
    assert_audio_ok("external_input_fx", audio)


def test_chain_materialized_as_graph() -> None:
    data = {
        "sample_rate": 48000,
        "duration": 0.25,
        "nodes": [
            {"id": "osc", "type": "Saw", "ctor_args": [55, 0.0]},
            {"id": "lp", "type": "LowPassFilter", "params": {"freq": 900, "res": 1.2}},
            {
                "id": "delay",
                "type": "ModDelay",
                "ctor_args": [2.0, 0.33],
                "params": {"feedback": 0.35, "mix": 0.3},
            },
        ],
        "chain": {
            "name": "main",
            "nodes": ["osc", "lp", "delay"],
        },
        "out": "main",
    }

    patch = load_patch_dict(data)
    audio = patch.render()
    assert_audio_ok("chain_materialized_as_graph", audio)


def test_save_roundtrip() -> None:
    data = make_graph_patch(
        sample_rate=48000,
        duration=0.25,
        nodes=[
            {"id": "osc", "type": "Saw", "ctor_args": [55, 0.0]},
            {"id": "lp", "type": "LowPassFilter", "params": {"freq": 900, "res": 1.2}},
            {
                "id": "delay",
                "type": "ModDelay",
                "ctor_args": [2.0, 0.33],
                "params": {"feedback": 0.35, "mix": 0.3},
            },
        ],
        connections=[
            ["osc", "lp"],
            ["lp", "delay"],
        ],
        outputs=[
            ["delay", 1.0],
        ],
        out="main",
    )

    path = "patches/generated_introspection_patch.json"
    save_patch_json(data, path)

    patch = load_patch_json(path)
    audio = patch.render()

    assert_audio_ok("save_roundtrip", audio)


def main() -> None:
    test_load_dict_serial()
    test_load_dict_graph_node_shorthand()
    test_load_dict_parallel()
    test_external_input_fx()
    test_chain_materialized_as_graph()
    test_save_roundtrip()

    print("\nALL INTROSPECTION JSON PATCH TESTS PASSED")


if __name__ == "__main__":
    main()
