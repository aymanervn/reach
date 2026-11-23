# Reach architecture

Keep behavior working while moving the structure toward one coherent, reusable architecture.

## Layers

Dependencies flow inward; outer layers may include inner ones, never the reverse.

```
core ← protocol ← ports ← services ← features
                    ↑          ↑          ↑
                 adapters ────────────────┘
                    ↑
              composition
                    ↑
            tools, apps (leaves)
```

- **core**: primitives and neutral data. No state, policy or OS; standard library only.
- **protocol**: cross-process contracts (Reach Service messages, shared memory, object names).
- **ports**: interfaces for every external boundary.
- **adapters**: Windows implementations of ports; the only layer that touches the OS.
- **services**: shared in-process state, caches, threading and policy over ports.
- **features**: self-contained capsules; return semantic actions instead of calling ports.
- **composition**: wires adapters, services and features and runs the app.
- **tools / apps**: executables; nothing depends on them.

`tools/check_architecture.py` enforces the include rules.

## Read before working

| Work touches | Read |
|---|---|
| layers, capsules, composition, ports, services, config, polling | `docs/boundaries.md` |
| rendering, motion, GPU/CPU, scrolling, theme, typography, borders | `docs/rendering.md` |
| pointer input, popups, edge reveals, bar visibility | `docs/interaction.md` |
| Dock, Stage, Switcher, activation, game mode, HUD, tray | `docs/features.md` |
| planning, or "what is left" | `docs/pending-work.md` |

## Working rules

- Trace the behavior end to end. Before adding a solution, search the repo for one that already handles the same responsibility; reuse it or extract a shared one at the innermost valid layer.
- Existing code is evidence, not proof of intent. Do not promote a precedent to a rule.
- Investigate suspicious platform code before changing it; it may be an undocumented workaround. Do not preserve structural debt only because it works.
- Finish migrations through the whole call and data path and delete superseded paths, APIs and dead code in scope. No compatibility or legacy path survives the migration commit.
- A migration or refactor changes structure only. Keep lifecycle order, event routing, pointer priority, focus restore, z-order and game-mode filtering identical. If a change is unavoidable, stop, explain it and ask before making it.
- Never make a check pass by exempting a file or path in `tools/check_architecture.py`; fix the code or ask. Delete checker entries whose files or symbols no longer exist.
- Never fix an unrelated finding during a task. Record it in `docs/pending-work.md` for its own task and commit.
- Current phase is stabilization: fix the scoped bug and record architectural violations. Start repo-wide cleanup only when the user starts that phase.
- The user has opened a theme redesign phase: surfaces, text strengths, fills, hues and accent roles. It is limited to the palette, theme tokens and their consumers; each step lands only after the user picks it on a before/after page and confirms it live.
- When evidence conflicts or a choice would materially change the architecture, explain it and ask the user.
- For a visual bug, get an exact repro and live evidence before fixing. Never call it fixed until the user confirms.
- Comment only a hack, workaround or temporary code, or what is needed to understand the code: a non-obvious reason, an invariant, or a contract's coordinate space, units or calling thread. Never restate what names and code already say.

## Verification

- Run builds and tests at Idle priority with affinity 0xF000 (cores 12–15): set it on the launching shell first.
- Run the relevant build, `ctest`, `tools/check_architecture.py` and `clang-format --dry-run --Werror`, in proportion to the change. Search for old APIs and stale paths afterwards.
- New tests only guard fatal crashes: worst-case render-budget overflow in `tests/host_layout_tests.cpp`. No new helper, framework, motion, rendering or UI tests. Edit an existing test only when a change breaks it. Use `REACH_ASSERT` at invariants.
- The user validates visible behavior on a deployed build. Do not automate it with probes.

## Keeping these docs

- `boundaries.md`, `rendering.md`, `interaction.md` and `features.md` hold only rules the user confirmed. Existing code never adds a rule.
- When the user states a new preference, restate it as a concrete rule with its consequences, ask for confirmation, then add one line that points to symbols or paths instead of copying code.
- Do not add migration or change-log docs. Record behavior to validate in `pending-work.md` and history in the commit message.
- `pending-work.md` holds open work only: one line per item, with `file:line` evidence when useful. Delete an item when it is done.
