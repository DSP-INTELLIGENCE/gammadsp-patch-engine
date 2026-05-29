# Milestones

## Done

- [x] Created extracted `gammadsp-patch-engine` repo.
- [x] Added initial extraction docs.
- [x] Copied minimal runtime source and headers.
- [x] Fixed SWIG build to use active Python and NumPy include path.
- [x] Removed generated `.gch` artifact.
- [x] Added missing `automation.py` dependency.
- [x] Confirmed SWIG smoke gate.
- [x] Confirmed SWIG introspection PatchEngine gate.
- [x] Confirmed JSON PatchEngine gate.
- [x] Added CLI runner baseline.
- [x] Added JSON/DSL runner behavior.
- [x] Added processor patch example with caller-supplied input JSON.
- [x] Tagged `checkpoint-json-dsl-runner-process-01`.

## In progress / next

- [ ] Update README and repo navigation for the current design.
- [ ] Add `AGENTS.md` for ChatGPT/Codex-compatible instructions.
- [ ] Add repository memory, roadmap, milestones, and handoff prompts.
- [ ] Add patch design guide and schema-lite examples.

## Planned

- [ ] Define patch metadata conventions.
- [ ] Define I/O contracts for generator, processor, analysis, and utility patches.
- [ ] Define public controls separate from raw node params.
- [ ] Define control-level automation conventions.
- [ ] Add library-grade examples.
- [ ] Add batch render/process workflows only after patch design stabilizes.
- [ ] Add Codex/CI runner scripts.

## Deferred

- [ ] Real-time audio playback demos.
- [ ] Graph-window demos.
- [ ] Legacy test migration.
- [ ] Large generated datasets.
- [ ] Full formal JSON Schema enforcement.
