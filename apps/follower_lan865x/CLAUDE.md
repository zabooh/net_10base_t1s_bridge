# follower_lan865x — Work Instructions

T1S follower endpoint firmware (`T1S_Follower.X`, PTP-over-10BASE-T1S time synchronization,
one interface, no bridging), ported on 2026-08-31 from the sister project
`C:\work\t1s_bridge\bridge\t1s_100baset_bridge\follower\`. Analogous to
`apps\bridge_lan865x_100baseT\` (own `CLAUDE.md` there), a standalone app at this
level, independent of the rest of the `net_10base_t1s` content repo.

## Hard rule: this project deliberately has NO MCC model

See [`firmware\T1S_Follower.X\NO_MCC_MODEL.md`](firmware/T1S_Follower.X/NO_MCC_MODEL.md)
for the full rationale (short version: an earlier model here was the *bridge's*, not the
follower's — "Generate Code" would have turned the follower into a bridge). **Never run MCC
"Generate Code" against `firmware\src\config\default\`** — builds are done exclusively from the
tracked `nbproject\configurations.xml`, via `batch\genmk.bat` (MPLAB X's own
`prjMakefilesGenerator.bat`). If something is missing from the generated code, the fix belongs
in its own source file or in `nbproject\configurations.xml`, not in an MCC model — there is none.

## Building, Flashing, Console

```bat
setup.bat                 :: once per machine, after cloning (venv, pyOCD, debug fix, makefiles)
build.bat                 :: incremental (default), TYPE_IMAGE=PRODUCTION
build.bat rebuild         :: clean + full
flash.bat                 :: pyOCD via EDBG probe
flash.bat --list          :: connected probes
cli.bat "help"             :: send a command over the serial console
```

- **Own `.venv`**, no shared repo-root tooling — deliberately simplified compared to the
  sister project when it comes to flashing: no role-based `flash_boards.py`/`boards.json`
  (there intended for two followers A/B) — `flash.bat` flashes a single board directly via
  pyOCD; with multiple probes connected, select one explicitly with `--probe <serial>`.
  `setup_compiler.py` (XC32 version selection), on the other hand, **is** included (since
  2026-08-31, two XC32 versions installed on this machine) — but only as a note, `build.bat`
  does not read `setup_compiler.config` (same reasoning as in the sister project: the only
  real consumer there is `build_summary.py`'s `xc32-nm`, which doesn't exist here). Details:
  `apps\bridge_lan865x_100baseT\CLAUDE.md` section 2.
- `batch\genmk.bat` uses the same dynamic MPLAB X version detection as
  `apps\bridge_lan865x_100baseT\batch\genmk.bat` (fixed there on 2026-08-31 against a
  hardcoded version list that would have missed a newer installed version — details there).
- **Board mapping on this bench** (from `apps\bridge_lan865x_100baseT\scripts\iperf_matrix_test.py`):
  Follower A = `COM10` / `192.168.0.201`, Follower B = `COM23` / `192.168.0.202`. Both
  originally ran firmware flashed via the sister project. **Since 2026-08-31, Follower B runs
  the `follower_lan865x` firmware built here** (the first flash of this port ever, see entry
  below); Follower A continues to run the old, unchanged, working firmware.

## Not our own bug — false accusation from 2026-08-31 retracted

On 2026-08-31, during bridge debugging, initially and mistakenly documented here as a "known,
open follower bug": broken outgoing ICMP/TCP packets (Follower A **and** Follower B),
seemingly confirmed by an isolation test (independent of the `_Lock`/`_Unlock` fix below,
independent of the follower — since it also reproduced on the never-touched Follower A
firmware). **The isolation test was incomplete** — it never cross-checked against an actually
correct bridge firmware, only against two different bridge states that were both faulty at the
time (first a wrong patch attempt, then the unhandled original bug). Following the user's
hint ("I don't think that's the follower"), retested correctly: with both the sister bridge
firmware and the project's own, ultimately fixed `bridge_lan865x_100baseT` firmware (item 9 in
its `docs\mcc-generated-code-patches.md`), **both** followers (A and B) deliver clean ICMP and
TCP packets — verified via Wireshark (`ip.len` correct, no more length discrepancy) and via a
complete iperf TCP transfer (Follower B: 1663/1663 packets, 2.32 MB, 0% loss, client and
server report identical). **There is no separate follower bug** — the entire original
observation was an artifact of the bridge being broken at the time of testing. Lesson for
future isolation tests: a third, independently known-good reference (here: the sister
firmware) belongs in the test matrix, not just "fixed vs. reverted" of the same own change.

## LAN865x driver race fixed (`_Lock`/`_Unlock`)

**2026-08-31.** As documented in `apps\bridge_lan865x_100baseT\docs\mcc-generated-code-patches.md`
item 2, but never ported here: `_Lock()`/`_Unlock()` in `drv_lan865x_api.c` only wrapped
`OSAL_MUTEX_Lock/Unlock` — on this bare-metal OSAL build (`osal_impl_basic.h`) that's just a
simple flag, not a real lock. The SPI transfer-complete callback (`_EventHandlerSPI()` →
`TC6_SpiBufferDone()`, a real hardware interrupt) could therefore fire at any time in the
middle of `TC6_Service()` — the same class of bug as the RX race in the bridge, but here on
the TX credit side (`g->txc`). **Fix adopted identically from the bridge:** `_Lock`/`_Unlock`
now additionally frame the code with `SYS_INT_Disable()`/`SYS_INT_Restore()`. No `.patch`
tracking needed (this project has no MCC model, the code is never regenerated) — the change
lives directly and permanently in the source, with a `HAND-PATCH` comment noting its origin.
- **`build.bat` additionally copies the hex to `release\T1S_Follower.hex` after every
  successful build** (since 2026-08-31, as in the sister project, checked in there — so a
  fresh clone can flash without building first). **Only `build.bat` updates this copy** — a
  build done directly from the MPLAB X IDE leaves `release\` stale. **`flash.bat` flashes
  exactly this `release\` file by default** (since 2026-08-31, previously `dist\`) — to flash a
  fresh local build instead, specify the `dist\` path explicitly:
  `flash.bat firmware\T1S_Follower.X\dist\default\production\T1S_Follower.X.production.hex`.
- Otherwise the same workflow applies as in the sister project: build in MPLAB X is done by
  the user themselves, don't proactively call `build.bat`/`flash.bat` to "prove" something.
