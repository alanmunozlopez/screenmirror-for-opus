# Paquetes conservados

| Carpeta | Fuente | Uso |
|---|---|---|
| `0.14.1` | `src/modes/plugin` | Actual: fast/balanced/quality, fast inicial. Visor macOS 0.14.0; web solo fast/balanced. |
| `0.11.0` | `src/final240/plugin` | Referencia histórica rápida RGB332 240×120, compatible con el visor macOS actual y web. |
| `0.12.0` | `src/final320/plugin` | Referencia RGB332 320×160. |
| `0.13.0` | `src/native565/plugin` | Referencia HQ RGB565; requiere visor nativo, sin selección dinámica de los tres modos. |

No se instalan automáticamente. Conserva una copia de tu paquete anterior y sus ajustes antes de sustituirlo; mantén una sola copia con el ID `alan.screenmirror`. Para reproducir un perfil histórico usa `sh scripts/build.sh final240`, `final320` o `native565`; se escribe en `build/`, separado del artefacto distribuido.

Los checksums de todos los paquetes y fuentes se comprueban con `python3 scripts/verify-artifacts.py`. Las geometrías, memoria y permisos dependen del core instalado y de otros mods; el paquete histórico no evita esos requisitos.
