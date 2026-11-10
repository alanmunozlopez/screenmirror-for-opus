# Validación de la exportación

Las pruebas son locales: no acceden al instrumento, USB MIDI físico ni discos montados. El paquete distribuido se conserva sin cambios.

## Comandos

```sh
sh scripts/test-host.sh
# Con SDK fijado y toolchain Blackfin:
OP1_BFIN_TOOLCHAIN=/ruta/a/toolchain/install sh scripts/test-blackfin.sh
```

- `verify-artifacts.py`: SHA-256 de fuentes de producción y cuatro paquetes históricos.
- `test_modes_independent.py`: **26 procesos con ASan/UBSan** ejecutan el controlador y ambos motores reales. Comprueban el modo inicial y persistido, valores inválidos, cancelación de trabajo pendiente, cambios durante captura/reserva, USB desconectado, rollover del contador CYCLES, intervalo global de envío y tres fallos de asignación. Solo hay una reserva de 640.000 bytes, alineada a 32 bytes y comprobada antes de los callbacks; el puntero se conserva en las transiciones.
- `test_lz_codec.py`: el codec C real se compara byte a byte con un modelo Python independiente, con guardas de salida, presupuesto de trabajo, entradas truncadas, longitudes inválidas y límite de entrada.
- `generate-fixtures.sh`: el controlador/motores C generan un corpus sintético para paneles 320, 376 y 400. Cada panel contiene visitas a los tres modos en ambas direcciones y actualizaciones completas y regionales: **42 publicaciones** en total. Los archivos `.syx` y referencias se escriben solo en `build/fixtures/`. La reconstrucción exacta de los 42 estados se comprueba en [screenmirror-desktop](https://github.com/alanmunozlopez/screenmirror-desktop) mediante las copias sintéticas del mismo corpus.
- `test_final_hook_abi.py`: prueba estática del ensamblado real. Verifica 43 registros restaurados, argumentos originales, contexto DAG lineal para C, stack de 184 bytes y una única continuación al flush. Las pruebas negativas detectan quitar una restauración, el área de argumentos o el reset de L1.
- `test-blackfin.sh`: reconstruye 0.14.1 y exige igualdad byte a byte con `releases/0.14.1/screenmirror.opmod`, SHA-256 `5148263eb1a80e9edce3c9d821eb18b9c3b7331617f0e4ce75a547ae5446e36a`, **20.808 bytes**. El SDK muestra imagen de 11.592 bytes, zero fill de 278.412 y 10 imports.

Los headers de `tests/sdk-stubs/` contienen solo los tipos y prototipos necesarios para los mocks. El empaquetado siempre usa el SDK real fijado; la opción `SCREENMIRROR_TEST_SDK` permite repetir también los casos C con sus headers completos.

## Comprobado y pendiente en hardware

El usuario confirmó imagen con 0.14.1 el **7 de noviembre de 2026**. Las iteraciones anteriores de regiones, captura final del menú y perfil rápido se probaron en su instrumento. La confirmación de 0.14.1 tiene alcance limitado: no se presenta como una campaña medida de los tres modos.

Queda por medir en el dispositivo la latencia tecla→imagen, los tres perfiles durante edición de samples y Tape, pérdidas USB, audio/MIDI, uso sostenido de CPU y convivencia con todos los mods. La persistencia se verifica mediante mocks del contrato, no mediante lectura posterior de ajustes del OP-1. Tampoco se ha identificado la imagen exacta del core instalado.

Una secuencia de prueba manual útil es: comprobar imagen tras arranque sin abrir menús; cambiar fast→balanced→quality y volver; editar samples y accionar Tape sin REC; esperar dos segundos y reiniciar para comprobar persistencia; escuchar audio y verificar MIDI; comparar con el paquete anterior. Esta exportación no realiza esos pasos automáticamente.

La auditoría de colocación del core original requirió un `.ldr` local no incluido. La igualdad del `.opmod` y el análisis de su puente sí son reproducibles desde este repositorio con el SDK y la toolchain públicos. Las pruebas de simulación no sustituyen una medición del coste en Blackfin ni garantizan que otro conjunto de mods deje memoria suficiente.
