# follower_lan865x — Arbeitsanweisungen

T1S-Follower-Endpunkt-Firmware (`T1S_Follower.X`, PTP-über-10BASE-T1S-Zeitsynchronisation,
eine Schnittstelle, kein Bridging), portiert am 2026-08-31 aus dem Schwesterprojekt
`C:\work\t1s_bridge\bridge\t1s_100baset_bridge\follower\`. Analog zu
`apps\bridge_lan865x_100baseT\` (eigene `CLAUDE.md` dort) eine eigenständige App auf dieser
Ebene, unabhängig vom übrigen `net_10base_t1s`-Content-Repo.

## Harte Regel: dieses Projekt hat absichtlich KEIN MCC-Modell

Siehe [`firmware\T1S_Follower.X\KEIN_MCC_MODELL.md`](firmware/T1S_Follower.X/KEIN_MCC_MODELL.md)
für die volle Begründung (Kurzfassung: ein früheres Modell hier war das der *Bridge*, nicht des
Followers — „Generate Code" hätte aus dem Follower eine Bridge gemacht). **Niemals MCC „Generate
Code" gegen `firmware\src\config\default\` laufen lassen** — gebaut wird ausschließlich aus der
getrackten `nbproject\configurations.xml` heraus, per `batch\genmk.bat` (MPLAB X eigener
`prjMakefilesGenerator.bat`). Fehlt im generierten Code etwas, gehört der Fix in eine eigene
Quelldatei oder in `nbproject\configurations.xml`, nicht in ein MCC-Modell — es gibt keins.

## Bauen, Flashen, Konsole

```bat
setup.bat                 :: einmalig pro Rechner, nach dem Klonen (venv, pyOCD, Debug-Fix, Makefiles)
build.bat                 :: inkrementell (Default), TYPE_IMAGE=PRODUCTION
build.bat rebuild         :: clean + full
flash.bat                 :: pyOCD über EDBG-Probe
flash.bat --list          :: angeschlossene Probes
cli.bat "help"             :: Kommando über die serielle Konsole schicken
```

- **Eigene `.venv`**, kein geteiltes Repo-Root-Tooling — bewusst vereinfacht gegenüber dem
  Schwesterprojekt beim Flashen: kein rollenbasiertes `flash_boards.py`/`boards.json` (dort für
  zwei Follower A/B gedacht) — `flash.bat` flasht direkt ein einzelnes Board über pyOCD, bei
  mehreren angeschlossenen Probes mit `--probe <serial>` gezielt auswählen. `setup_compiler.py`
  (XC32-Versionsauswahl) ist dagegen **doch** dabei (seit 2026-08-31, zwei installierte
  XC32-Versionen auf diesem Rechner) — aber nur als Notiz, `build.bat` liest
  `setup_compiler.config` nicht (dieselbe Begründung wie im Schwesterprojekt: der einzige echte
  Konsument dort ist `build_summary.py`s `xc32-nm`, das es hier nicht gibt). Details:
  `apps\bridge_lan865x_100baseT\CLAUDE.md` Abschnitt 2.
- `batch\genmk.bat` nutzt dieselbe dynamische MPLAB-X-Versionserkennung wie
  `apps\bridge_lan865x_100baseT\batch\genmk.bat` (dort am 2026-08-31 gegen eine hartcodierte
  Versionsliste gefixt, die eine neuere installierte Version übersehen hätte — Details dort).
- **Board-Zuordnung auf dieser Werkbank** (aus `apps\bridge_lan865x_100baseT\scripts\iperf_matrix_test.py`):
  Follower A = `COM10` / `192.168.0.201`, Follower B = `COM23` / `192.168.0.202` — diese beiden
  physischen Boards liefen bisher mit Firmware, die über das Schwesterprojekt geflasht wurde;
  dieses Projekt hier bringt jetzt den Quellcode mit, ersetzt aber noch nicht automatisch, was auf
  den Boards läuft (`flash.bat` erst nach Bedarf ausführen, nicht proaktiv).
- **`build.bat` kopiert das Hex nach jedem erfolgreichen Build zusätzlich nach
  `release\T1S_Follower.hex`** (seit 2026-08-31, wie im Schwesterprojekt, dort eingecheckt —
  damit ein frischer Klon flashen kann, ohne vorher zu bauen). **Nur `build.bat` aktualisiert
  diese Kopie** — ein Build direkt aus der MPLAB-X-IDE lässt `release\` veraltet stehen.
  **`flash.bat` flasht standardmäßig genau diese `release\`-Datei** (seit 2026-08-31, vorher
  `dist\`) — um stattdessen einen frischen lokalen Build zu flashen, den `dist\`-Pfad explizit
  angeben: `flash.bat firmware\T1S_Follower.X\dist\default\production\T1S_Follower.X.production.hex`.
- Sonst gilt dieselbe Arbeitsweise wie im Schwesterprojekt: Build in MPLAB X durch den User
  selbst, nicht proaktiv `build.bat`/`flash.bat` aufrufen, um etwas „zu beweisen".
