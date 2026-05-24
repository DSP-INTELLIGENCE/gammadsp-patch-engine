# automation.py
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Iterable

import numpy as np
import GammaDSP as dsp


@dataclass(frozen=True)
class AutomationPoint:
    time: float
    value: float


@dataclass
class AutomationLane:
    node: str
    param: str
    type: str = "linear"
    points: list[AutomationPoint] | None = None

    def __post_init__(self) -> None:
        pts = self.points or []
        self.points = sorted(
            [p if isinstance(p, AutomationPoint) else AutomationPoint(float(p[0]), float(p[1])) for p in pts],
            key=lambda p: p.time,
        )

        lane_type = str(self.type).lower().strip()
        if lane_type not in {"linear", "step", "hold"}:
            raise ValueError(f"Unsupported automation type for {self.node}.{self.param}: {self.type!r}")
        self.type = lane_type

        if not self.points:
            raise ValueError(f"Automation lane {self.node}.{self.param} has no points")

    def value_at(self, time_seconds: float) -> float:
        t = float(time_seconds)
        pts = self.points or []

        if t <= pts[0].time:
            return float(pts[0].value)

        if t >= pts[-1].time:
            return float(pts[-1].value)

        prev = pts[0]
        for nxt in pts[1:]:
            if t <= nxt.time:
                if self.type in {"step", "hold"}:
                    return float(prev.value)

                span = max(nxt.time - prev.time, 1e-12)
                frac = (t - prev.time) / span
                return float(prev.value + frac * (nxt.value - prev.value))

            prev = nxt

        return float(pts[-1].value)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "AutomationLane":
        if "node" not in data:
            raise ValueError(f"Automation lane missing node: {data!r}")
        if "param" not in data:
            raise ValueError(f"Automation lane missing param: {data!r}")

        return cls(
            node=str(data["node"]),
            param=str(data["param"]),
            type=str(data.get("type", "linear")),
            points=[AutomationPoint(float(p[0]), float(p[1])) for p in data.get("points", [])],
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "node": self.node,
            "param": self.param,
            "type": self.type,
            "points": [[p.time, p.value] for p in self.points or []],
        }


@dataclass
class AutomationSet:
    lanes: list[AutomationLane]

    @classmethod
    def from_list(cls, items: Iterable[dict[str, Any] | AutomationLane] | None) -> "AutomationSet":
        lanes: list[AutomationLane] = []

        for item in items or []:
            if isinstance(item, AutomationLane):
                lanes.append(item)
            else:
                lanes.append(AutomationLane.from_dict(dict(item)))

        return cls(lanes=lanes)

    def __bool__(self) -> bool:
        return bool(self.lanes)

    def apply(self, engine: Any, time_seconds: float) -> None:
        for lane in self.lanes:
            value = lane.value_at(time_seconds)
            engine.set(lane.node, **{lane.param: value})

    def to_list(self) -> list[dict[str, Any]]:
        return [lane.to_dict() for lane in self.lanes]


def _is_graph_node(engine: Any, node_name: str) -> bool:
    node = engine.nodes[node_name]
    return "Graph" in node.module.__class__.__name__


def _render_exact(engine: Any, target: str, n: int) -> np.ndarray:
    if target not in engine.nodes:
        raise KeyError(f"Unknown render target: {target}")

    node = engine.nodes[target]
    out = np.zeros(int(n), dtype=np.float32)

    if _is_graph_node(engine, target):
        x = np.zeros(int(n), dtype=np.float32)
        dsp.runFunction(node.module, x, out, int(n))
        return out

    if node.kind == "generator":
        dsp.runGenerator(node.module, out, int(n))
        return out

    raise NotImplementedError(
        f"Target '{target}' is not renderable without input. "
        "Use process_with_automation(...) for function targets."
    )


def _process_exact(engine: Any, target: str, input_block: np.ndarray) -> np.ndarray:
    if target not in engine.nodes:
        raise KeyError(f"Unknown process target: {target}")

    x = np.ascontiguousarray(input_block, dtype=np.float32)
    out = np.zeros_like(x)
    node = engine.nodes[target]

    dsp.runFunction(node.module, x, out, len(x))
    return out


def render_with_automation(
    engine: Any,
    target: str,
    duration: float,
    automation: AutomationSet | list[dict[str, Any]] | None,
    block_size: int | None = None,
    normalize: bool = True,
) -> np.ndarray:
    auto = automation if isinstance(automation, AutomationSet) else AutomationSet.from_list(automation)
    block = int(block_size or getattr(engine, "block_size", 128) or 128)
    block = max(block, 1)

    sample_rate = int(getattr(engine, "sample_rate", 48000))
    total = int(sample_rate * float(duration))

    chunks: list[np.ndarray] = []
    pos = 0

    while pos < total:
        n = min(block, total - pos)
        t = pos / float(sample_rate)

        if auto:
            auto.apply(engine, t)

        chunks.append(_render_exact(engine, target, n))
        pos += n

    audio = np.concatenate(chunks).astype(np.float32) if chunks else np.zeros(0, dtype=np.float32)
    return engine.normalize(audio) if normalize else audio


def process_with_automation(
    engine: Any,
    target: str,
    input_buffer: np.ndarray,
    automation: AutomationSet | list[dict[str, Any]] | None,
    block_size: int | None = None,
    normalize: bool = True,
) -> np.ndarray:
    auto = automation if isinstance(automation, AutomationSet) else AutomationSet.from_list(automation)
    block = int(block_size or getattr(engine, "block_size", 128) or 128)
    block = max(block, 1)

    sample_rate = int(getattr(engine, "sample_rate", 48000))
    x = np.ascontiguousarray(input_buffer, dtype=np.float32)

    chunks: list[np.ndarray] = []
    pos = 0
    total = len(x)

    while pos < total:
        n = min(block, total - pos)
        t = pos / float(sample_rate)

        if auto:
            auto.apply(engine, t)

        chunks.append(_process_exact(engine, target, x[pos:pos + n]))
        pos += n

    audio = np.concatenate(chunks).astype(np.float32) if chunks else np.zeros(0, dtype=np.float32)
    return engine.normalize(audio) if normalize else audio
