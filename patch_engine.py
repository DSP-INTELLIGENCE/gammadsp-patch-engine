# patch_engine.py
from __future__ import annotations

from typing import Any

import numpy as np
import sounddevice as sd
import GammaDSP as dsp

from swig_introspect import SWIGResolver


_SYSTEM_PROMPT_PREAMBLE = """You are the GammaDSP patch-building agent.

Work only against the active GammaDSP scaffold runtime. The active runtime is the
SWIG-introspection patch compiler made of swig_introspect.py, patch_engine.py,
patch_dsl.py, patch_json.py, and automation.py.

Core rules:
- Use live SWIG introspection to discover classes and setters.
- Do not use Python metadata overlays, static registries, generated factories, or
  module_overlays.py for active patch compilation.
- Create modules with positional constructor arguments through ctor_args.
- Apply named parameters through dynamic setter calls.
- Build explicit feed-forward graph topology with FunctionGraph/DSPGraph.
- Render FunctionGraph/DSPGraph through runFunction, not runGenerator.
- Use runGenerator, runFunction, and runFunctionInPlace as canonical SWIG block
  helpers.
- Keep Python references alive for every C++ object used by chains, blocks, and
  graphs.
- Patch only active runtime files unless a task explicitly targets a legacy branch.

JSON patch contract:
- nodes: list of {id/name, type/module, ctor_args/args, params, kind?}
- graph/graphs: explicit connections and outputs
- chain/chains: generator-led chains materialize as graphs; function-only chains
  materialize as FunctionChain
- block/blocks: function blocks materialize as FunctionBlock
- out/output: named render/process target
- automation: block-rate parameter lanes evaluated before each block

When generating code, prefer small, reversible scaffold patches with tests.
"""


class PatchNode:
    def __init__(self, name: str, module: Any, kind: str, category: str):
        self.name = name
        self.module = module
        self.kind = kind
        self.category = category


class PatchEngine:
    def __init__(self, sample_rate: int = 48000, block_size: int = 128, strict: bool = False):
        self.sample_rate = int(sample_rate)
        self.block_size = int(block_size)
        self.strict = bool(strict)

        dsp.set_samplerate(self.sample_rate)

        self.resolver = SWIGResolver()

        # Own all Python references so C++ graph/chain raw pointers stay alive.
        self.nodes: dict[str, PatchNode] = {}
        self.refs: list[Any] = []

    # ============================================================
    # Ownership / node creation
    # ============================================================

    def keep(self, obj: Any) -> Any:
        self.refs.append(obj)
        return obj

    def _make_node(
        self,
        name: str,
        module: Any,
        kind: str | None = None,
        category: str | None = None,
    ) -> PatchNode:
        if kind is None:
            kind = self.resolver.infer_kind(module)

        if category is None:
            category = self.resolver.infer_category(module)

        node = PatchNode(name, module, kind, category)
        self.nodes[name] = node
        self.keep(module)
        return node

    def create(
        self,
        name: str,
        module_type: str,
        kind: str | None = None,
        ctor_args: list[Any] | tuple[Any, ...] | None = None,
        **params: Any,
    ) -> Any:
        """
        Create a GammaDSP object by live SWIG introspection.

        Positional constructor arguments go through ctor_args:

            engine.create("osc", "Saw", ctor_args=[110, 0.0])

        key=value params are applied as setter calls:

            engine.create("lp", "LowPassFilter", freq=1200, res=0.707)
        """
        ctor_args = list(ctor_args or [])

        module = self.resolver.construct(module_type, ctor_args)
        self.resolver.apply_params(module, params)

        self._make_node(name, module, kind=kind)

        return module

    def get(self, name: str) -> Any:
        if name not in self.nodes:
            raise KeyError(f"Unknown node: {name}")

        return self.nodes[name].module

    def set(self, node_name: str, **params: Any) -> None:
        if node_name not in self.nodes:
            raise KeyError(f"Unknown node: {node_name}")

        node = self.nodes[node_name]
        self.resolver.apply_params(node.module, params)

    # ============================================================
    # Chain / block construction
    # ============================================================

    def make_chain(self, name: str, node_names: list[str]) -> Any:
        errors = self.validate_node_chain(node_names)
        if errors:
            raise ValueError("Invalid chain:\n" + "\n".join(f"  - {e}" for e in errors))

        chain = self.resolver.construct("FunctionChain")

        for node_name in node_names:
            chain.addFilter(self.nodes[node_name].module)

        self._make_node(name, chain, kind="function", category="routing")

        return chain

    def make_block(self, name: str, node_names: list[str]) -> Any:
        errors = self.validate_node_chain(node_names)
        if errors:
            raise ValueError("Invalid block:\n" + "\n".join(f"  - {e}" for e in errors))

        block = self.resolver.construct("FunctionBlock")

        for node_name in node_names:
            block.addFilter(self.nodes[node_name].module)

        self._make_node(name, block, kind="function", category="routing")

        return block

    # ============================================================
    # Render helpers
    # ============================================================

    def render_generator(self, node_name: str, duration: float) -> np.ndarray:
        if node_name not in self.nodes:
            raise KeyError(f"Unknown node: {node_name}")

        node = self.nodes[node_name]

        n = int(self.sample_rate * float(duration))
        out = np.zeros(n, dtype=np.float32)

        dsp.runGenerator(node.module, out, n)

        return out

    def process(self, node_name: str, input_buffer: np.ndarray) -> np.ndarray:
        if node_name not in self.nodes:
            raise KeyError(f"Unknown node: {node_name}")

        node = self.nodes[node_name]

        x = np.ascontiguousarray(input_buffer, dtype=np.float32)
        out = np.zeros_like(x, dtype=np.float32)

        dsp.runFunction(node.module, x, out, len(x))

        return out

    def render_generator_through(self, gen_name: str, fx_name: str, duration: float) -> np.ndarray:
        x = self.render_generator(gen_name, duration)
        return self.process(fx_name, x)

    def normalize(self, x: np.ndarray, target: float = 0.95) -> np.ndarray:
        x = np.asarray(x, dtype=np.float32)

        peak = float(np.max(np.abs(x))) if len(x) else 0.0

        if peak > 1e-9:
            return x * (float(target) / peak)

        return x

    def play(self, x: np.ndarray) -> None:
        x = np.asarray(x, dtype=np.float32)
        sd.play(x, self.sample_rate)
        sd.wait()

    # ============================================================
    # Graph construction
    # ============================================================

    def _is_external_input_name(self, name: str) -> bool:
        return str(name).lower() in {"input", "in", "external", "$in"}

    def _find_graph_type(self) -> str:
        classes = set(self.resolver.list_classes())

        if "FunctionGraph" in classes:
            return "FunctionGraph"

        if "DSPGraph" in classes:
            return "DSPGraph"

        raise ValueError("No graph module found. Expected FunctionGraph or DSPGraph.")

    def _toposort_graph_nodes(
        self,
        connections: list[tuple[str, str, float]],
        outputs: list[tuple[str, float]],
    ) -> list[str]:
        """
        Return feed-forward node order for FunctionGraph/DSPGraph construction.
        """
        order_seen: list[str] = []
        seen: set[str] = set()

        def add_seen(node_name: str) -> None:
            if self._is_external_input_name(node_name):
                return

            if node_name not in seen:
                seen.add(node_name)
                order_seen.append(node_name)

        for src, dst, _gain in connections:
            add_seen(src)
            add_seen(dst)

        for node_name, _gain in outputs:
            add_seen(node_name)

        indeg = {node_name: 0 for node_name in order_seen}
        adj = {node_name: [] for node_name in order_seen}

        for src, dst, _gain in connections:
            if self._is_external_input_name(src):
                continue

            if src not in indeg or dst not in indeg:
                continue

            adj[src].append(dst)
            indeg[dst] += 1

        queue = [node_name for node_name in order_seen if indeg[node_name] == 0]
        result: list[str] = []

        while queue:
            node_name = queue.pop(0)
            result.append(node_name)

            for dst in adj[node_name]:
                indeg[dst] -= 1
                if indeg[dst] == 0:
                    queue.append(dst)

        if len(result) != len(order_seen):
            raise ValueError(
                "Graph has a cycle or non-feed-forward dependency. "
                "FunctionGraph/DSPGraph v0 needs feed-forward order."
            )

        return result

    def make_graph(
        self,
        name: str,
        connections: list[tuple[str, str, float]],
        outputs: list[tuple[str, float]] | None = None,
        normalize: bool = False,
    ) -> Any:
        """
        Build a C++ FunctionGraph/DSPGraph from Python node names.

        connections:
            [("osc", "lp", 1.0), ("lp", "delay", 1.0)]

        external input source:
            [("input", "lp", 1.0)]

        outputs:
            [("delay", 1.0)]
        """
        outputs = outputs or []

        errors = self.validate_graph_connections(connections, outputs)
        if errors:
            raise ValueError("Invalid graph:\n" + "\n".join(f"  - {e}" for e in errors))

        graph_type = self._find_graph_type()
        graph = self.resolver.construct(graph_type)

        node_order = self._toposort_graph_nodes(connections, outputs)
        node_indices: dict[str, int] = {}

        for node_name in node_order:
            if node_name not in self.nodes:
                raise ValueError(f"Unknown graph node: {node_name}")

            patch_node = self.nodes[node_name]
            module = patch_node.module

            if patch_node.kind == "generator" and hasattr(graph, "addGenerator"):
                idx = graph.addGenerator(module)
            elif patch_node.kind == "function" and hasattr(graph, "addFunction"):
                idx = graph.addFunction(module)
            elif hasattr(graph, "addNode"):
                idx = graph.addNode(module)
            elif hasattr(graph, "addFunction"):
                idx = graph.addFunction(module)
            else:
                raise AttributeError(
                    f"{graph_type} has no addGenerator/addFunction/addNode method"
                )

            if idx is None:
                raise RuntimeError(f"Graph add method returned None for node: {node_name}")

            if int(idx) < 0:
                raise RuntimeError(f"Failed to add node to graph: {node_name}")

            node_indices[node_name] = int(idx)

        for src, dst, gain in connections:
            if dst not in node_indices:
                raise ValueError(f"Unknown graph destination node: {dst}")

            if self._is_external_input_name(src):
                if not hasattr(graph, "connectInput"):
                    raise AttributeError(f"{graph_type} has no connectInput method")

                ok = graph.connectInput(node_indices[dst], float(gain))
            else:
                if src not in node_indices:
                    raise ValueError(f"Unknown graph source node: {src}")

                if not hasattr(graph, "connect"):
                    raise AttributeError(f"{graph_type} has no connect method")

                ok = graph.connect(node_indices[src], node_indices[dst], float(gain))

            if ok is False:
                raise RuntimeError(f"Failed graph connection: {src} -> {dst}")

        if outputs:
            first = True

            for out_name, gain in outputs:
                if out_name not in node_indices:
                    raise ValueError(f"Unknown graph output node: {out_name}")

                if first and hasattr(graph, "setOutput"):
                    ok = graph.setOutput(node_indices[out_name], float(gain))
                    first = False
                elif hasattr(graph, "addOutput"):
                    ok = graph.addOutput(node_indices[out_name], float(gain))
                elif hasattr(graph, "setOutput"):
                    ok = graph.setOutput(node_indices[out_name], float(gain))
                else:
                    raise AttributeError(f"{graph_type} has no output method")

                if ok is False:
                    raise RuntimeError(f"Failed to add graph output: {out_name}")
        else:
            if connections:
                last_dst = connections[-1][1]
                if last_dst in node_indices and hasattr(graph, "setOutput"):
                    graph.setOutput(node_indices[last_dst], 1.0)

        if hasattr(graph, "setNormalizeOutputs"):
            graph.setNormalizeOutputs(bool(normalize))

        self._make_node(name, graph, kind="function", category="routing")

        return graph

    def render_graph(self, graph_name: str, duration: float) -> np.ndarray:
        if graph_name not in self.nodes:
            raise KeyError(f"Unknown graph node: {graph_name}")

        n = int(self.sample_rate * float(duration))

        # FunctionGraph is exposed as Function*, not Generator*.
        # For generator-only graphs, feed silence as the external input.
        x = np.zeros(n, dtype=np.float32)
        out = np.zeros(n, dtype=np.float32)

        graph = self.nodes[graph_name].module
        dsp.runFunction(graph, x, out, n)

        return out
        
    def process_graph(self, graph_name: str, input_buffer: np.ndarray) -> np.ndarray:
        if graph_name not in self.nodes:
            raise KeyError(f"Unknown graph node: {graph_name}")

        x = np.ascontiguousarray(input_buffer, dtype=np.float32)
        out = np.zeros_like(x, dtype=np.float32)

        graph = self.nodes[graph_name].module
        dsp.runFunction(graph, x, out, len(x))

        return out
        
    # ============================================================
    # Debug / introspection API
    # ============================================================

    def list_modules(self) -> list[str]:
        return self.resolver.list_classes()

    def module_exists(self, module_type: str) -> bool:
        try:
            self.resolver.resolve_class(module_type)
            return True
        except Exception:
            return False

    def describe(self, module_type: str) -> str:
        cls = self.resolver.resolve_class(module_type)

        methods = []
        for method_name in dir(cls):
            if method_name.startswith("_"):
                continue

            attr = getattr(cls, method_name, None)
            if callable(attr):
                methods.append(method_name)

        lines = [
            f"Class: {cls.__name__}",
            "",
            "Callable methods:",
        ]

        for method_name in sorted(methods):
            lines.append(f"  - {method_name}")

        return "\n".join(lines)

    def describe_module(self, module_type: str) -> str:
        return self.describe(module_type)

    def print_module(self, module_type: str) -> None:
        print(self.describe_module(module_type))

    def list_nodes(self, kind: str | None = None, category: str | None = None) -> list[str]:
        names = []

        for name, node in self.nodes.items():
            if kind is not None and node.kind != kind:
                continue

            if category is not None and node.category != category:
                continue

            names.append(name)

        return sorted(names)

    def node_exists(self, node_name: str) -> bool:
        return node_name in self.nodes

    def describe_node(self, node_name: str) -> str:
        if node_name not in self.nodes:
            raise KeyError(f"Unknown node: {node_name}")

        node = self.nodes[node_name]

        methods = []
        for method_name in dir(node.module):
            if method_name.startswith("_"):
                continue

            attr = getattr(node.module, method_name, None)
            if callable(attr):
                methods.append(method_name)

        lines = [
            f"Node: {node.name}",
            f"  class:    {node.module.__class__.__name__}",
            f"  kind:     {node.kind}",
            f"  category: {node.category}",
            f"  object:   {node.module}",
            "",
            "  callable methods:",
        ]

        for method_name in sorted(methods):
            lines.append(f"    - {method_name}")

        return "\n".join(lines)

    def print_node(self, node_name: str) -> None:
        print(self.describe_node(node_name))

    def describe_all_nodes(self) -> str:
        lines = []

        for name in self.list_nodes():
            lines.append(self.describe_node(name))
            lines.append("")

        return "\n".join(lines).rstrip()

    def print_nodes(self) -> None:
        print(self.describe_all_nodes())

    def get_node_info(self, node_name: str) -> dict[str, Any]:
        if node_name not in self.nodes:
            raise KeyError(f"Unknown node: {node_name}")

        node = self.nodes[node_name]

        return {
            "name": node.name,
            "class": node.module.__class__.__name__,
            "kind": node.kind,
            "category": node.category,
            "methods": [
                method_name
                for method_name in dir(node.module)
                if not method_name.startswith("_")
                and callable(getattr(node.module, method_name, None))
            ],
        }

    def get_module_info(self, module_type: str) -> dict[str, Any]:
        cls = self.resolver.resolve_class(module_type)

        return {
            "class": cls.__name__,
            "methods": [
                method_name
                for method_name in dir(cls)
                if not method_name.startswith("_")
            ],
        }

    def debug_summary(self) -> str:
        lines = []
        lines.append("GammaDSP PatchEngine")
        lines.append(f"  sample_rate: {self.sample_rate}")
        lines.append(f"  block_size:  {self.block_size}")
        lines.append(f"  nodes:       {len(self.nodes)}")
        lines.append(f"  refs:        {len(self.refs)}")
        lines.append("")
        lines.append("Node list:")

        if not self.nodes:
            lines.append("  <none>")
        else:
            for name in sorted(self.nodes):
                node = self.nodes[name]
                lines.append(
                    f"  - {name}: {node.module.__class__.__name__} "
                    f"({node.kind}, {node.category})"
                )

        return "\n".join(lines)

    def print_debug_summary(self) -> None:
        print(self.debug_summary())

    # ============================================================
    # System prompt / runtime contract
    # ============================================================

    def runtime_contract(self, max_modules: int | None = 160) -> dict[str, Any]:
        """
        Return the active scaffold contract from live SWIG introspection.

        This intentionally does not consult module_overlays.py, registry.py,
        dynamic_registry.py, or generated factories. The returned data is a
        compact machine-readable companion to system_prompt().
        """
        all_classes = self.resolver.list_classes()
        classes = all_classes
        if max_modules is not None:
            classes = all_classes[: max(0, int(max_modules))]

        graph_types = [name for name in all_classes if name in {"FunctionGraph", "DSPGraph"}]

        return {
            "runtime": "GammaDSP SWIG introspection PatchEngine",
            "sample_rate": self.sample_rate,
            "block_size": self.block_size,
            "strict": self.strict,
            "active_files": [
                "swig_introspect.py",
                "patch_engine.py",
                "patch_dsl.py",
                "patch_json.py",
                "automation.py",
            ],
            "forbidden_runtime_dependencies": [
                "module_overlays.py",
                "dynamic_registry.py",
                "dynamic_factory.py",
                "metadata.py",
                "registry.py",
                "registry_autogen.py",
                "factory.py",
            ],
            "graph_types": graph_types,
            "module_count": len(all_classes),
            "modules": classes,
        }

    def system_prompt(
        self,
        max_modules: int | None = 160,
        include_methods: bool = False,
    ) -> str:
        """
        Build a no-overlay GammaDSP system prompt from the live scaffold.

        The prompt is suitable for code-generation agents that need to write
        JSON patches, DSL patches, or small scaffold changes against the active
        PatchEngine runtime. Class names come from SWIGResolver.list_classes().
        Optional method listings also use direct Python/SWIG introspection.
        """
        classes = self.resolver.list_classes()
        visible_classes = classes
        if max_modules is not None:
            visible_classes = classes[: max(0, int(max_modules))]

        lines: list[str] = [_SYSTEM_PROMPT_PREAMBLE.rstrip(), ""]
        lines.append("Active runtime facts:")
        lines.append(f"- sample_rate: {self.sample_rate}")
        lines.append(f"- block_size: {self.block_size}")
        lines.append(f"- strict: {self.strict}")
        lines.append(f"- discovered GammaDSP classes: {len(classes)}")
        lines.append("")
        lines.append("Available GammaDSP classes from live SWIG introspection:")

        if not visible_classes:
            lines.append("- <none discovered>")
        elif include_methods:
            for class_name in visible_classes:
                info = self.get_module_info(class_name)
                methods = [
                    method_name
                    for method_name in info.get("methods", [])
                    if not str(method_name).startswith("_")
                ]
                lines.append(f"- {class_name}")
                for method_name in sorted(methods)[:80]:
                    lines.append(f"  - {method_name}")
        else:
            for class_name in visible_classes:
                lines.append(f"- {class_name}")

        if max_modules is not None and len(classes) > len(visible_classes):
            lines.append(f"- ... {len(classes) - len(visible_classes)} more classes omitted")

        lines.append("")
        lines.append("Patch output requirements:")
        lines.append("- Prefer JSON patches using ctor_args for constructors and params for setters.")
        lines.append("- Prefer explicit graph connections over implicit overlay metadata.")
        lines.append("- Include a focused validation command or test for every scaffold patch.")
        lines.append("- Do not import or depend on overlay/registry/factory files in active runtime code.")

        return "\n".join(lines)

    def print_system_prompt(
        self,
        max_modules: int | None = 160,
        include_methods: bool = False,
    ) -> None:
        print(self.system_prompt(max_modules=max_modules, include_methods=include_methods))

    # ============================================================
    # Validation
    # ============================================================

    def validate_node_chain(self, node_names: list[str]) -> list[str]:
        errors = []

        for node_name in node_names:
            if node_name not in self.nodes:
                errors.append(f"Unknown node: {node_name}")
                continue

            node = self.nodes[node_name]

            if node.kind not in {"function", "generator"}:
                errors.append(
                    f"Node '{node_name}' has unsupported kind for mono chain/block: {node.kind}"
                )

        return errors

    def validate_graph_connections(
        self,
        connections: list[tuple[str, str, float]],
        outputs: list[tuple[str, float]] | None = None,
    ) -> list[str]:
        outputs = outputs or []
        errors = []

        for src, dst, gain in connections:
            if not self._is_external_input_name(src) and src not in self.nodes:
                errors.append(f"Unknown graph source: {src}")

            if dst not in self.nodes:
                errors.append(f"Unknown graph destination: {dst}")

            try:
                float(gain)
            except Exception:
                errors.append(f"Invalid graph gain for {src} -> {dst}: {gain!r}")

        for node_name, gain in outputs:
            if node_name not in self.nodes:
                errors.append(f"Unknown graph output: {node_name}")

            try:
                float(gain)
            except Exception:
                errors.append(f"Invalid graph output gain for {node_name}: {gain!r}")

        return errors