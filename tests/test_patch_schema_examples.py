from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import numpy as np

from patch_json import load_patch_json

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples" / "patches"
SCHEMA = "gammadsp.patch.v0"


def assert_audio_ok(name: str, audio: np.ndarray) -> None:
    assert isinstance(audio, np.ndarray), f"{name}: output is not ndarray"
    assert audio.dtype == np.float32, f"{name}: expected float32, got {audio.dtype}"
    assert len(audio) > 0, f"{name}: empty audio"
    assert np.all(np.isfinite(audio)), f"{name}: non-finite samples"
    peak = float(np.max(np.abs(audio)))
    print(f"{name}: peak={peak:.6f}, first10={audio[:10]}")
    assert peak > 1e-6, f"{name}: silent output"


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_design_shape(path: Path, data: dict[str, Any]) -> None:
    assert data.get("schema") == SCHEMA, f"{path}: expected schema {SCHEMA!r}"
    assert data.get("name"), f"{path}: missing name"
    assert isinstance(data.get("description"), str), f"{path}: missing description"

    io = data.get("io")
    assert isinstance(io, dict), f"{path}: missing io object"
    assert io.get("mode") in {"generator", "processor"}, f"{path}: invalid io.mode"
    assert isinstance(io.get("inputs"), list), f"{path}: io.inputs must be a list"
    assert isinstance(io.get("outputs"), list), f"{path}: io.outputs must be a list"
    assert data.get("out") in io.get("outputs", []), f"{path}: out must be listed in io.outputs"

    nodes = data.get("nodes")
    assert isinstance(nodes, list) and nodes, f"{path}: nodes must be a non-empty list"
    ids: set[str] = set()
    for node in nodes:
        assert isinstance(node, dict), f"{path}: node must be an object"
        assert node.get("id"), f"{path}: node missing id"
        assert node.get("type"), f"{path}: node {node.get('id')!r} missing type"
        assert node["id"] not in ids, f"{path}: duplicate node id {node['id']!r}"
        assert node["id"] not in {"input", "in", "external", "$in"}, (
            f"{path}: external input names belong in graph connections, not nodes"
        )
        ids.add(str(node["id"]))
        if "params" in node:
            assert isinstance(node["params"], dict), f"{path}: node params must be an object"
        if "ctor_args" in node:
            assert isinstance(node["ctor_args"], list), f"{path}: ctor_args must be a list"

    controls = data.get("controls", [])
    assert isinstance(controls, list), f"{path}: controls must be a list"
    for control in controls:
        assert control.get("name"), f"{path}: control missing name"
        assert control.get("label"), f"{path}: control missing label"
        assert control.get("type") in {"float", "int", "bool", "enum"}, (
            f"{path}: unsupported control type {control.get('type')!r}"
        )
        targets = control.get("targets")
        assert isinstance(targets, list) and targets, f"{path}: control missing targets"
        for target in targets:
            assert target.get("node") in ids, f"{path}: control target references unknown node"
            assert target.get("param"), f"{path}: control target missing param"

    if io["mode"] == "generator":
        assert not io["inputs"], f"{path}: generator patches should not require inputs"
    if io["mode"] == "processor":
        assert "input" in io["inputs"], f"{path}: processor patch should declare input"
        graph = data.get("graph")
        assert isinstance(graph, dict), f"{path}: processor patch should use a graph"
        connections = graph.get("connections", [])
        assert any(conn and conn[0] == "input" for conn in connections), (
            f"{path}: processor graph should connect external input"
        )


def deterministic_input(sample_rate: int, duration: float) -> np.ndarray:
    n = max(1, int(sample_rate * duration))
    t = np.arange(n, dtype=np.float32) / float(sample_rate)
    return (0.3 * np.sin(2.0 * np.pi * 110.0 * t)).astype(np.float32)


def run_patch_example(path: Path, data: dict[str, Any]) -> None:
    patch = load_patch_json(path)
    mode = data["io"]["mode"]
    duration = float(data.get("duration", 0.25))
    if mode == "generator":
        audio = patch.render(duration=duration)
        assert_audio_ok(path.name, audio)
    elif mode == "processor":
        sample_rate = int(data.get("sample_rate", 48000))
        x = deterministic_input(sample_rate, duration)
        audio = patch.process(x)
        assert_audio_ok(path.name, audio)
    else:  # pragma: no cover - guarded by schema validation
        raise AssertionError(f"Unsupported mode: {mode}")


def test_patch_schema_examples() -> None:
    paths = sorted(EXAMPLES.glob("*.json"))
    assert paths, f"No patch examples found in {EXAMPLES}"
    for path in paths:
        data = load_json(path)
        validate_design_shape(path, data)
        run_patch_example(path, data)


def main() -> None:
    test_patch_schema_examples()
    print("\nALL PATCH SCHEMA EXAMPLES PASSED")


if __name__ == "__main__":
    main()
