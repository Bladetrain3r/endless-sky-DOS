# Bounded native pursuit slice

`pursuit_command` reproduces `AI::MoveToAttack` and default-precision
`AI::TurnToward` for the loaded stock Star Barge. Its only fitted weapon is an
anti-missile turret, so native `TargetAim` falls back to the current target
direction. The stock ship has no reverse thrust or afterburner. The command
returns forward thrust and turn; the caller applies it through the already
qualified `motion_step` and owns target selection and firing.

`pursuit_intercept` reproduces `AI::RendezvousTime`, including NaN when no
nonnegative solution exists and positive infinity for its equal-speed
approaching case. It is available for a separate turret lead rule;
the stock `MoveToAttack` fallback itself does not use target velocity.

Run `python3 dos/pursuit/check.py` to build the unchanged-object native oracle,
compare the C helper under ASan/UBSan, then cross-compile and compare in
headless DOSBox. `--reuse-oracle` accepts only the locally hash-checked oracle
outputs. The native oracle calls the actual private AI static methods through
an oracle-only header access change; it does not change original game objects.
It exports fitted `Acceleration`, `ReverseAcceleration`, `TurnRate`, `DragForce`
and acceleration multiplier from loaded game data. `assets.py::stage(out)`
guards those outputs and copies `PURSUIT.DAT` into the runtime directory.

`PURSUIT.DAT` is exactly 48 bytes: eight-byte `ESPURS1` plus NUL magic and five
little-endian binary64 values in `MotionParameters` field order. `pursuit_load`
rejects missing, short, extended, malformed and non-finite profiles, plus
unsupported reverse or out-of-range parameters. Rejection leaves the
destination unchanged.

The comparison covers 16 ahead, behind, crossing, close, zero-displacement,
heading-wrap and velocity cases, plus 11 intercept cases including near-equal
speeds, the equal-speed infinity and impossible solutions. Command flags must agree exactly; turn
tolerance is 1e-12 and intercept time tolerance is 1e-8 world ticks. Native
and DOS currently report zero observed error. Evidence is in
`dos/reports/pursuit-equivalence.json`.

This slice does not implement tactics, target selection, retreat, avoidance,
factions, turret firing or complete AI behavior.
