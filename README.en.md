# TH18Coop

[简体中文](README.md)

A multiplayer mod for **Touhou 18: Unconnected Marketeers**, with two-player local co-op and two- or three-player online co-op. Players choose their own characters and starting cards through the game's native menus. Resources, money, cards and cooldowns are tracked separately.

## Getting started

Download the portable ZIP from [Releases](https://github.com/qqhh-5545/TH18Coop/releases), extract the entire folder and run **虹龙洞联机.exe**. Select your game directory in the launcher. You must provide a supported Japanese `th18.exe` or Chinese `th18c.exe` installation. Game files are not included; the Steam version is not supported.

The main buttons are in English. Other launcher settings and help remain in Chinese.

- **Local Co-op**: two players share one computer and select characters and cards in turn.
- **Online Co-op**: the host chooses two or three players and creates a session. Guests enter the host address, port and connection code. Keep the connection window open while playing.
- **Replay**: opens an isolated, replay-only game instance using the native replay list, stage selector and playback controls. Watching does not commit progress. Multiplayer recordings require the matching mod version.
- **键位** (key bindings): configure local P1, local P2 and your online controls separately. Click a field and press a key. Changes apply when the game is next started. Esc and Enter remain menu controls.
- **?**: gameplay and data-location help.

## Default controls

| Action | Local P1 / online | Local P2 |
|---|---|---|
| Move | Arrow keys | I / K / J / L (up / down / left / right) |
| Shoot / confirm | Z | G |
| Focus | Shift | F |
| Bomb / back | X | H |
| Use active card | C | U |
| Switch active card | D | O |

P1 and online controls use the native keyboard/controller configuration until you enable a custom keyboard profile. Local P2 has a separate keyboard profile. Local players must avoid overlapping bindings; online players can use different bindings on each computer.

## Online connections

Networking uses **TCP** with host-negotiated input buffering: 1–12 frames, default 2. This is exact-input synchronization, without prediction rollback. A connection waits when network jitter exceeds the buffer.

On a LAN, use the host's LAN address. For a TCP tunnel, forward the tunnel to the host's listening port, default **28180**, and give guests the tunnel hostname and external port. All players need matching game-core, mod and gameplay-configuration versions. Cosmetic preferences and physical key bindings are local.

## Gameplay

- Boss damage scales with the number of non-ghost participants: ×3/4 for two players, ×2/3 for three.
- Exhausting lives turns a player into a drifting ghost. Nearby teammates can donate a spare life by releasing fire and tapping focus five times. Ghosts also revive at stage clear or the end-of-stage shop.
- Tap shoot five times near an eligible teammate to transfer 1.00 power. Donations belong only to the recipient.
- Life/Bomb supplies, fragments, full-power supplies and consumable resource cards benefit the whole team. Ordinary small/large P items and money remain individual; ordinary P drop counts increase with the player count.
- Bombs grant living teammates a short protection window. Continue is disabled after a team wipe.
- The sidebar uses the game's loaded UI assets and native card animations. The local player's active card appears at the lower left.

See the launcher's help and [gameplay rules](docs/COOPERATIVE_RULES.md) for precise timing and limits.

## Saves and recordings

Co-op uses its own progressive save; single-player progress is not imported. The native `custom.exe` configuration is copied into the isolated profile without modifying the original.

Paths below are relative to the selected game directory:

- Persistent co-op save: `coop-data/ShanghaiAlice/th18`.
- Online sessions: `coop-data/sessions/<ID>/host`, `guest`, or `guest-p3`. Only a normally ended host session may back up and commit persistent progress.
- Multiplayer recordings: `coop-data/replay/*.rpy`. Copy these files to the same folder on a friend's computer to watch with the matching version.
- Temporary replay profiles: `coop-data/replay-sessions`; cleaned after normal viewing, retained on failure.
- Logs: `coop-data/logs` and `coop.log` beside the launcher module.

`game-path.txt` and `keybindings.ini` are stored beside the launcher. They are personal preferences and are not included in the portable download. Old raw input logs cannot be converted into multiplayer replays.

## Building

Use Windows PowerShell, Zig **0.13.0**, and the .NET Framework C# compiler:

```powershell
./build.ps1 -ZigPath C:/Tools/zig/zig.exe
./test.ps1 -ZigPath C:/Tools/zig/zig.exe
```

The automated tests use simulated engine fixtures and loopback networking; they do not automatically launch the original game. Full-length runs, every character/card combination and script-changing patches have not been exhaustively verified.

The project code is MIT-licensed. Bundled components retain their required licenses and attribution in `THIRD_PARTY_NOTICES.md` and `licenses/`. The game and its assets belong to their respective rights holders.
