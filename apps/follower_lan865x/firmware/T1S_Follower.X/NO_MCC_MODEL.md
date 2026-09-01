# This project deliberately has no MCC model

**Removed on 2026-08-21** (decision made on 2026-08-12). Previously this contained
`T1S_Follower_default/mcc-config.mc4` and 47 `components/*.yml`, plus the two
`mcc-manifest-*.yml` — 48 tracked files, 272 KB.

## Why they're gone

**The model wasn't this project's — it was the bridge's.** The hash maps of
both projects were byte-identical, and the module list still contained `drvGmac`,
`drvMiim`, `drvExtPhyLan8740`, `sercom0`, and `tcpipNetConfig_1` — all things the
follower doesn't have. The derivation is in
[CONFIG_BASELINE.md](../../../docs/wissen/CONFIG_BASELINE.md) §0.2.

This led to a trap, and it was the reason for the removal: **"Generate Code"
in this project would not have restored an older follower, but would have turned the
project into a bridge** — two interfaces, GMAC, MAC bridge. The damage
looks like a destroyed project afterward, not a regenerated one, and costs
correspondingly more time to track down.

Nothing was lost in the process: there never was a model describing this follower.

## What this means for the build: nothing

The build is done from `nbproject/configurations.xml`, and `genmk.bat` generates the
Makefile fragments from it using the tool that MPLAB X itself ships with. **Verified on
2026-08-21:** fragments deleted, regenerated from the edited `configurations.xml`,
full rebuild — `BUILD SUCCESSFUL`, hex **the same size** (649,676 B) and
only **two differing lines**, which are the build timestamp `__DATE__`/`__TIME__`
in `app.c`.

## If you want the model back

Then the right answer is **not** `git revert`, but a **new** model
that describes this follower: one interface, no GMAC, no MAC bridge, the
LAN865x on SPI. The values for that are fully documented in
[CONFIG_BASELINE.md](../../../docs/wissen/CONFIG_BASELINE.md) — that document exists precisely for
this case. What an agent must observe while doing so is in
[MCC_IN_THE_AGENT_AGE.md](../../../docs/strategie/MCC_IN_THE_AGENT_AGE.md) Appendix A.

**The bridge gave up its model on 2026-08-22 too** — but there the reason was
different: not a model that described the project incorrectly, but the
clock rework, which required a patch in generated territory. Rationale and the
metrics of the model are in `CONFIG_BASELINE.md` §5.3, the note there in
[KEIN_MCC_MODELL.md](../../../firmware/T1S_100BaseT_Bridge.X/KEIN_MCC_MODELL.md).
