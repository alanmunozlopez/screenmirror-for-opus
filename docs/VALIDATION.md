# Validación de la exportación

Las pruebas son locales: no acceden al instrumento, USB MIDI físico ni discos montados. El paquete distribuido se conserva sin cambios.

## Comandos

```sh
sh scripts/test-host.sh
# Con SDK fijado y toolchain Blackfin:
OP1_BFIN_TOOLCHAIN=/ruta/a/toolchain/install sh scripts/test-blackfin.sh
```

- `verify-artifacts.py`: SHA-256 de fuentes de producción y cuatro paquetes históricos.
- `test_modes_independent.py`: **24 procesos con ASan/UBSan** ejecutan el controlador y el motor `sharp` reales (0.17.0). Comprueban que no se captura antes de ver USB ni con USB desconectado (contador de capturas de la telemetría), `screen mirror` en `off` desde el arranque y en vivo (ningún mensaje MIDI, captura fresca y sesión nueva al volver a `on`), el `send rate` persistido, por defecto y fuera de rango, que un `mode` guardado por 0.16.0 y otros nombres se ignoran, cambios de `send rate` sin reiniciar sesión, cambio de geometría del panel (sesión nueva a la anchura física), carrera de reserva, USB desconectado, límite de una captura cada 100 ms, rollover del contador CYCLES y fallo de asignación. Solo hay una reserva de 560.008 bytes, alineada a 32 bytes y comprobada antes de los callbacks.
- `test_lz_codec.py`: el codec C real se compara byte a byte con un modelo Python independiente, con guardas de salida, presupuesto de trabajo, entradas truncadas, longitudes inválidas y límite de entrada.
- `generate-fixtures.sh`: el controlador y el motor C reales generan un corpus sintético `sharp` para paneles 320, 376 y 400: siete escenas, cada una con keyframe y parche regional de una tesela, **42 publicaciones** en total (14 por panel) en una sola sesión. `SCREENMIRROR_TEST_SEND=0..3` repite el corpus con x1–x4. Los archivos `.syx` y referencias se escriben solo en `build/fixtures/`. [screenmirror-desktop](https://github.com/alanmunozlopez/screenmirror-desktop) comprueba la reconstrucción exacta con sus copias en `tests/fixtures/sharp/` y conserva el corpus 0.16.0 de cuatro modos en `tests/fixtures/modes/`.
- `test_final_hook_abi.py`: prueba estática del ensamblado real. Verifica 43 registros restaurados, argumentos originales, contexto DAG lineal para C, stack de 184 bytes y una única continuación al flush. Las pruebas negativas detectan quitar una restauración, el área de argumentos o el reset de L1.
- `test-blackfin.sh`: solo aplica a las fuentes de 0.14.1; en la rama experimental las fuentes difieren y la igualdad falla por diseño. La build 0.16.0 probada en el instrumento mide 22.728 bytes, SHA-256 `554a200dbc32cdbb0eef66a78bcff3f20ddf936ed3133b76af485bb72ceb49ab`. La build 0.17.0 (solo `sharp`, aún sin probar en el instrumento) mide 12.668 bytes, SHA-256 `81d8cf126666100b69d6eadc9f35bfc847598ea1b53c5350399944e9a490e12d`.
- Con fuentes 0.14.1, reconstruye 0.14.1 y exige igualdad byte a byte con `releases/0.14.1/screenmirror.opmod`, SHA-256 `5148263eb1a80e9edce3c9d821eb18b9c3b7331617f0e4ce75a547ae5446e36a`, **20.808 bytes**. El SDK muestra imagen de 11.592 bytes, zero fill de 278.412 y 10 imports.

Los headers de `tests/sdk-stubs/` contienen solo los tipos y prototipos necesarios para los mocks. El empaquetado siempre usa el SDK real fijado; la opción `SCREENMIRROR_TEST_SDK` permite repetir también los casos C con sus headers completos.

## Comprobado y pendiente en hardware

**0.15.0/0.16.0, 10 de noviembre de 2026:** el usuario probó en su OP-1 (panel 400×200) `balanced` y `quality` de x1 a x4 y `sharp` x4. Los SysEx se grabaron en el ordenador con una herramienta solo de entrada. Resultado: 0 errores CRC y 0 fragmentos perdidos, salvo lotes cancelados a propósito por un cambio de modo. Los tiempos están en el README. El usuario no percibió cambios en el audio con x4; no es una medición de audio. La planificación de un keyframe `sharp` ocupa main 22 ms de mediana y hasta 56 ms.

El usuario confirmó imagen con 0.14.1 el **7 de noviembre de 2026**. Las iteraciones anteriores de regiones, captura final del menú y perfil rápido se probaron en su instrumento. La confirmación de 0.14.1 tiene alcance limitado: no se presenta como una campaña medida de los tres modos.

Queda por medir en el dispositivo la latencia tecla→imagen, los tres perfiles durante edición de samples y Tape, pérdidas USB, audio/MIDI, uso sostenido de CPU y convivencia con todos los mods. La persistencia se verifica mediante mocks del contrato, no mediante lectura posterior de ajustes del OP-1. Tampoco se ha identificado la imagen exacta del core instalado.

Una secuencia de prueba manual útil es: comprobar imagen tras arranque sin abrir menús; cambiar fast→balanced→quality y volver; editar samples y accionar Tape sin REC; esperar dos segundos y reiniciar para comprobar persistencia; escuchar audio y verificar MIDI; comparar con el paquete anterior. Esta exportación no realiza esos pasos automáticamente.

La auditoría de colocación del core original requirió un `.ldr` local no incluido. La igualdad del `.opmod` y el análisis de su puente sí son reproducibles desde este repositorio con el SDK y la toolchain públicos. Las pruebas de simulación no sustituyen una medición del coste en Blackfin ni garantizan que otro conjunto de mods deje memoria suficiente.
