# patch_dsl.py
from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Any

import numpy as np
import sounddevice as sd

from patch_engine import PatchEngine


def strip_comment(line: str) -> str:
    return line.split("#", 1)[0].strip()


def parse_value(text: str) -> Any:
    text = text.strip()

    if text.lower() in {"true", "yes", "on"}:
        return True

    if text.lower() in {"false", "no", "off"}:
        return False

    try:
        if "." in text or "e" in text.lower():
            return float(text)
        return int(text)
    except ValueError:
        return text


def parse_kv(tokens: list[str]) -> dict[str, Any]:
    out = {}

    for tok in tokens:
        if "=" not in tok:
            continue

        key, value = tok.split("=", 1)
        out[key.strip()] = parse_value(value.strip())

    return out


def split_chain(expr: str) -> list[str]:
    return [x.strip() for x in expr.split("->") if x.strip()]


def split_block(expr: str) -> list[str]:
    return [x.strip() for x in re.split(r"[|,+]", expr) if x.strip()]


@dataclass
class RenderSpec:
    mode: str
    args: tuple

@dataclass
class GraphDef:
    name: str
    connections: list[tuple[str, str, float]]
    outputs: list[tuple[str, float]]
    normalize: bool = False


class PatchDSL:
    def __init__(self, sample_rate: int = 48000, block_size: int = 128, strict: bool = False):
        self.sample_rate = int(sample_rate)
        self.block_size = int(block_size)
        self.duration = 3.0
        self.strict = strict

        self.engine = PatchEngine(
            sample_rate=self.sample_rate,
            block_size=self.block_size,
            strict=self.strict,
        )

        self.block_defs: dict[str, list[str]] = {}
        self.chain_defs: dict[str, list[str]] = {}
        self.render_specs: dict[str, RenderSpec] = {}

        self.output_name: str | None = None
        self.last_audio: np.ndarray | None = None

        self.graph_defs: dict[str, GraphDef] = {}
        self.current_graph: str | None = None

    def load_text(self, text: str) -> "PatchDSL":
        for raw in text.splitlines():
            line = strip_comment(raw)

            if not line:
                continue

            self.parse_line(line)

        self.build_blocks()
        self.build_graphs()
        self.build_chains()

        return self

    def parse_line(self, line: str) -> None:
        parts = line.split()

        if not parts:
            return

        cmd = parts[0].lower()

        if cmd == "sr":
            self.sample_rate = int(parse_value(parts[1]))
            self.engine = PatchEngine(
                sample_rate=self.sample_rate,
                block_size=self.block_size,
                strict=self.strict,
            )
            return

        if cmd == "dur":
            self.duration = float(parse_value(parts[1]))
            return

        if cmd == "node":
            self.parse_node(parts)
            return

        if cmd == "set":
            self.parse_set(parts)
            return

        if cmd == "block":
            self.parse_block(line)
            return

        if cmd == "chain":
            self.parse_chain(line)
            return

        if cmd == "out":
            self.output_name = parts[1]
            return

        if cmd == "graph":
            self.parse_graph(line)
            return

        if cmd == "connect":
            self.parse_connect(line)
            return

        if cmd in {"graphout", "output"}:
            self.parse_graphout(line)
            return
            
        raise ValueError(f"Unknown DSL command: {cmd}")

    def parse_node(self, parts: list[str]) -> None:
        if len(parts) < 3:
            raise ValueError("node syntax: node <name> <type> [ctor_args...] key=value ...")

        name = parts[1]
        module_type = parts[2]

        ctor_args = []
        params = {}

        for tok in parts[3:]:
            if "=" in tok:
                key, value = tok.split("=", 1)
                params[key.strip()] = parse_value(value.strip())
            else:
                ctor_args.append(parse_value(tok))

        self.engine.create(name, module_type, ctor_args=ctor_args, **params)
        
    def parse_set(self, parts: list[str]) -> None:
        if len(parts) < 3:
            raise ValueError("set syntax: set <node> key=value ...")

        name = parts[1]
        params = parse_kv(parts[2:])

        self.engine.set(name, **params)

    def parse_block(self, line: str) -> None:
        # block filters lp | bp | hp
        rest = line[len("block"):].strip()
        name, expr = rest.split(None, 1)

        self.block_defs[name] = split_block(expr)

    def parse_chain(self, line: str) -> None:
        # chain main osc -> dc -> lp -> clip -> delay
        rest = line[len("chain"):].strip()
        name, expr = rest.split(None, 1)

        self.chain_defs[name] = split_chain(expr)

    def build_blocks(self) -> None:
        for name, members in self.block_defs.items():
            if name in self.engine.nodes:
                continue

            self.engine.make_block(name, members)

    def build_chains(self) -> None:
        for chain_name, members in self.chain_defs.items():
            if not members:
                continue

            first = members[0]

            if first not in self.engine.nodes:
                raise ValueError(f"Unknown node in chain '{chain_name}': {first}")

            first_node = self.engine.nodes[first]

            # generator -> function chain
            if first_node.kind == "generator":
                fx_members = members[1:]

                if not fx_members:
                    self.render_specs[chain_name] = RenderSpec("generator", (first,))
                    continue

                fx_chain_name = f"__fx_{chain_name}"
                self.engine.make_chain(fx_chain_name, fx_members)

                self.render_specs[chain_name] = RenderSpec(
                    "generator_through",
                    (first, fx_chain_name),
                )

            # function-only chain
            else:
                self.engine.make_chain(chain_name, members)
                self.render_specs[chain_name] = RenderSpec("function", (chain_name,))

    def render(self, target: str | None = None, duration: float | None = None) -> np.ndarray:
        target = target or self.output_name

        if target is None:
            raise ValueError("No output target. Use: out <chain_name>")

        if target not in self.render_specs:
            raise ValueError(f"Unknown output target: {target}")

        duration = float(self.duration if duration is None else duration)
        spec = self.render_specs[target]

        if spec.mode == "generator":
            gen_name = spec.args[0]
            audio = self.engine.render_generator(gen_name, duration)

        elif spec.mode == "generator_through":
            gen_name, fx_name = spec.args
            audio = self.engine.render_generator_through(gen_name, fx_name, duration)

        elif spec.mode == "graph":
            graph_name = spec.args[0]
            audio = self.engine.render_graph(graph_name, duration)

        else:
            raise NotImplementedError(
                "Function-only chains need an input buffer. Use a generator or graph as the output target."
            )
        

        self.last_audio = self.engine.normalize(audio)
        return self.last_audio

    def ensure_graph_def(self, name: str) -> GraphDef:
        if name not in self.graph_defs:
            self.graph_defs[name] = GraphDef(
                name=name,
                connections=[],
                outputs=[],
                normalize=False,
            )
        return self.graph_defs[name]

    def parse_graph(self, line: str) -> None:
        # Forms:
        #   graph main
        #   graph main osc -> lp -> clip -> delay
        #   graph main normalize=true osc -> lp -> delay
        rest = line[len("graph"):].strip()

        if not rest:
            raise ValueError("graph syntax: graph <name> [node -> node -> node]")

        pieces = rest.split()
        name = pieces[0]
        self.current_graph = name

        g = self.ensure_graph_def(name)

        expr = rest[len(name):].strip()

        if not expr:
            return

        # Optional normalize=true before expression.
        expr_parts = expr.split()
        kv = parse_kv(expr_parts)

        if "normalize" in kv:
            g.normalize = bool(kv["normalize"])
            # Remove normalize=true token from expression.
            expr = " ".join(p for p in expr_parts if not p.startswith("normalize="))

        if "->" in expr:
            chain = split_chain(expr)

            for src, dst in zip(chain, chain[1:]):
                g.connections.append((src, dst, 1.0))

            if chain:
                g.outputs = [(chain[-1], 1.0)]

    def parse_connect(self, line: str) -> None:
        # Forms:
        #   connect osc -> lp
        #   connect osc -> lp gain=0.5
        #   connect main osc -> lp gain=0.5
        rest = line[len("connect"):].strip()

        if "->" not in rest:
            raise ValueError("connect syntax: connect [graph] <src> -> <dst> [gain=...]")

        lhs, rhs = rest.split("->", 1)
        lhs_tokens = lhs.split()

        if len(lhs_tokens) == 2:
            graph_name = lhs_tokens[0]
            src = lhs_tokens[1]
        else:
            if self.current_graph is None:
                raise ValueError("No current graph. Use: graph <name> before connect.")
            graph_name = self.current_graph
            src = lhs.strip()

        rhs_tokens = rhs.split()
        if not rhs_tokens:
            raise ValueError("connect missing destination node")

        dst = rhs_tokens[0]
        params = parse_kv(rhs_tokens[1:])
        gain = float(params.get("gain", 1.0))

        g = self.ensure_graph_def(graph_name)
        g.connections.append((src, dst, gain))

    def parse_graphout(self, line: str) -> None:
        # Forms:
        #   graphout delay
        #   graphout delay gain=0.5
        #   graphout main delay gain=0.5
        #   output delay
        rest = line.split(None, 1)[1].strip()
        tokens = rest.split()

        if not tokens:
            raise ValueError("graphout syntax: graphout [graph] <node> [gain=...]")

        if len(tokens) >= 2 and tokens[0] in self.graph_defs:
            graph_name = tokens[0]
            node_name = tokens[1]
            param_tokens = tokens[2:]
        else:
            if self.current_graph is None:
                raise ValueError("No current graph. Use: graph <name> before graphout/output.")
            graph_name = self.current_graph
            node_name = tokens[0]
            param_tokens = tokens[1:]

        params = parse_kv(param_tokens)
        gain = float(params.get("gain", 1.0))

        g = self.ensure_graph_def(graph_name)
        g.outputs.append((node_name, gain))

    def build_graphs(self) -> None:
        for name, g in self.graph_defs.items():
            if name in self.engine.nodes:
                continue

            self.engine.make_graph(
                name,
                connections=g.connections,
                outputs=g.outputs,
                normalize=g.normalize,
            )

            self.render_specs[name] = RenderSpec("graph", (name,))
    
    def debug(self) -> str:
        return self.engine.debug_summary()
        
    def process(self, input_buffer: np.ndarray, target: str | None = None) -> np.ndarray:
        target = target or self.output_name

        if target is None:
            raise ValueError("No output target. Use: out <name>")

        if target not in self.render_specs:
            raise ValueError(f"Unknown output target: {target}")

        spec = self.render_specs[target]

        if spec.mode != "graph":
            raise NotImplementedError("External input processing is currently graph-only.")

        graph_name = spec.args[0]
        audio = self.engine.process_graph(graph_name, input_buffer)

        self.last_audio = self.engine.normalize(audio)
        return self.last_audio
        
    def play(self, audio: np.ndarray | None = None) -> None:
        audio = self.last_audio if audio is None else audio

        if audio is None:
            raise ValueError("Nothing rendered yet.")

        self.engine.play(audio)