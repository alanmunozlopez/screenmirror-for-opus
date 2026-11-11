# Kept packages

| Folder | Source | Use |
|---|---|---|
| `0.14.1` | `src/modes/plugin` at that release | Latest package: fast/balanced/quality, fast by default. macOS viewer 0.14.0; the web viewer supports fast/balanced only. |
| `0.11.0` | `src/final240/plugin` | Historical fast reference, RGB332 240×120; works with the current macOS and web viewers. |
| `0.12.0` | `src/final320/plugin` | RGB332 320×160 reference. |
| `0.13.0` | `src/native565/plugin` | HQ RGB565 reference; needs the native viewer and has no runtime choice of the three modes. |

Nothing here is installed automatically. Keep a copy of your previous package and its settings before replacing it, and keep a single copy of the plugin in `mods/`. These packages use the ID `alan.screenmirror`; 0.17.0 and later use `severusnare.screenmirror`. To rebuild a historical profile, use `sh scripts/build.sh final240`, `final320` or `native565`; the output goes to `build/`, separate from the distributed artifact.

`python3 scripts/verify-artifacts.py` checks the checksums of every package and source. Geometries, memory and permissions depend on the installed core and the other mods; a historical package does not remove those requirements.
