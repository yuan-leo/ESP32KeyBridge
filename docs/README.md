# Documentation guide

[日本語](README.ja.md)

Which document to read, and in what order. The documents readers use — the beginner's guide, the adapter list, the data model, the core design — are bilingual, as are the READMEs, the release checklist, and every example. The rest of the design record (requirements, use cases, decisions, configuration notes, API sketches) and the process documents are Japanese-only (`*.ja.md`).

## Start here

| I want to... | Read |
| --- | --- |
| Understand what the library does and see a working sketch | [../README.md](../README.md) |
| **Start from zero: understand what is actually happening** | [GUIDE_BASICS.md](GUIDE_BASICS.md) — the beginner's guide: key model, transforms, and the traits and limits of each interface |
| Find a sketch for my hardware | [../examples/README.md](../examples/README.md) — every example is a complete, flashable sketch |
| Pick the right adapter, and know what it can and cannot do | [ADAPTERS.md](ADAPTERS.md) |
| Know why the key model looks like this | [DATA_MODEL.md](DATA_MODEL.md) |
| Write my own adapter | The `InputAdapter` / `OutputAdapter` comments in `src/ESP32KeyBridge.h`, the `TouchButtons` example, then [CORE_DESIGN.md](CORE_DESIGN.md) |
| See where the project stands and what is left | [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) and the status column of [ADAPTERS.md](ADAPTERS.md) |
| Know why something was designed this way | [DECISIONS.ja.md](DECISIONS.ja.md) |

## The documents

**Getting started**

- [GUIDE_BASICS.md](GUIDE_BASICS.md) — the beginner's guide: the pipeline as a whole, the key model and the transforms, the traits and limits of every input and output adapter, how to pick a chip and a port, and the common pitfalls. The way in before the design documents.

**Design (read in this order for the full picture)**

1. [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) — what the library is for and the range of devices it targets. *(Japanese)*
2. [USE_CASES.ja.md](USE_CASES.ja.md) — the concrete scenarios the design was validated against. Each one records the rule it forced (for example: clear an input's keys on disconnect, so a dead battery cannot leave a modifier stuck). *(Japanese)*
3. [DATA_MODEL.md](DATA_MODEL.md) — the intermediate representation: key identity as kind + code, held state vs one-shot traffic, lock state ownership, layout conversion.
4. [CORE_DESIGN.md](CORE_DESIGN.md) — the core / adapter / example boundary, the dependency rules, and the time & concurrency boundary (which context each library's callbacks run in).
5. [ADAPTERS.md](ADAPTERS.md) — every adapter: behavior, implementation status, dependencies, how the PC sees the USB composition, and tolerance to a slow `update()`.
6. [CONFIGURATION.ja.md](CONFIGURATION.ja.md) — configuration model notes; why persistence and configuration UIs stay outside the core. *(Japanese)*
7. [DECISIONS.ja.md](DECISIONS.ja.md) — the decision ledger, including options that were rejected and ideas deferred past v1. *(Japanese)*
8. [API_SKETCHES.ja.md](API_SKETCHES.ja.md) — early API sketches from before implementation. Kept as a record; `src/ESP32KeyBridge.h` is the authority. *(Japanese)*

**Process** *(Japanese)*

- [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) — implementation order, current position, and remaining work.
- [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) / [RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md) — what to confirm before a release, and how the shared release workflow runs.
- [../tests/README.ja.md](../tests/README.ja.md) — the test layout (`unit/`, `examples_compile/`, `single/`, `peer/`, `manual/`) and how to run each.
- [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) — the coverage matrix: what is tested where, and what is still planned.

## Conventions

- Code examples in the documents spell out the namespace (`esp32keybridge::`) rather than relying on a `using` directive.
- A document describes what is **implemented**; anything not yet built is marked as planned rather than written in the present tense.
- `ADAPTERS.md` distinguishes *implemented but not yet hardware-verified* from *hardware-verified*, so the status column can be trusted as-is when judging release readiness.
- A bilingual document keeps both versions in step: change one, change the other in the same commit. The Japanese version is the authority when they disagree.
