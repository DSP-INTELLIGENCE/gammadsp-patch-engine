# patch_json.py
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np

from patch_engine import PatchEngine
from automation import AutomationSet, render_with_automation, process_with_automation


# ============================================================
# Path / parsing helpers
# ============================================================

def _as_path(path) -> Path:
    return path if isinstance(path, Path) else Path(path)


def _parse_connection(item) -> tuple[str, str, float]:
    """
    Accepts:
      ["osc", "lp"]
      ["osc", "lp", 0.5]
      {"src": "osc", "dst": "lp", "gain": 0.5}
    """
    if isinstance(item, dict):
        return (
            str(item["src"]),
            str(item["dst"]),
            float(item.get("gain", 1.0)),
        )

    if isinstance(item, (list, tuple)):
        if len(item) == 2:
            return str(item[0]), str(item[1]), 1.0
        if len(item) == 3:
            return str(item[0]), str(item[1]), float(item[2])

    raise ValueError(f"Invalid connection item: {item!r}")


def _parse_output(item) -> tuple[str, float]:
    """
    Accepts:
      "delay"
      ["delay", 0.8]
      {"node": "delay", "gain": 0.8}
    """
    if isinstance(item, str):
        return item, 1.0

    if isinstance(item, dict):
        return str(item["node"]), float(item.get("gain", 1.0))

    if isinstance(item, (list, tuple)):
        if len(item) == 1:
            return str(item[0]), 1.0
        if len(item) == 2:
            return str(item[0]), float(item[1])

    raise ValueError(f"Invalid output item: {item!r}")


def _normalize_graphs(data: dict[str, Any]) -> list[dict[str, Any]]:
    graphs: list[dict[str, Any]] = []

    if data.get("graph"):
        graph = dict(data["graph"])
        graph.setdefault("name", data.get("out", "main"))
        graphs.append(graph)

    raw = data.get("graphs")

    if raw:
        if isinstance(raw, dict):
            for name, graph_data in raw.items():
                graph = dict(graph_data)
                graph.setdefault("name", str(name))
                graphs.append(graph)

        elif isinstance(raw, list):
            for graph_data in raw:
                graphs.append(dict(graph_data))

        else:
            raise ValueError("'graphs' must be a list or object")

    return graphs


def _normalize_chains(data: dict[str, Any]) -> dict[str, list[str]]:
    chains: dict[str, list[str]] = {}

    raw = data.get("chains")

    if raw:
        if isinstance(raw, dict):
            for name, members in raw.items():
                chains[str(name)] = [str(x) for x in members]

        elif isinstance(raw, list):
            for item in raw:
                name = str(item["name"])
                members = item.get("nodes", item.get("members", []))
                chains[name] = [str(x) for x in members]

        else:
            raise ValueError("'chains' must be a list or object")

    if data.get("chain"):
        raw_chain = data["chain"]

        if isinstance(raw_chain, dict):
            name = str(raw_chain.get("name", data.get("out", "main")))
            members = raw_chain.get("nodes", raw_chain.get("members", []))
            chains[name] = [str(x) for x in members]

        else:
            name = str(data.get("out", "main"))
            chains[name] = [str(x) for x in raw_chain]

    return chains


def _normalize_blocks(data: dict[str, Any]) -> dict[str, list[str]]:
    blocks: dict[str, list[str]] = {}

    raw = data.get("blocks")

    if raw:
        if isinstance(raw, dict):
            for name, members in raw.items():
                blocks[str(name)] = [str(x) for x in members]

        elif isinstance(raw, list):
            for item in raw:
                name = str(item["name"])
                members = item.get("nodes", item.get("members", []))
                blocks[name] = [str(x) for x in members]

        else:
            raise ValueError("'blocks' must be a list or object")

    if data.get("block"):
        raw_block = data["block"]

        if isinstance(raw_block, dict):
            name = str(raw_block.get("name", "block"))
            members = raw_block.get("nodes", raw_block.get("members", []))
            blocks[name] = [str(x) for x in members]

    return blocks


def _graph_from_node_list(
    name: str,
    members: list[str],
    normalize: bool = False,
) -> dict[str, Any]:
    connections = [
        [src, dst, 1.0]
        for src, dst in zip(members, members[1:])
    ]

    outputs = []
    if members:
        outputs = [[members[-1], 1.0]]

    return {
        "name": name,
        "connections": connections,
        "outputs": outputs,
        "normalize": normalize,
    }


def _is_graph_node(engine: PatchEngine, node_name: str) -> bool:
    node = engine.nodes[node_name]
    cls_name = node.module.__class__.__name__
    return "Graph" in cls_name


# ============================================================
# Loaded JSON patch wrapper
# ============================================================

@dataclass
class LoadedJSONPatch:
    engine: PatchEngine
    data: dict[str, Any]
    output_name: str | None = None
    duration: float = 3.0
    automation: AutomationSet | None = None
    last_audio: np.ndarray | None = None

    def has_automation(self) -> bool:
        return bool(self.automation and self.automation.lanes)

    def render(
        self,
        target: str | None = None,
        duration: float | None = None,
    ) -> np.ndarray:
        target = target or self.output_name

        if target is None:
            raise ValueError("No output target. Add 'out': 'main' to the JSON patch.")

        if target not in self.engine.nodes:
            raise ValueError(f"Unknown render target: {target}")

        duration = float(self.duration if duration is None else duration)

        if self.has_automation():
            audio = render_with_automation(
                self.engine,
                target,
                duration,
                self.automation,
                block_size=self.engine.block_size,
                normalize=True,
            )
            self.last_audio = audio
            return self.last_audio

        node = self.engine.nodes[target]

        if _is_graph_node(self.engine, target):
            audio = self.engine.render_graph(target, duration)

        elif node.kind == "generator":
            audio = self.engine.render_generator(target, duration)

        else:
            raise NotImplementedError(
                f"Target '{target}' is not renderable without input. "
                f"Use process(input_buffer, target=...) for function targets."
            )

        self.last_audio = self.engine.normalize(audio)
        return self.last_audio

    def process(
        self,
        input_buffer: np.ndarray,
        target: str | None = None,
    ) -> np.ndarray:
        target = target or self.output_name

        if target is None:
            raise ValueError("No output target. Add 'out': 'main' to the JSON patch.")

        if target not in self.engine.nodes:
            raise ValueError(f"Unknown process target: {target}")

        if self.has_automation():
            audio = process_with_automation(
                self.engine,
                target,
                input_buffer,
                self.automation,
                block_size=self.engine.block_size,
                normalize=True,
            )
            self.last_audio = audio
            return self.last_audio

        if _is_graph_node(self.engine, target):
            audio = self.engine.process_graph(target, input_buffer)
        else:
            audio = self.engine.process(target, input_buffer)

        self.last_audio = self.engine.normalize(audio)
        return self.last_audio

    def play(self, audio: np.ndarray | None = None) -> None:
        audio = self.last_audio if audio is None else audio

        if audio is None:
            raise ValueError("Nothing rendered yet.")

        self.engine.play(audio)

    def save(self, path) -> None:
        save_patch_json(self.data, path)

    def debug(self) -> str:
        base = self.engine.debug_summary()
        if not self.has_automation():
            return base

        lines = [base, "", "Automation lanes:"]
        for lane in self.automation.lanes if self.automation else []:
            lines.append(f"  - {lane.node}.{lane.param} ({lane.type}) {len(lane.points or [])} point(s)")
        return "\n".join(lines)


# ============================================================
# Loader
# ============================================================

class PatchJSONLoader:
    def __init__(self, strict: bool = False):
        self.strict = bool(strict)

    def load_file(self, path) -> LoadedJSONPatch:
        path = _as_path(path)
        data = json.loads(path.read_text(encoding="utf-8"))
        return self.load_dict(data)

    def load_dict(self, data: dict[str, Any]) -> LoadedJSONPatch:
        sample_rate = int(data.get("sample_rate", data.get("sr", 48000)))
        block_size = int(data.get("block_size", 128))
        duration = float(data.get("duration", data.get("dur", 3.0)))

        engine = PatchEngine(
            sample_rate=sample_rate,
            block_size=block_size,
            strict=self.strict,
        )

        for item in data.get("nodes", []):
            node_id = item.get("id", item.get("name"))
            module_type = item.get("type", item.get("module"))

            if not node_id:
                raise ValueError(f"Node missing id/name: {item!r}")

            if not module_type:
                raise ValueError(f"Node missing type/module: {item!r}")

            ctor_args = list(item.get("ctor_args", item.get("args", [])))
            params = dict(item.get("params", {}))

            reserved = {
                "id",
                "name",
                "type",
                "module",
                "ctor_args",
                "args",
                "params",
                "kind",
            }

            for key, value in item.items():
                if key not in reserved:
                    params.setdefault(key, value)

            kind = item.get("kind")

            engine.create(
                str(node_id),
                str(module_type),
                kind=kind,
                ctor_args=ctor_args,
                **params,
            )

        for name, members in _normalize_blocks(data).items():
            engine.make_block(name, members)

        for name, members in _normalize_chains(data).items():
            if not members:
                continue

            first = members[0]

            if first not in engine.nodes:
                raise ValueError(f"Unknown first node in chain '{name}': {first}")

            first_node = engine.nodes[first]

            if first_node.kind == "generator":
                if len(members) == 1:
                    continue

                graph_data = _graph_from_node_list(name, members)
                connections = [_parse_connection(x) for x in graph_data["connections"]]
                outputs = [_parse_output(x) for x in graph_data["outputs"]]

                engine.make_graph(
                    name,
                    connections=connections,
                    outputs=outputs,
                    normalize=False,
                )

            else:
                engine.make_chain(name, members)

        for graph_data in _normalize_graphs(data):
            name = str(graph_data.get("name", data.get("out", "main")))

            connections = [
                _parse_connection(x)
                for x in graph_data.get("connections", [])
            ]

            if not connections and "nodes" in graph_data:
                members = [str(x) for x in graph_data["nodes"]]
                connections = [
                    (src, dst, 1.0)
                    for src, dst in zip(members, members[1:])
                ]

            outputs = [
                _parse_output(x)
                for x in graph_data.get("outputs", [])
            ]

            if not outputs and "output" in graph_data:
                outputs = [_parse_output(graph_data["output"])]

            normalize = bool(graph_data.get("normalize", False))

            engine.make_graph(
                name,
                connections=connections,
                outputs=outputs,
                normalize=normalize,
            )

        output_name = data.get("out", data.get("output"))

        if output_name is None:
            graphs = _normalize_graphs(data)
            chains = _normalize_chains(data)

            if graphs:
                output_name = graphs[-1].get("name", "main")
            elif chains:
                output_name = list(chains.keys())[-1]
            elif data.get("nodes"):
                output_name = data["nodes"][0].get("id", data["nodes"][0].get("name"))

        automation = AutomationSet.from_list(data.get("automation", []))

        return LoadedJSONPatch(
            engine=engine,
            data=data,
            output_name=output_name,
            duration=duration,
            automation=automation,
        )


# ============================================================
# Public API
# ============================================================

def load_patch_json(path, strict: bool = False) -> LoadedJSONPatch:
    return PatchJSONLoader(strict=strict).load_file(path)


def load_patch_dict(data: dict[str, Any], strict: bool = False) -> LoadedJSONPatch:
    return PatchJSONLoader(strict=strict).load_dict(data)


def save_patch_json(data: dict[str, Any], path, indent: int = 2) -> None:
    path = _as_path(path)
    path.parent.mkdir(parents=True, exist_ok=True)

    path.write_text(
        json.dumps(data, indent=indent, sort_keys=False),
        encoding="utf-8",
    )


def make_graph_patch(
    nodes: list[dict[str, Any]],
    connections: list,
    outputs: list | None = None,
    out: str = "main",
    sample_rate: int = 48000,
    duration: float = 3.0,
    normalize: bool = False,
    automation: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    data = {
        "sample_rate": sample_rate,
        "duration": duration,
        "nodes": nodes,
        "graph": {
            "name": out,
            "normalize": normalize,
            "connections": connections,
            "outputs": outputs or [],
        },
        "out": out,
    }

    if automation:
        data["automation"] = automation

    return data
