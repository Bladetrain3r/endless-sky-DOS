# Star Barge opponent profiles

`python3 dos/opponent_profile/native.py` links a replacement main to the unchanged
native Endless Sky object files in `.work/native/build`. It loads the stock fitted
Star Barge, asserts the subset supported by the DOS resource, shield and propulsion
kernels, and writes native getter values and `Ship::DoGeneration` traces under
`.work/opponent_profile/`. The source, data, binary and outputs are hashed in
`provenance.json`. The stock anti-missile turret remains fitted. Energy Blaster
firing energy and heat are read separately from the native weapon and included as
an **artificial training gun cost**; this is not a stock Star Barge gun or loadout.

`dos/opponent_profile/assets.py` exposes `stage(out)`, returning a dictionary of
`Path`s keyed `resource`, `shield`, `propulsion`, `hull`. It validates the native
baseline, upstream source and data, oracle source, and oracle output hashes before
writing `BARGERES.DAT` (`ESRES2\0\0` + seven little-endian binary64),
`BARGESH.DAT` (`ESSHLD2\0` + seven binary64 and two uint32), `BARGEPRO.DAT`
(`ESPROP2\0` + four binary64), and `BARGEHP.DAT` (`ESPLAYER1` + hull capacity
and native minimum hull as two binary64). Running the script directly stages to
`.work/flight/run`. The text oracle prints 17 significant decimal digits so
conversion round-trips the native binary64 getters.

The fixture rejects reverse/afterburner movement, special movement costs,
active cooling, solar/fuel generation, delayed hull repair, disabled recovery,
overheat hull damage, special shield costs and carried ships. It traces a healthy
barge, drained shields, empty battery, overheat recovery and hull disable. It does
not qualify anti-missile turret logic, firing cadence, status effects, collision,
AI, repairs, or generic behavior for other ships.

`python3 dos/opponent_profile/check_dos.py` stages the profiles, compiles the
existing generic C resource and shield kernels for i386 DOS, and compares all
430 native generation rows in a network-isolated DOSBox container at 16 MiB and
fixed 20,000 cycles. It also runs `check.py` with native C in the reference
container.
Observed maximum absolute error is zero for shield, energy and heat in both
checkers. Compact evidence is retained in `dos/reports/opponent-profile-equivalence.json`;
intermediate binaries and traces remain in ignored `.work/`.
