# Session Log — tcpip_iperf_lan865x Bridge Bring-Up

Chronological record of actions taken and outcomes reached while bringing up the
10BASE-T1S ↔ 100BASE-T bridge on this app. Kept in English regardless of the console
language used during the session (see `CLAUDE.md`, section 0). Append after each
completed step — do not wait until the end of the session.

---

## 2026-08-30

### Screenshot capture of MCC TCP/IP Configurator state

- Located the day's screenshots in
  `C:\Users\M91221\OneDrive - Microchip Technology Inc\Pictures\Screenshots`.
- Copied them into `docs\images\` with descriptive names reflecting their content
  (MCC TCP/IP Configurator screens):
  - `tcpip-configurator-overview.png` — Overview page, all TCP/IP layers, LAN8742A
    selected in the Data Link layer.
  - `tcpip-configurator-datalink-lan8742a.png` — Data Link layer, GMAC + LAN865x-0
    wired to the LAN8742A PHY (later replaced by a rescroll of the same view — see
    below).
  - `tcpip-configurator-config-summary-interfaces.png` — Config Summary → Interface
    tab: Interface 0 = LAN865x, Interface 1 = GMAC.
  - `tcpip-configurator-datalink-lan8740.png` — Data Link layer, GMAC + LAN865x-0
    wired to the LAN8740 PHY (an earlier/alternate PHY selection).
- A later screenshot of the same Data Link layer view (PHY component list scrolled
  down, `DP83640` highlighted) was copied in as
  `tcpip-configurator-datalink-phy-list-scrolled.png`, then — on request — the
  original `tcpip-configurator-datalink-lan8742a.png` was deleted and this file was
  renamed to take its place, so `tcpip-configurator-datalink-lan8742a.png` now shows
  the scrolled PHY list state.

### GMAC boot failure — first diagnosis pass

- User switched the PHY selection in MCC to the physically correct part, rebuilt,
  and reflashed, but the boot-time error persisted.
- Confirmed via the serial CLI (`cli.bat --port COM8 --read 3 "reset"`, per the
  recipe in `CLAUDE.md` section 2) that the failure is unchanged:
  ```
  TCP/IP Stack: Initialization Started
  TCP/IP Stack: GMAC MAC initialization failed
  TCP/IP Stack: Initialization failed 9 - Aborting!
  ```
- This matches the failure already recorded in `CLAUDE.md` section 3 (entry dated
  2026-08-29/30) — pin assignment and clock/GMAC/MIIM macros had already been
  diff-checked against the working sister project
  (`t1s_100baset_bridge`) and found identical, yet the failure remained.

### Clock configuration comparison (peripheral clocking hypothesis)

- User hypothesized the GMAC peripheral clocking might still be wrong and asked for
  a fresh comparison against the sister project.
- Diffed `firmware\src\config\default\peripheral\clock\plib_clock.c` between this
  project and the sister project: **byte-identical except for a MISRA 2012 → MISRA
  2023 deviation-comment update** (this project was regenerated with a newer MCC
  MISRA ruleset). No functional difference — `MCLK_AHBMASK` (which gates the GMAC
  AHB bus clock) is `0xffffffU` in both, i.e. all AHB clocks including GMAC's are
  enabled; the `GCLK_PCHCTRL[7..9]` channels configured are for SERCOM peripherals
  (SPI/USART), not GMAC — GMAC on SAME54 does not need a dedicated `GCLK` peripheral
  channel, only the AHB bus clock.
- Conclusion: the top-level clock initialization is **not** the source of the
  divergence between the working sister project and this project's GMAC init
  failure — this reconfirms (rather than newly discovers) what section 3 of
  `CLAUDE.md` already noted.
- Also diffed `driver\gmac\src\dynamic\drv_gmac_lib_same5x.c` (this project) against
  `drv_gmac_lib_samE5x.c` (sister project, older Harmony `net` package version):
  large textual rewrite (static-function renames, added inline pointer-cast
  helpers, MISRA-driven casts) consistent with the already-documented package
  version gap (`net v3.14.5` vs. sister `v3.11.1`). The `GMAC_NCFGR` register setup
  (speed/duplex/MDC clock divider bits) itself is unchanged in content, only
  reformatted — so no smoking gun found here yet either.
- **Open follow-up:** the `drv_miim.c` rewrite between package versions (noted in
  `CLAUDE.md` section 3, >1800 diff lines) is still the next candidate to check for
  actual behavioral changes, not just MISRA/style churn.

### Runtime memory-dump CLI command ported from the sister project

- The sister project (`t1s_100baset_bridge/firmware/src/app.c`) has a `dump
  <addr_hex> <count>` CLI command that hex/ASCII-dumps arbitrary RAM, useful for
  reading live driver structs (e.g. GMAC descriptor arrays, MAC config) instead of
  reasoning from source alone.
- Ported the relevant parts into this project's `firmware\src\app.c` (a user file,
  not MCC-generated — allowed to edit per `CLAUDE.md` section 1's exception):
  - `DumpMem(addr, count)` — 16-bytes-per-line hex+ASCII dump; blocks on
    `SYS_CONSOLE_WriteFreeBufferCountGet()` before each `SYS_CONSOLE_PRINT()` line
    to avoid silently dropping output when the SERCOM TX ring buffer fills faster
    than 115200 baud can drain it (same rationale as the sister project's version).
  - `cmd_mem_dump()` — parses `dump <addr_hex> <count>` and calls `DumpMem()`.
  - Registered as a new `"Test"` command group via `SYS_CMD_ADDGRP()` in a local
    `Command_Init()`, called from `APP_Initialize()`.
- Only the memory-dump command was ported (not the sister project's `ipdump`,
  `stats`, `meminfo`, `logclear`, `logstat`, `uptime`, `history` commands), since
  the immediate goal is runtime inspection of GMAC/MAC state, not a full port of
  the sister project's diagnostic command set.
- **Not yet built/flashed** — per working agreement, the user builds in MPLAB X
  themselves; this session does not proactively invoke `build.bat`. Once built and
  flashed, `cli.bat --port COM8 --read N "dump 0x<addr> <count>"` can be used to
  inspect e.g. `gmac_dcpt_array`, `s_gmac_queue`, or `pMACDrv->sGmacData` at
  runtime.
- Added two further commands to the same `"Test"` group, complementing `dump`:
  `peek <addr_hex> [size=1|2|4]` (read a single value, default 4 bytes) and
  `poke <addr_hex> <value_hex> [size=1|2|4]` (write a single value, default 4
  bytes) — for quick single-register/variable inspection and for forcing state
  (e.g. a register bit) while debugging from the CLI, without needing a full
  `dump` block for a one-off check.

### Documentation process set up

- Added section 0 to `CLAUDE.md`: all Markdown files under `docs\` must be written
  in English (the console/chat language is independent and stays whatever the user
  is using); all C and Python code must be entirely in English (identifiers,
  comments, log/console output, docstrings, error messages).
- Established this file (`docs\session-log.md`) as the running record of actions
  and outcomes for this bring-up effort, and `docs\bridge-configuration-manual.md`
  (see below) as a separate, reader-facing how-to document — kept apart so the
  chronological log and the standalone manual don't get tangled together.

### Heap-size comparison — second, more promising divergence found

- User pushed back on the clocking hypothesis being fully closed; re-verified by
  diffing `plib_clock.c` again and confirming (as above) it is byte-identical
  (bar the MISRA comment) and that `PORT_Initialize()` → `CLOCK_Initialize()`
  ordering in `initialization.c` matches the sister project exactly (line
  621/623 here vs. 747/749 there) — clocking stays ruled out.
- Compared heap sizing instead, since `DRV_GMAC_Initialize()` allocates RX/TX
  descriptors and buffers via `macControl->memH` (the TCP/IP internal heap) in
  `F_DRV_GMAC_RxCreate`/`F_DRV_GMAC_TxCreate` (`drv_gmac.c` lines 513-521) — a
  starved heap fails exactly at this point and produces the observed message.
  Found a real, substantial divergence:
  - **TCP/IP stack heap** (`TCPIP_STACK_DRAM_SIZE`, `configuration.h`): **39250
    bytes** here vs. **131072 bytes** in the sister project — despite identical
    consumers (8 RX / 8 TX GMAC descriptors, 1536-byte RX buffers, 2×
    LAN865x RX descriptors, 10 TCP + 10 UDP sockets — all confirmed equal by
    diff).
  - **Linker (libc) heap** (`nbproject\configurations.xml`, `heap-size`
    property): **44960 bytes** here vs. **163840 bytes** in the sister
    project.
  - The two are linked: `tcpipHeapConfig.heapType = TCPIP_STACK_HEAP_TYPE_INTERNAL`
    with `malloc_fnc = malloc` (`initialization.c`/`configuration.h`) means the
    entire TCP/IP heap is obtained via a single `malloc(TCPIP_STACK_DRAM_SIZE)`
    call out of the linker-reserved libc heap. Here that leaves only ~5.7 KB of
    libc heap free for everything else after that one allocation, vs. ~32 KB
    of headroom in the sister project.
  - Both values here are smaller than the sister project's by almost exactly
    the same ratio (~3.6×), which reads as an across-the-board undersized
    heap configuration rather than a deliberate choice — plausibly left over
    from before the GMAC interface was added and never revisited.
- A background research agent (`Explore`, prompted to compare `drv_miim.c`,
  `drv_ethphy.c`, and the LAN8742A vs. LAN8740 PHY objects line-by-line for
  genuine behavioral differences, not MISRA-style noise) reported back in
  parallel: **no behavioral differences found** in any of those areas — only
  renames/casts, and one timing-order shift in `F_DRV_ETHPHY_SetupPhaseReset`
  (reset-timeout deadline computed one MDIO sub-phase earlier) too small
  (~tens of µs against a 500 ms budget) to plausibly be the actual cause. This
  reinforces the heap-size divergence as the leading candidate.
- **Fix applied so far:** user increased the linker heap-size in MCC (System →
  Project Configuration → XC32 Global Options → Linker → General → Heap Size)
  from 44960 to **163840** bytes, matching the sister project — see
  `docs\images\mcc-linker-heap-size-before-44960.png` and
  `docs\images\mcc-linker-heap-size-after-163840.png`.
- **Still open:** `TCPIP_STACK_DRAM_SIZE` itself (the TCP/IP Stack component's
  own "Basic Configuration" heap-size property in MCC, separate from the
  linker heap-size just changed) has **not** yet been raised from 39250 to
  match the sister project's 131072 — needed next, then Generate Code, then a
  build/flash/reset cycle to check whether the GMAC init failure clears.
- **MCC field located and set:** the TCP/IP heap size lives under component
  `TCPIP CORE` → category **"Heap Configuration"** → field **"TCP/IP Stack
  Dynamic RAM Size"** (model id `TCPIP_STACK_DRAM_SIZE`, under `TCP/IP STACK` →
  `BASIC CONFIGURATION` → `TCPIP CORE` in the Project Graph). It was previously
  unset in this project (no `User` override in `tcpipStack.yml`, running on the
  auto-computed default that produced `39250`). Noted along the way: the
  sister project's model has this field set to `65536`, yet its generated
  `configuration.h` shows `131072` — exactly double — suggesting the
  generator template doubles the entered value (possibly a safety margin);
  worth confirming after this project's own Generate Code run rather than
  assuming. User has now set it to **`65535`** here (see
  `docs\images\mcc-tcpip-core-dynamic-ram-size-65535.png`) — matching the
  sister project's `65536` (off by one, likely the field's max/rounding).
  Still needs **Generate Code** to actually take effect in `configuration.h`.

### RESOLVED — root cause confirmed: undersized heap, not clocking/pins/PHY

- After Generate Code, build, and flash with both heap sizes raised (linker
  `heap-size` 44960→163840, MCC `TCPIP_STACK_DRAM_SIZE` 39250→65535), the user
  confirmed: **both network interfaces (LAN865x 10BASE-T1S and GMAC
  100BASE-T) now come up successfully on boot** — the
  `TCP/IP Stack: GMAC MAC initialization failed` / `Initialization failed 9`
  failure is gone.
- Further confirmed with a live test: **`ping 192.168.0.202` (another T1S node
  on the bus) succeeded**, i.e. the 10BASE-T1S side is not just "initialized"
  but actually passing traffic.
- This confirms the diagnosis chain from this session: the clock
  configuration, GMAC/MDIO pin assignment, PHY address, and `drv_miim.c`/
  `drv_ethphy.c` MDIO logic were all correctly ruled out one by one — the
  actual cause was the TCP/IP stack's internal heap (and the libc heap backing
  it via a single `malloc(TCPIP_STACK_DRAM_SIZE)` call) being sized far
  smaller than the sister project's, so `DRV_GMAC_Initialize()`'s descriptor/
  buffer allocation (`F_DRV_GMAC_RxCreate`/`TxCreate`) was starved of memory.
- **Still to verify:** 100BASE-T side traffic (a ping across the GMAC
  interface, not just the T1S side), and bridging behavior between the two
  interfaces end-to-end.

### `netinfo` CLI command — link status confirms the fix on both interfaces

- Ran `netinfo` over the serial CLI (`cli.bat --port COM8 --read 3 "netinfo"`)
  to check both interfaces' state directly:
  ```
  Interface eth0/LAN865X0(0): 192.168.0.11/24, GW 192.168.0.1, MAC 00:04:25:1c:a0:02, Link UP, Status: Ready
  Interface eth1/GMAC(1):     0.0.0.0/24,      GW 0.0.0.0,       MAC 00:04:25:1c:a0:02, Link UP, Status: Ready
  ```
- **Both interfaces report `Link is UP` / `Status: Ready`**, including the
  GMAC/100BASE-T side — direct confirmation that the original bug (the PHY
  link never coming up) is genuinely fixed by the heap-size correction, not
  just that stack initialization no longer aborts.
- Two things flagged for follow-up, not yet resolved:
  - `eth1` (GMAC) has `IPv4 Address: 0.0.0.0` and `Gateway: 0.0.0.0` — no
    address assigned yet, so no traffic can pass over it even though the link
    is physically up. Unclear whether this interface is meant to get a
    DHCP/static address or intentionally run address-less as a pure bridge
    port — needs a decision before further testing on that side.
  - Both interfaces report the **same MAC address**
    (`00:04:25:1c:a0:02`). Expected once the MAC bridge is enabled, but per
    the "Bridge aktivieren" note above, `TCPIP_NETWORK_MACBRIDGE_ADD_IDXn` is
    **not yet** enabled on either `NETCONFIG-0`/`NETCONFIG-1` in this
    project — so a shared MAC at this stage is unexpected; the sister
    project's Config Summary shows distinct `Internal Mac: NO` (interface 0)
    vs. `Internal Mac: YES` (interface 1), suggesting these should currently
    differ. Worth checking the MAC address source configuration before
    enabling bridging.

### `eth1` IP config root cause found: typos in MCC `User` overrides

- Traced why `eth1` (GMAC) showed `0.0.0.0`: `configuration.h` has
  `TCPIP_NETWORK_DEFAULT_IP_ADDRESS_IDX1 = "192.168..12"` (missing an octet —
  double dot) and `TCPIP_NETWORK_DEFAULT_GATEWAY_IDX1 = "192.168.0..1"` (extra
  dot) — both malformed strings that fail to parse, hence the stack falls
  back to `0.0.0.0`. Confirmed in the MCC model
  (`firmware\tcpip_iperf_lan865x.X\tcpip_iperf_lan865x_default\components\tcpipNetConfig_1.yml`):
  both fields carry a `type: User` override with the typo'd value, sitting
  next to the (correct, unused) `type: Dynamic` auto-default
  (`192.168.100.11` / `192.168.100.1` respectively).
- Also confirmed `TCPIP_NETWORK_DEFAULT_MAC_ADDR_IDX1` has **no** `User`
  override in the model — it's just the `Dynamic` auto-default
  (`00:04:25:1C:A0:02`), which happens to be identical to `eth0`'s MAC. Since
  MAC bridging is not yet enabled on either `NETCONFIG` instance (see section
  3 of `CLAUDE.md`), two interfaces sharing one MAC at this stage is likely
  unintended, not a bridging side-effect.
- **Fix path (MCC, not hand-editing `configuration.h` — see rule 1):** select
  the `NETCONFIG` component's **Instance 1** (the one wired to
  `DRV_GMAC_Object` in the Data Link Layer graph) → **Basic Configuration** →
  correct the **"IP Address"** and **"Gateway"** fields (typo fixes), and set
  an explicit, distinct **"MAC Address"** for this instance → **Generate
  Code**.
- **Open decision handed to the user, not yet made:** should `eth1` end up on
  the same subnet as `eth0` (`192.168.0.0/24` — sensible groundwork for the
  planned bridge) or keep the original separate-subnet default
  (`192.168.100.0/24`)? Determines what the corrected "IP Address"/"Gateway"
  values should actually be, beyond just fixing the typos.
- **Fix applied and confirmed via `netinfo`:** user corrected the MCC fields
  (put both interfaces on `192.168.0.0/24`, gave `eth1` its own MAC). Current
  state:
  ```
  eth0/LAN865X0(0): 192.168.0.11, mask 255.255.255.0, gw 192.168.0.1,   MAC 00:04:25:1c:a0:02, Link UP, Ready
  eth1/GMAC(1):     192.168.0.12, mask 255.255.255.0, gw 0.0.0.0,       MAC 00:04:25:1c:a0:03, Link UP, Ready
  ```
  Both interfaces now have distinct, valid IPs and distinct MACs.
  `eth1`'s gateway is still `0.0.0.0` — not corrected, likely harmless for
  same-subnet LAN traffic but worth revisiting if `eth1` ever needs to route
  off-subnet.

### End-to-end ping test plan across the physical test setup

- Test topology as described by the user:
  - **Bridge under test** — serial console on **COM8**. `eth0` (LAN865x/T1S) =
    `192.168.0.11`, `eth1` (GMAC/100BASE-T) = `192.168.0.12`.
  - **A second, independent 10BASE-T1S node** on the same T1S bus as the
    bridge's `eth0` — serial console on **COM23**, IP `192.168.0.202`.
  - **PC** — its Ethernet adapter (labelled "Ethernet 8" in Windows) on
    `192.168.0.100`, connected to the bridge's `eth1` (100BASE-T) side.
  - All three IPs share the `192.168.0.0/24` subnet.
- Planned/observed ping matrix:

  | From → To | Target IP | Result |
  |---|---|---|
  | PC → Bridge `eth0` | 192.168.0.11 | planned |
  | PC → Bridge `eth1` | 192.168.0.12 | planned |
  | PC → other T1S node | 192.168.0.202 | planned |
  | Bridge → other T1S node (`ping 192.168.0.202`, default route via `eth0`) | 192.168.0.202 | **confirmed working** |
  | Bridge → PC (`ping 192.168.0.100 i eth1`, explicit interface) | 192.168.0.100 | planned |
  | Other T1S node → PC | 192.168.0.100 | planned |

- **Gap identified when asked to review the plan for completeness:** the
  mirror direction **other T1S node → Bridge** (pinging the bridge's `eth0`
  and/or `eth1` IP from the COM23 node) was missing from the plan — not
  guaranteed to behave identically to "Bridge → node" just because the
  reverse direction works.
- Also flagged as a useful (optional) diagnostic, not yet run: **other T1S
  node → Bridge `eth1` (192.168.0.12)**. Since the MAC bridge is not yet
  enabled on either `NETCONFIG` instance (see section 3 of `CLAUDE.md`), this
  is expected to **fail** — there is no reason for the stack to forward
  traffic between the two independent network interfaces yet. A failure here
  is a healthy negative-control result, not a bug; a pass would be
  unexpected and worth investigating. The real "does the bridge actually
  bridge" milestone is PC ↔ other-node pinging *each other* directly without
  addressing either of the bridge's own IPs — not yet possible until MAC
  bridging is enabled, tracked as a separate future step.

### Full ping matrix executed — results

- **PC network setup caveat discovered first:** this session runs on the same
  PC as the "Ethernet 8" adapter (192.168.0.100, wired directly to the
  bridge's `eth1`) — but the PC also has a **Wi-Fi adapter on
  192.168.0.78**, i.e. two interfaces sharing the `192.168.0.0/24` subnet.
  Windows silently routed the first round of test pings over Wi-Fi instead
  of the intended Ethernet link (confirmed via `Get-NetIPAddress`), which
  would have produced misleading results (e.g. a "successful" ping possibly
  hitting an unrelated Wi-Fi host rather than the bridge). Fixed by forcing
  the source address explicitly (`ping -S 192.168.0.100 <target>`) for every
  PC-side test below. User independently confirmed the same dual-IP setup.
- **cli.py quoting gotcha hit while running these tests:** invoking
  `cli.bat` through `cmd /c "...\"ping 1.2.3.4\"..."` from Git Bash silently
  split the quoted `"ping 192.168.0.202"` argument into two separate
  single-word commands (each producing `unknown command`) — the nested
  quote levels (bash → `cmd /c` → `cli.bat`'s `%*`) don't survive intact for
  multi-word arguments, even though the exact same pattern works fine for
  single-word commands (`reset`, `netinfo`, already used earlier this
  session without issue). **Fix: call `scripts\cli.py` directly with the
  sister project's venv Python, bypassing `cli.bat`/`cmd /c` entirely** —
  removes one quoting layer and preserves multi-word quoted arguments
  correctly:
  ```
  PYTHONIOENCODING=utf-8 "C:\work\t1s_bridge\bridge\t1s_100baset_bridge\.venv\Scripts\python.exe" "scripts\cli.py" --port COM8 --read 8 "ping 192.168.0.202 i eth0"
  ```
  Prefer this form over the `cli.bat`/`cmd /c` wrapper for any future
  multi-word CLI command from this environment.
- **Results:**

  | From → To | Source IP | Target | Result |
  |---|---|---|---|
  | PC → Bridge `eth0` | 192.168.0.100 (forced with `-S`, see caveat above) | 192.168.0.11 | ❌ timeout (100% loss) |
  | PC → Bridge `eth1` | 192.168.0.100 (forced with `-S`) | 192.168.0.12 | ✅ 4/4 replies |
  | PC → other T1S node | 192.168.0.100 (forced with `-S`) | 192.168.0.202 | ❌ destination host unreachable |
  | Bridge → other T1S node, **no interface specified** | ambiguous/undetermined (this is exactly the ambiguity described below) | 192.168.0.202 | ❌ 0/4 replies |
  | Bridge → other T1S node, **`i eth0`** | 192.168.0.11 | 192.168.0.202 | ✅ 4/4 replies, ~1 ms |
  | Bridge → PC, **`i eth1`** | 192.168.0.12 | 192.168.0.100 | ✅ 4/4 replies, ~1 ms |
  | Other T1S node → Bridge `eth0` | 192.168.0.202 | 192.168.0.11 | ✅ 4/4 replies, 1-2 ms |
  | Other T1S node → PC | 192.168.0.202 | 192.168.0.100 | ❌ 0/4 replies |
  | Other T1S node → Bridge `eth1` (diagnostic) | 192.168.0.202 | 192.168.0.12 | ❌ 0/4 replies (expected negative control) |

- **Interpretation:** every ping between two *directly, physically connected*
  peers succeeds (PC↔`eth1`, `eth0`↔other node). Every ping that would
  require traffic to cross from one bridge interface to the other, or reach
  a device only reachable through the bridge without addressing the bridge
  itself, fails — consistent with the MAC bridge still being disabled.
  Nothing here contradicts the plan; this is exactly the expected
  intermediate state before bridging is turned on.
### MAC bridge found to be already enabled — outdated `CLAUDE.md` note

- When asked "where in MCC" to enable MAC bridging (the planned next step),
  checked the current model/generated state first instead of assuming it was
  still off (per the "not yet activated" note in `CLAUDE.md` section 3, dated
  2026-08-30 but apparently stale): found `TCPIP_NETWORK_MACBRIDGE_ADD_IDX0`
  and `_IDX1` already set to `true` (`User` override) in
  `tcpipNetConfig_0.yml`/`tcpipNetConfig_1.yml`, **and already generated**
  into `configuration.h` (`#define TCPIP_STACK_USE_MAC_BRIDGE` + full bridge
  config: 2 ports, 17 FDB entries, 8-packet pool) and `initialization.c`
  (`tcpipMacbridgeTable[2] = {{0},{1}}`, `tcpipBridgeInitData`, and a
  `{TCPIP_MODULE_MAC_BRIDGE, &tcpipBridgeInitData}` entry in
  `TCPIP_STACK_MODULE_CONFIG_TBL`).
- MCC location for this field (for reference): component **`NETCONFIG`** →
  **Instance 0** / **Instance 1** (the Data Link Layer graph blocks used
  earlier to fix `eth1`'s IP) → checkbox **"Add to MAC Bridge"**.
- This means the MAC bridge was **already structurally active** during the
  full ping-matrix test round above — yet none of the cross-interface pings
  (PC↔`eth0`, PC↔node, node↔PC) worked. Either the flashed firmware at test
  time predated this config being generated, or bridging is enabled in
  config but not actually forwarding for some other reason — undetermined,
  needs the retest below to distinguish.
- User is confident the firmware already tested does stem from this
  bridge-enabled configuration, but as a precaution is redoing **Generate
  Code → Build All → flash** in MPLAB X before the next round of testing.
  **Still to do once that lands:** re-run the three still-failing rows from
  the ping matrix (PC↔`eth0`, PC↔node, node↔PC) — if the MAC bridge is
  genuinely working, these should now succeed without needing to address the
  bridge's own IPs.

### Retest after Generate Code → Build All → flash — bridge still not forwarding

- User redid Generate Code, Build All, and flash in MPLAB X (precaution, was
  already confident the previously-tested firmware matched this config), then
  asked to re-run the full ping matrix. `netinfo` on COM8 first confirmed the
  interface state is unchanged (`eth0` 192.168.0.11, `eth1` 192.168.0.12, both
  Link UP). Full matrix re-run with the same commands as the previous round
  (direct `scripts\cli.py` invocations, `--port COM8`/`COM23`, PowerShell
  `ping -S 192.168.0.100 ...` for the PC side):

  | From → To | Before this reflash | After this reflash |
  |---|---|---|
  | PC → Bridge `eth0` (192.168.0.11) | ❌ timeout | ❌ timeout (unchanged) |
  | PC → Bridge `eth1` (192.168.0.12) | ✅ 4/4 | ✅ 3/3 (unchanged) |
  | PC → other T1S node (192.168.0.202) | ❌ unreachable | ❌ unreachable (unchanged) |
  | Bridge → node, no interface | ❌ 0/4 | ✅ **4/4 — changed** |
  | Bridge → node, `i eth0` | ✅ 4/4 | ✅ 4/4 (unchanged) |
  | Bridge → PC, `i eth1` | ✅ 4/4 | ✅ 4/4 (unchanged) |
  | Node → Bridge `eth0` | ✅ 4/4 | ✅ 4/4 (unchanged) |
  | Node → PC | ❌ 0/4 | ❌ 0/4 (unchanged) |
  | Node → Bridge `eth1` (diagnostic) | ❌ 0/4 | ❌ 0/4 (unchanged) |

- **Only one row changed:** an unqualified `ping <ip>` issued from the
  bridge's own console is no longer ambiguous (works now without `i eth0`).
  This affects only the bridge acting as its own IP host on `eth0`'s route,
  not actual frame forwarding between the two physical segments.
- **The three rows that actually prove bridging (PC↔`eth0`, PC↔node,
  node↔PC) are all still failing, unchanged by the reflash.** Conclusion:
  **the MAC bridge module is configured and generated
  (`TCPIP_STACK_USE_MAC_BRIDGE`, `tcpipMacbridgeTable`,
  `TCPIP_MODULE_MAC_BRIDGE` module entry — all confirmed present in the
  freshly generated `configuration.h`/`initialization.c`), but is not
  actually forwarding frames between `eth0` and `eth1` at runtime.** This is
  now the open problem, distinct from (and following on from) the two
  already-resolved issues this session (heap size, `eth1` IP typos).
  Not yet investigated: whether the bridge module is being started/attached
  to both ports correctly at runtime (vs. just present in the static config
  tables), whether `TCPIP_MAC_BRIDGE_MAX_PORTS_NO`/port-index wiring in
  `tcpipMacbridgeTable[2] = {{0},{1}}` actually matches the two active
  `NETCONFIG` instances, and whether there's a required additional runtime
  call (e.g. explicit bridge-start) beyond static init that's missing.

### Bridge diagnostics via the built-in `bridge` CLI command — root cause found

- The firmware already has a `bridge` console command (guarded by
  `TCPIP_STACK_MAC_BRIDGE_COMMANDS`, already `true`) with subcommands
  `status`, `stats [clr]`, `fdb show`/`reset`/`add`/`delete`, and (if
  `TCPIP_MAC_BRIDGE_EVENT_NOTIFY` is on) `register` — implemented in
  `tcpip_commands.c` around `F_Command_Bridge` /
  `F_Command_BridgeShowStats` / `F_Command_BridgeShowFDB`. `bridge fdb show`
  needs no extra config; `bridge stats` needs "Enable Statistics"
  (`TCPIP_MAC_BRIDGE_STATISTICS`) on to return real counters.
- User enabled both **"Enable Statistics"** and **"Enable Event Notify"** in
  the MAC Bridge's Advanced Settings in MCC (screenshot:
  `docs\images\tcpip-configurator-netconfig-advanced-settings.png`),
  regenerated, rebuilt, and reflashed.
- Confirmed this is the **only** activation mechanism for MAC bridging in
  MCC — there is no separate standalone "MAC Bridge" component in either
  project's model; bridging is derived purely from the two per-`NETCONFIG`
  "Add to MAC Bridge" checkboxes (`TCPIP_NETWORK_MACBRIDGE_ADD_IDX0`/`_IDX1`,
  both already `true` here), and MCC auto-generates the whole bridge module
  (including this Advanced Settings panel, nested under one `NETCONFIG`
  instance for lack of its own component) once ≥2 interfaces have it
  checked.
- **Diagnostic sequence run over COM8/COM23**
  (`scripts\cli.py --port COM8/COM23 --read N "bridge ..."`, same direct-
  Python invocation pattern as the ping tests):
  1. `bridge status` → `status: 2` (`SYS_STATUS_READY`) — module is up.
  2. `bridge fdb show` → 17-entry table, only 3 populated: two `static,
     host` entries for the bridge's own two MACs (`...a0:02` on port 0's
     side, `...a0:03` on port 1's side), and **one dynamic entry learned on
     port 1** (`c0:47:0e:ad:47:2a`, almost certainly the PC's NIC).
     **Nothing learned on port 0** — no entry at all for the other T1S
     node's MAC (`00:04:25:8e:8c:a1`), despite node↔bridge pings having
     succeeded repeatedly over `eth0`.
  3. `bridge stats` → confirms it: **`port 0 stats: pkts received: 0`**
     (T1S/`eth0`), vs. **`port 1 stats: pkts received: 95`**
     (100BASE-T/`eth1`). Port 0 does show `fwd mcast: 91` (i.e. the bridge
     *is* successfully flooding broadcast/multicast frames received on
     port 1 *out* through port 0) — so egress on port 0 works — but nothing
     inbound on port 0 ever reaches the bridge's RX/learn pipeline.
  4. To rule out the counts just being stale/pre-bridge-enable: generated
     fresh T1S-side traffic (`ping 192.168.0.100` from the other node over
     COM23 — still failed, 0/4, as expected) and re-ran `bridge stats`.
     `port 0 pkts received` stayed at **exactly 0**; only port 1's counters
     grew (95→119), from unrelated ambient broadcast traffic on the
     100BASE-T segment, not from the node's ping attempt. This rules out a
     timing/staleness explanation — port 0 genuinely never feeds anything
     into the bridge.
- **Root cause identified by diffing against the sister project:**
  `DRV_LAN865X_PROMISCUOUS_IDX0` = **`false`** in this project's
  `configuration.h`, vs. **`true`** in the sister project's (confirmed via
  its `drvExtMacLan865x_0.yml`, `User` override). With promiscuous mode off,
  the LAN865x driver only hands frames addressed to its own MAC (or
  broadcast) up to the stack — anything else is discarded at the driver
  level before the bridge's port-0 glue ever sees it. A bridge port
  fundamentally needs to see *all* traffic on its segment (to learn source
  MACs and make forwarding decisions for frames not addressed to itself),
  so a non-promiscuous bridge port is a textbook explanation for exactly
  this symptom (port 0 blind to inbound traffic, port 1 fine).
- **Fix location in MCC (not yet applied):** component **`LAN865x`** →
  **Instance 0** (the MAC-layer block in the Data Link Layer graph) →
  checkbox **"Promiscuous Mode"** (model id
  `DRV_LAN865X_PROMISCUOUS_IDX0`) — currently absent as a `User` override in
  this project's `drvExtMacLan865x_0.yml` (running on the component's
  default, `false`). Enable it, Generate Code, Build All, flash, then
  re-run `bridge fdb show`/`bridge stats` (expect a port-0 entry for the
  other node's MAC, non-zero port-0 `pkts received`) and the still-failing
  ping-matrix rows (PC↔`eth0`, PC↔node, node↔PC).

### Promiscuous-mode fix applied — partial success, new asymmetric bug found

- User enabled `DRV_LAN865X_PROMISCUOUS_IDX0` in MCC (LAN865x → Instance 0 →
  "Promiscuous Mode"), regenerated, rebuilt, reflashed. `netinfo` confirmed
  the board came back up unchanged (`eth0` 192.168.0.11, `eth1` 192.168.0.12,
  both Link UP).
- **`bridge stats` baseline right after reflash:** `port 0 pkts received:
  82` (was stuck at 0 before the fix) — confirms the promiscuous-mode fix
  worked, port 0 now genuinely receives traffic into the bridge.
- **`bridge fdb show` baseline:** now shows entries learned **on port 0**
  for the first time — including `00:04:25:8e:8c:a1`, which is exactly the
  other T1S node's MAC (matches its `netinfo` output over COM23 earlier).
  **Also found a second, unexpected dynamic entry learned on port 0:
  `00:04:25:ca:ce:d9`** — not a device the user has mentioned; flagged for
  the user to identify (a third node on the T1S bus? Stray/duplicate
  address?), not yet explained.
- **Re-ran the real bridging test** (PC↔`eth0` and PC↔node via PowerShell
  `ping -S 192.168.0.100 ...`, node↔PC via `scripts\cli.py --port COM23
  "ping 192.168.0.100"`): **all three still fail** — PC→`eth0` and PC→node
  now report `Request timed out` (previously `Destination host
  unreachable` — a minor behavior change, still a failure); node→PC still
  `0/4 replies`.
- **`bridge stats`/`bridge fdb show` re-checked after these ping attempts —
  revealed a precise, asymmetric root cause:**
  ```
  Δ port 0: pkts received +189, dest notme-ucast +5, fwd ucast +0 (stayed at 0 the whole time)
  Δ port 1: fwd ucast +5, fwd direct +5
  Δ FDB entry for the PC's MAC (learned on port 1): fwdPackets +5
  ```
  Interpretation: 5 unicast frames arrived on port 0 addressed to the PC's
  MAC and were **successfully forwarded port 0 → port 1** (the "5" lines up
  with the node's `ping 192.168.0.100` attempt reaching the PC). But **port
  0's `fwd ucast` counter never moved off 0 across the entire test run** —
  the bridge has forwarded broadcast/multicast in both directions
  (`fwd mcast` grows on both ports), and unicast port 0→port 1, but **has
  never once forwarded a unicast frame port 1 → port 0**, despite the
  node's MAC being correctly present in the FDB with a port-0 mapping. This
  exactly explains both remaining failures: the node's outbound requests
  reach the PC (port 0→1 works), but the PC's replies never make it back
  (port 1→0 unicast forwarding is dead) — and symmetrically, anything the
  PC initiates toward the node needs the same broken port 1→0 unicast path.
- **Conclusion:** promiscuous mode was real and necessary but not
  sufficient — there is now a distinct, more narrowly-scoped bug: **unicast
  forwarding *into* port 0 (LAN865x/T1S) never happens**, while unicast
  *out of* port 0 (learned from port 0, delivered to port 1) works, and
  broadcast/multicast works in both directions. Not yet investigated: what
  differs about the bridge's TX-to-port-0 path for forwarded frames vs. the
  LAN865x driver's own locally-originated transmissions (which do work fine
  — e.g. the earlier successful `ping ... i eth0` issued directly from the
  bridge's own console) — this is the next thing to dig into, likely in
  `drv_lan865x_api.c`'s TX entry point or how the bridge glue calls it for
  a forwarded (vs. locally-originated) packet.

### GMAC needs its own "promiscuous equivalent" too - `TCPIP_MAC_RX_FILTER_TYPE_ALL_ACCEPT`

- User asked whether GMAC also needs a promiscuous-mode-like setting (by
  analogy with the LAN865x fix above). Compared `TCPIP_GMAC_RX_FILTERS` in
  `configuration.h` between the two projects:
  - This project (before this fix): `BCAST_ACCEPT | MCAST_ACCEPT |
    UCAST_ACCEPT | CRC_ERROR_REJECT` only.
  - Sister project: additionally `MCAST_HASH_ACCEPT | UCAST_HASH_ACCEPT |
    CRC_ERROR_ACCEPT | MAXFRAME_ACCEPT | ALL_ACCEPT | FRAMEERROR_ACCEPT`.
  - `TCPIP_MAC_RX_FILTER_TYPE_ALL_ACCEPT` is GMAC's equivalent of the
    LAN865x promiscuous flag - plain `UCAST_ACCEPT` only accepts unicast
    frames addressed to GMAC's own MAC; without `ALL_ACCEPT`, any unicast
    frame addressed to someone else (e.g. the PC's MAC, needing forwarding
    to port 0) gets dropped by the GMAC RX filter before the bridge glue
    ever sees it. This lines up exactly with the observed
    `port 1: dest notme-ucast: 0` throughout every prior test - GMAC was
    silently discarding exactly the frames the bridge would need to relay
    toward the T1S side.
  - MCC field: component `GMAC` (under `TCP/IP STACK` -> `DATA LINK
    LAYER`) -> checkbox `TCPIP_GMAC_ETH_FILTER_ALL_ACCEPT`.
- User enabled this checkbox in MCC and regenerated - confirmed by re-reading
  `configuration.h`, which now includes
  `TCPIP_MAC_RX_FILTER_TYPE_ALL_ACCEPT` in `TCPIP_GMAC_RX_FILTERS` (file
  changed mid-investigation, timestamp 2026-08-30 19:52:01). Not yet
  built/flashed at the time this was found - user is doing that now, will
  report back before the next round of `bridge stats`/`bridge fdb show`/
  ping-matrix retesting.

### RESOLVED - full end-to-end bridging confirmed working

- User rebuilt and reflashed with the `TCPIP_GMAC_ETH_FILTER_ALL_ACCEPT` fix
  applied. `netinfo` confirmed the board came back up unchanged (`eth0`
  192.168.0.11, `eth1` 192.168.0.12, both Link UP).
- **The two tests that matter - PC and the other T1S node pinging each
  other directly, addressing neither of the bridge's own IPs - both
  succeeded:**
  ```
  PC (192.168.0.100) -> other T1S node (192.168.0.202): 4/4 replies, 1-3 ms
  Other T1S node (192.168.0.202) -> PC (192.168.0.100):  4/4 replies, 1-2 ms
  ```
  This is the actual proof of a working transparent Layer-2 bridge - traffic
  crosses from the 10BASE-T1S segment to the 100BASE-T segment and back
  without either endpoint needing to know the bridge exists.
- `bridge stats` confirms the fix symmetrically: **`port 0 fwd ucast: 9` and
  `port 1 fwd ucast: 9`** - both directions now forward unicast traffic
  (port 0's `fwd ucast` had been stuck at exactly 0 through every prior test
  in this session; now moving in lockstep with port 1).
- Minor anomaly noted, not blocking: a PC ping to the bridge's `eth0` IP
  (192.168.0.11) got a reply reporting source IP `192.168.0.12` (`eth1`'s
  IP) instead of `.11`. Likely a source-IP-selection quirk once both
  interfaces are bridged into one L2 segment; not yet investigated, doesn't
  affect the actual node<->PC bridging that was the goal.
- **Summary of the full root-cause chain this session, for anyone reading
  this log later:** the original boot failure (`GMAC MAC initialization
  failed`) was caused by an undersized TCP/IP heap and libc heap (fixed by
  raising `TCPIP_STACK_DRAM_SIZE` and the linker `heap-size` to match the
  sister project). Once both interfaces came up, `eth1` had no usable IP due
  to typo'd `User` overrides in the MCC model (fixed). The MAC bridge was
  then found to already be enabled in the model/generated code but not
  actually forwarding traffic, traced via the built-in `bridge`
  status/stats/fdb CLI commands to two separate missing promiscuous-mode
  equivalents - `DRV_LAN865X_PROMISCUOUS_IDX0` for the LAN865x/T1S port and
  `TCPIP_GMAC_ETH_FILTER_ALL_ACCEPT` for the GMAC/100BASE-T port - both now
  enabled and confirmed working end-to-end.
- **Open items for follow-up, not blocking the core bridge function:**
  - The unidentified third MAC learned on port 0 (`00:04:25:ca:ce:d9`,
    noted earlier) - still unexplained.
  - The `.11`/`.12` reply source-IP anomaly noted above.
  - `eth1`'s gateway still `0.0.0.0` (noted earlier as likely harmless).
  - `docs\bridge-configuration-manual.md` is still just a skeleton - now
    that a full working configuration has been reached, this is a good
    point to start filling in its sections from what was learned this
    session.

- **User's conclusion from this test round, confirmed correct:** both network
  interfaces (10BASE-T1S `eth0` and 100BASE-T `eth1`) are individually fully
  functional — link up, correct IP, and every directly-connected peer
  reachable — but the **bridge function itself is not yet present/active**.
  This cleanly separates two previously entangled problems this session: the
  original GMAC/PHY bring-up bug (heap size, resolved) is a *different*
  problem from bridging not being enabled yet (a *planned*, not-yet-done
  step, not a bug) — the ping matrix is the evidence that draws that line.
- **New finding, not previously anticipated:** now that `eth0` and `eth1`
  share the same subnet (`192.168.0.0/24`), an **unqualified `ping <ip>`
  issued on the bridge's own console became ambiguous** — `ping
  192.168.0.202` (no `i eth0`) failed outright (0/4), while the identical
  target with `i eth0` explicitly appended succeeded (4/4). The stack no
  longer reliably infers the right outgoing interface once both sides of the
  bridge are on one subnet. This is likely *expected/necessary* behavior
  once the MAC bridge is actually enabled (a true L2 bridge presents both
  ports as one interface, so the ambiguity resolves itself) — but it's worth
  keeping in mind while testing pre-bridge-activation: **always pass an
  explicit `i eth0`/`i eth1` when pinging from the bridge's own console**
  until bridging is turned on.
- **Next step:** enable the MAC bridge (`TCPIP_NETWORK_MACBRIDGE_ADD_IDXn` on
  both `NETCONFIG-0`/`NETCONFIG-1`, per section 3 of `CLAUDE.md`), regenerate,
  rebuild/flash, then re-run the currently-failing rows (PC↔`eth0`,
  PC↔node, node↔PC) — those should start working once the bridge actually
  forwards between its two ports, which is the real end-to-end proof of a
  working bridge.
- **Exact commands used for this test round** (PowerShell tool unless noted;
  working directory for the Bash commands was
  `apps\tcpip_iperf_lan865x\`), in the order they were run:

  ```powershell
  # PC-side, round 1 (before the dual-IP/Wi-Fi issue was caught - unreliable, see above)
  ping -n 4 192.168.0.11
  ping -n 4 192.168.0.12
  ping -n 4 192.168.0.202

  # confirm the PC's interfaces/IPs
  Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -like "192.168.0.*" -or $_.InterfaceAlias -like "*Ethernet*" } | Select-Object InterfaceAlias, IPAddress, PrefixLength

  # PC-side, round 2 (source address forced - the results actually reported above)
  ping -n 3 -S 192.168.0.100 192.168.0.11
  ping -n 3 -S 192.168.0.100 192.168.0.12
  ping -n 3 -S 192.168.0.100 192.168.0.202
  ```

  ```bash
  # Bridge-side, FAILED attempts (cli.bat / cmd /c quoting gotcha, see above) -
  # kept here as a record of what NOT to do, not as a working recipe:
  cd /c/work/t1s_bridge/bridge/harmony/net_10base_t1s/apps/tcpip_iperf_lan865x && \
    PYTHONIOENCODING=utf-8 MSYS_NO_PATHCONV=1 cmd /c "C:\work\t1s_bridge\bridge\harmony\net_10base_t1s\apps\tcpip_iperf_lan865x\cli.bat --port COM8 --read 4 \"ping 192.168.0.202\"" < /dev/null
  cd /c/work/t1s_bridge/bridge/harmony/net_10base_t1s/apps/tcpip_iperf_lan865x && \
    PYTHONIOENCODING=utf-8 MSYS_NO_PATHCONV=1 cmd /c "C:\work\t1s_bridge\bridge\harmony\net_10base_t1s\apps\tcpip_iperf_lan865x\cli.bat --port COM8 --read 4 \"ping 192.168.0.100 i eth1\"" < /dev/null

  # Bridge-side (COM8), working form - direct python.exe invocation:
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM8 --read 4 "ping 192.168.0.202" < /dev/null
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM8 --read 8 "ping 192.168.0.202" < /dev/null
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM8 --read 8 "ping 192.168.0.202 i eth0" < /dev/null
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM8 --read 8 "ping 192.168.0.100 i eth1" < /dev/null

  # Other T1S node-side (COM23):
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM23 --read 6 "netinfo" < /dev/null
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM23 --read 8 "ping 192.168.0.100" < /dev/null   # 1st try, ran out before final line
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM23 --read 8 "ping 192.168.0.11" < /dev/null
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM23 --read 8 "ping 192.168.0.100" < /dev/null   # 2nd try, still ran out
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM23 --read 8 "ping 192.168.0.12" < /dev/null
  PYTHONIOENCODING=utf-8 "/c/work/t1s_bridge/bridge/t1s_100baset_bridge/.venv/Scripts/python.exe" "scripts/cli.py" --port COM23 --read 12 "ping 192.168.0.100" < /dev/null  # 3rd try, --read 12 finally caught the "done" line
  ```

  Also confirmed earlier in this session (same direct-`cli.py` form, `--read 3`)
  for `reset` and `netinfo` on COM8 — those single-word commands worked fine
  even through the `cli.bat`/`cmd /c` wrapper; the quoting gotcha above only
  bit once a command had an embedded space.
- **Confirmed-working reference binary identified:** the user confirmed
  `C:\work\t1s_bridge\bridge\t1s_100baset_bridge\release\T1S_100BaseT_Bridge.hex`
  (built 2026-08-29, 565258 bytes) is a working build of the sister project —
  its 100BASE-T PHY comes up cleanly on that hardware. Note it predates the
  `peek`/`poke` CLI commands added to this project below and only has `dump`.
  Useful as a known-good fallback/comparison image if needed (e.g. to confirm
  the sister board's PHY still links with a binary independent of any source
  changes made since).

### Porting readiness check: what else the sister project has enabled in MCC

- Before porting the sister project's extra application features (packet
  mirroring, a test server, a "no-IP" test mode, persistent env config,
  LAN865x diagnostics - all plain user source files, not MCC components),
  compared the two projects' MCC component trees to find prerequisite gaps.
  Found three MCC components present in the sister project but missing here:
  **Telnet Server** (`TCPIP_STACK_USE_TELNET_SERVER`, needed for the
  sister's `TCPIP_TELNET_AuthenticationRegister()` call), **Net Presentation
  Layer** (`net_Pres`, the crypto/TLS component Telnet depends on), and
  **Emulated EEPROM** (needed by the sister's `env.c` persistent-config
  module). Confirmed via each project's `TCP_forwardSlash_IP STACK`
  component-folder listing and `configuration.h`'s `TCPIP_STACK_USE_*`
  defines. The custom source files themselves (`env.c/h`,
  `lan865x_diag.c/h`, `noip_test.c/h`, `port_mirror.c/h`, `testserver.c/h`)
  sit directly under the sister's `firmware\src\` (same level as `app.c`),
  not under `config\default\`, so they need no MCC component of their own -
  just the three prerequisites above, and a plain file copy afterward.
- Discussed with the user how to port these without losing the ability to
  keep configuring the project with MCC afterward: every prerequisite
  component must be added via the MCC GUI + Generate Code (never by copying
  the sister project's already-generated files into `config\default\`,
  which would leave MCC unaware of them and at risk of being clobbered or
  left inconsistent on a future regenerate); the plain user files are safe
  to copy directly into `firmware\src\` since MCC never touches that
  location. Recommended an incremental approach: add one MCC component at a
  time, Generate Code + build + verify after each, before moving to the
  next - isolates whether any build failure comes from the newly-added MCC
  component itself or from code ported afterward.
- User enabled **Emulated EEPROM** and **Telnet Server** in MCC and
  generated - confirmed via `configuration.h` picking up `EMULATED_EEPROM0`
  and (further down, not yet re-read in full) the Telnet defines.

### Telnet authentication callback ported into `app.c`

- User asked for a Telnet authentication callback in `app.c`, matching the
  sister project's approach (a password check). Found it in the sister's
  `app.c`: `TelnetAuthenticationHandler(user, password, pInfo, hParam)` -  a
  hardcoded check (`user == "admin"`, `password == "password"`) via
  `strcmp()`, printing "Telnet Access Authenticated/Declined" and returning
  `true`/`false` accordingly; registered once in `APP_Initialize()` via
  `TCPIP_TELNET_AuthenticationRegister(TelnetAuthenticationHandler,
  &TelnetHandlerParam)`. Confirmed the `TCPIP_TELNET_AUTH_HANDLER` signature
  in this project's (newer) `telnet.h` is unchanged from what the sister
  project's package version expects - safe to port as-is, no API drift.
- Ported into this project's `app.c` (a user file, not MCC-generated):
  added `#include <string.h>` (for `strcmp`) and
  `#include "config/default/library/tcpip/telnet.h"`, the
  `TelnetAuthenticationHandler()` function and `TelnetHandlerParam`
  variable (placed in the Application Callback Functions section, next to
  `TCPIP_STACK_InitCallback`), and the registration call at the top of
  `APP_Initialize()`.

### Recurring MCC generator bug: missing `#include <stdarg.h>` - second occurrence, in `telnet.c`

- Immediately after enabling Telnet and regenerating, the build failed:
  ```
  ../src/config/default/library/tcpip/src/telnet.c: In function 'F_Telnet_PRINT':
  telnet.c:427: error: implicit declaration of function 'va_start' [-Werror=implicit-function-declaration]
  telnet.c:429: error: implicit declaration of function 'va_end' [-Werror=implicit-function-declaration]
  ```
  Same bug class already documented in `CLAUDE.md` section 3 for
  `drv_lan865x_api.c`'s `PrintRateLimited()` - MCC's code generator omits
  `#include <stdarg.h>` in generated files that use `va_start`/`va_end`,
  with no MCC GUI field to fix it (it's a generator bug, not a
  configuration option). This is the **second** distinct generated file hit
  by the same bug in this project.
- Fixed by adding `#include <stdarg.h>` directly to
  `firmware\src\config\default\library\tcpip\src\telnet.c` (a deliberate,
  documented exception to the "never hand-edit generated code" rule, since
  there is no MCC-side fix available). `CLAUDE.md` section 3's existing
  entry was broadened to cover both occurrences and to flag this as a
  recurring pattern to keep checking for, rather than filed as a new,
  separate entry.
- **Reminder for next time this file (or any other file using
  `va_start`/`va_end`) gets regenerated:** check whether the include
  survived the regenerate, same as the existing `drv_lan865x_api.c`
  reminder.

### DEFERRED: Telnet login still shows "Access Denied" despite the handler registering OK

- User tried logging in via Tera Term (`admin`/`password`) after the Telnet
  callback was ported - got "Access Denied" every time.
- First diagnostic round: added a temporary debug print inside
  `TelnetAuthenticationHandler()` (`Telnet auth attempt: user="..."
  password="..."`) to see the raw received credentials. User reported
  **nothing at all appeared on COM8** during a login attempt - meaning the
  handler itself was never being invoked (in `telnet.c`'s
  `F_Telnet_LogonCheck()`, `authRes` only gets set by calling
  `telnetAuthHandler` if it is non-`NULL`; if it's `NULL`, `authRes` stays
  at its initial `false` with zero console output - exactly matching the
  symptom).
- Traced the registration/reset ordering in the generated code to check
  whether this was plausible: `initialization.c`'s `SYS_Initialize()` calls
  `TCPIP_STACK_Init()` (line 811) before `APP_Initialize()` (line 818);
  `TCPIP_STACK_RUN_TIME_INIT` is `false` in this project, so all TCP/IP
  module inits (including Telnet's, which resets
  `telnetAuthHandler = NULL` - `telnet.c` line ~317) run synchronously
  inside `TCPIP_STACK_Init()`, i.e. before `APP_Initialize()` registers our
  handler - the ordering looked correct on paper, matching the sister
  project's identical pattern.
- Added a second temporary diagnostic: print the return value of
  `TCPIP_TELNET_AuthenticationRegister()` at boot (`Telnet auth handler
  registration: OK/FAILED`) - fires at startup, checkable with a plain
  `reset` over COM8, no need to coordinate timing with a live Tera Term
  attempt. **Result: registration succeeds ("OK").** So the handler *is*
  registered, yet still never gets invoked (or gets invoked but something
  else about the flow still ends in a denial) when an actual Telnet client
  connects - narrowed down but not yet resolved.
- Attempted to self-diagnose further by scripting a simultaneous COM8
  listener + Telnet client (`scratchpad/telnet_diag.py`, pyserial + raw
  socket) to capture both sides of a login attempt in one shot - blocked by
  `COM8` already being held open (`PermissionError: Access denied` from
  `pyserial`), most likely MPLAB X's own terminal/console view holding the
  port. Not yet retried with that closed.
- **Deferred at the user's request** to move on to other work. State to
  pick back up from: registration confirmed working, actual connection
  still denied by something not yet identified; the two temporary debug
  `SYS_CONSOLE_PRINT()` calls are still in `app.c` (harmless, left in place
  on purpose so the next session doesn't have to re-add them) and should be
  removed once Telnet login is confirmed working.

---

<!-- Append new dated entries above this line as work continues. -->
