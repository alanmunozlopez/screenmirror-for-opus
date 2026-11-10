# ScreenMirror for OPUS

Plugin para recuperar en el ordenador la pantalla de un Teenage Engineering **OP-1 original**, incluido el menú de OPUS 2. Autor: [alanmunozlopez](https://github.com/alanmunozlopez).

La versión actual es **0.14.1**. Una vez instalado y cargado por OPUS, comienza a capturar y transmitir por USB MIDI en cada arranque; el perfil inicial es `fast`. No necesita abrir un menú del instrumento para empezar a recibir imagen.

| Perfil en OPUS | Resolución y color | Visor compatible |
|---|---|---|
| `fast` — predeterminado | 240×120, RGB332, 256 colores; prioridad a respuesta | macOS y Chrome |
| `balanced` | 320×160, RGB332, 256 colores; más detalle | macOS y Chrome |
| `quality` | Resolución física 320×160, 376×188 o 400×200; RGB565 | App macOS 0.14.0 |

Las actualizaciones regionales, compresión sin pérdida y descarte de trabajo antiguo reducen lo enviado. Los perfiles mayores pueden tardar más cuando cambia toda la pantalla; no se prometen 30 FPS.

## Los tres proyectos

- **Este repositorio:** plugin Blackfin, pruebas del emisor y paquetes `.opmod`.
- [screenmirror-desktop](https://github.com/alanmunozlopez/screenmirror-desktop): app nativa **macOS Apple Silicon**, compatible con los tres perfiles. Windows todavía no está implementado.
- [screenmirror-web](https://github.com/alanmunozlopez/screenmirror-web): visor Chrome mediante Web MIDI en localhost/HTTPS. Admite `fast` y `balanced`; todavía no admite `quality`.

## Instalar el plugin

Requiere un OP-1 **OG con OPUS 2 ya instalado**, basado en el firmware #246 y con los imports del manifiesto disponibles. No es un plugin para OP-1 Field ni para Hundekraft. El capturador usa un hook específico de #246, comprobado por bytes; no presupone una API universal de framebuffer.

1. Descarga [releases/0.14.1/screenmirror.opmod](releases/0.14.1/screenmirror.opmod). Conserva fuera del disco del OP-1 una copia de tu plugin anterior y de sus ajustes.
2. Pon el instrumento en modo disco. **Cuando macOS reconozca el disco**, sustituye únicamente `mods/screenmirror.opmod`. Mantén una sola copia con el ID `alan.screenmirror`.
3. Expulsa el disco y reinicia siguiendo el procedimiento de plugins de OPUS. El plugin arranca automáticamente cuando OPUS lo carga.
4. Abre el visor de escritorio o el visor web. El receptor se conecta al OP-1 disponible; Chrome solicita permiso Web MIDI/SysEx en el navegador la primera vez. No hay configuración de streaming en el instrumento.

Para cambiar de perfil: **SYNTH + DRUM + TAPE + MIXER → T2 → selecciona screenmirror con el encoder azul → pulsa azul → MIRROR MODE → gira verde**. El cambio no requiere reiniciar. OPUS guarda el valor después de dos segundos sin cambios; en el siguiente arranque se usa esa elección. `fast` es el valor inicial cuando no hay un ajuste guardado.

Si necesitas volver atrás, restaura tu `.opmod` anterior y su visor. También se conserva [0.11.0, perfil rápido](releases/0.11.0/screenmirror.opmod), la referencia histórica confirmada antes de añadir los tres modos. Los demás paquetes históricos se describen en [releases/README.md](releases/README.md). Cambiar el plugin no requiere reflashear. TE-BOOT queda como recuperación del firmware mediante el procedimiento de Teenage Engineering si fuese necesario, no como parte de esta instalación.

## Probar sin OP-1

Requiere Python 3 y un compilador C con AddressSanitizer/UBSan (`cc`, Clang o GCC).

```sh
git clone https://github.com/alanmunozlopez/screenmirror-for-opus.git
cd screenmirror-for-opus
sh scripts/test-host.sh
```

Ejecuta los **26 casos del controlador real**, pruebas del codec contra un modelo Python independiente y un corpus generado con los motores C reales para tres geometrías. Los headers mínimos de `tests/sdk-stubs/` solo proporcionan declaraciones; ninguna llamada abre un dispositivo. Para repetir las pruebas C con los headers completos del SDK fijado:

```sh
SCREENMIRROR_TEST_SDK="$PWD/.work/opus/plugin-kit/sdk" sh scripts/test-host.sh
```

El corpus del emisor se genera en `build/fixtures/`; la reconstrucción exacta de esos fotogramas se comprueba también en el repositorio de escritorio. Consulta [docs/VALIDATION.md](docs/VALIDATION.md).

## Compilar el `.opmod`

El SDK público se obtiene desde [LizardMan245/opus](https://github.com/LizardMan245/opus/tree/43b18c97ca0cde430b7d176ca4388fd5d1efeb7d/plugin-kit), fijado al commit `43b18c97ca0cde430b7d176ca4388fd5d1efeb7d`. Python estándar es suficiente para empaquetar. Necesitas además una toolchain **bfin-elf GCC 13.3.0 / binutils 2.38**; el SDK y el compilador no están incluidos en este repositorio.

```sh
sh scripts/setup-sdk.sh
OP1_BFIN_TOOLCHAIN=/ruta/a/toolchain/install sh scripts/build.sh
```

Si ya tienes el SDK en otro checkout del commit indicado:

```sh
OPUS_PLUGIN_KIT=/ruta/a/opus/plugin-kit \
OP1_BFIN_TOOLCHAIN=/ruta/a/toolchain/install \
sh scripts/build.sh
```

El resultado se escribe en `build/modes/screenmirror.opmod`; **nunca sobrescribe** el paquete conservado en `releases/`. Para compilar y verificar su igualdad byte a byte con 0.14.1 y el puente ensamblado:

```sh
OP1_BFIN_TOOLCHAIN=/ruta/a/toolchain/install sh scripts/test-blackfin.sh
```

En macOS Apple Silicon se conserva el procedimiento de compilación nativa del compilador, con Xcode Command Line Tools, `make`, `curl` y Homebrew:

```sh
brew install gmp mpfr libmpc
sh scripts/build-blackfin-macos.sh
sh scripts/setup-sdk.sh
sh scripts/test-blackfin.sh
```

El script descarga fuentes GNU y verifica sus firmas antes de construir en `.work/`; puede tardar. La recompilación del plugin se ha comprobado en macOS con la toolchain ya construida. En Linux, `build.sh` usa la misma interfaz `OP1_BFIN_TOOLCHAIN` y Python; la construcción de una toolchain Linux nueva no se ha repetido como parte de esta separación. Véanse también las [instrucciones del SDK fijado](https://github.com/LizardMan245/opus/blob/43b18c97ca0cde430b7d176ca4388fd5d1efeb7d/plugin-kit/docs/getting-started.md).

Ningún script instala plugins, abre MIDI físico ni flashea firmware.

## Estado y límites

El usuario confirmó recepción de imagen en su OP-1 con **0.14.1 el 7 de noviembre de 2026**. Esto no equivale a medir por separado los tres modos, persistencia, carga sostenida o audio/MIDI en esa versión. Las pruebas sintéticas y el empaquetado sí se ejecutan localmente. [Arquitectura y transporte](docs/ARCHITECTURE.md) · [Validación](docs/VALIDATION.md) · [Historial](docs/HISTORY.md).

El plugin reserva aproximadamente **0,93 MB de RAM** mientras está cargado. Captura y transmite si el USB está configurado, incluso con el visor cerrado; todavía no incorpora suspensión por ausencia de receptor. El código no registra callbacks de audio. El límite de envío es una llamada cada 5 ms, no una garantía de recepción: la API de transmisión no ofrece ACK ni presión de cola.

## Licencia

GPL-3.0; véanse [LICENSE](LICENSE) y [NOTICE](NOTICE). Las atribuciones a OPUS y al SDK se conservan como procedencia de dependencias.
