# GPU composition migration review register

Updated 2026-10-16 with the Power selection-color milestone. These acceptance items remain
open. This register complements the
[delivery checklist](gpu-composition-migration-checklist.md) and
[workload evidence](gpu-workload-baseline.md); it does not replace remaining implementation.

The user intentionally restored Explorer and removed Reach's installation, then authorized
continued implementation without restarting Reach. Keep that setup. No installed Reach launch,
restart, reinstallation, shell/task repair or OS power/radio change is part of this continuation.
Builds, pure tests and owned input-transparent diagnostics may continue. Live checks below stay
pending until a later authorized running session is available.

| ID | Item needing review | Evidence available | Evidence still required |
| --- | --- | --- | --- |
| R1 | Shared chrome, clipping, shadows and fitted sources | Both native renderers, CPU fallback, scoped DPI/pixel/resource fixtures and source lifetime checks | Review compact shadow/notch appearance, intermediate filtered edges and total source/driver cost in the product. Resolve the remaining capability gate rather than inferring approval from an isolated fixture. |
| R2 | Migrated feedback, selectors, loaders, carets and Launcher/Quick Settings reveals | Focused native/fallback fixtures, build/test results and documented idle/source behavior | Live appearance, overlapping input/capture/focus, source changes during motion, theme/font changes and hidden behavior. Review the entire affected consumer, not only its shared primitive. |
| R3 | Clipboard retained item width (`0e4c0b4`) | Both builds/all 40 tests; 320 displayed owned poses; full native/fallback regressions; exact-time input/reversal; source reuse and finite cleanup | Live close/clear/restore/scroll interaction and capture/focus; review moving rounded/text-edge filtering and the measured temporary surface increase (about 2.14 MB former, 2.19 MB settled, 5.22 MB expanded in the four-item scene). Height and smooth scrolling still need implementation. |
| R4 | Battery pending/Bluetooth grace deadlines (`910b9c0`) | Both builds/all 40 tests; owned two-second expiry, acknowledgement/recovery/reset and zero-delta status fixtures; waiting frame requests removed | Real power/radio service notifications, enumeration interruptions and behavior across hide/game-mode transitions. Installed follow-up was declined by reachctl after the user's intentional reset; no reinstall is authorized. |
| R5 | Live native previews and interaction | Dock/Switcher retained fitting, native destination descriptors and isolated source/motion checks | DWM source churn/loss, simultaneous overlapping preview surfaces, painter order, activation, exact input alignment and mixed-DPI transitions. Stage/native choreography and bar movement separation still need implementation. |
| R6 | Environment and lifecycle matrix | Scoped 100/125/150/200%, two themes, fonts, lifetime/recreation and bounded-source fixtures | Product checks across mixed DPI, monitor constraints/resolutions, normal/high refresh and battery saver; capture, focus, game mode, hidden retirement and callback cancellation. Record each unavailable configuration explicitly. |
| R7 | Total efficiency and regression acceptance | Opt-in counters, read-only collector, isolated scene timings and nominal retained-source bytes | Comparable repeated Reach + Settings + DWM CPU/GPU, latency, energy/power and memory measurements; variation/regression thresholds, observer overhead and independent GPU-memory evidence. No passing whole-product claim yet. |
| R8 | Final code/ledger acceptance | Focused commits, architecture checks, obsolete-path searches and recorded CPU responsibilities for migrated consumers | Finish every remaining consumer and timer/native disposition; remove superseded execution paths; review current architecture/ledger against the finished source. Do not close the goal with pending implementation or acceptance evidence. |
| R9 | Power option/custom-field colors and shared textbox ink | Both builds/all 40 tests; independent caret/color, bounded-source fallback and endpoint scheduling fixtures; complete former-producer pixel comparisons recorded in the baseline | Review intermediate glyph contrast from tinted coverage, custom edits/selection/suffix/placeholder/caret overlap, rapid reselection and focus. Confirm live theme/font/DPI changes and the measured source-memory/driver cost. Keep live and total-cost acceptance open; no Reach/Settings deployment was performed. |

The remaining implementation includes Clipboard height/smooth scrolling, Settings Wi-Fi/Bluetooth
row expansion and eligible scrolling, Top Bar control
widths, Dock membership/capacity fitting, Stage choreography and native bar movement separation.
Each implementation milestone adds its own validation and updates the relevant review entry.
