# Captura, memoria y transporte

La implementación actual está en `src/modes/plugin/`. Sus archivos de producción se conservan byte a byte respecto del workspace anterior. La carpeta compartida `src/lz-regions-source/` mantiene los includes relativos del codec sin modificar código de ejecución.

## Capturar el menú y las pantallas normales

`manifest.txt` instala un `place call` en `0x019AB8C8`, esperando `07e3caaf`: el flush de la pantalla final del firmware #246. A diferencia de `opus_on_frame`, esa ruta incluye el menú de OPUS. No es una dirección del menú del core ni una API pública y genérica para copiar framebuffers.

El puente `flush_entry.S` preserva 43 registros, deja los registros circulares L0–L3 a cero antes de llamar C, reserva el área de argumentos y encadena exactamente una vez al flush original. Usa 184 bytes de stack para el puente, además de los que necesite C. La prueba de ABI inspecciona el objeto Blackfin ensamblado y tiene controles negativos; no ejecuta instrucciones en una CPU Blackfin real.

El callback valida rango, alineación, modo de invalidación y una geometría de 320, 376 o 400 columnas. Lee RGB565 con el layout documentado por el SDK, `buf[256*(width-1-x)+y]`, copia un snapshot y no retiene el puntero de firmware. La captura se limita a una cada 100 ms y solo ocurre mientras main ha visto USB configurado y `screen mirror` está en `on`; si no, el callback vuelve sin leer píxeles. No asigna memoria, comprime ni llama a USB desde el callback gráfico.

## Modo único y ajustes

Desde 0.17.0 hay un solo motor, `sharp` (`rgb_sender.c`): RGB332 con la geometría física del panel. Su anchura se toma de cada captura y un cambio de geometría abre sesión nueva. El manifiesto ofrece `screen on,off` (etiqueta `screen mirror`, inicialmente `on`) y `send rate x1..x4`, inicialmente x4. `send` cambia `sm_burst` sin reiniciar la sesión. Cualquier otro nombre de ajuste, incluido un `mode` guardado por 0.16.0, se ignora.

Main publica `capture_allowed`, que solo él escribe. Lo baja cuando `screen` está en `off` o `fw_usb_configured()` falla, y en ese momento descarta el trabajo pendiente y la referencia. En `off` no envía nada, ni telemetría. Al reanudar marca como atendido el último snapshot publicado, de modo que nunca se envía una imagen tomada antes de la pausa, y después vuelve a permitir la captura; si la pausa venía de `off`, abre una sesión nueva. La reserva de píxeles se hace en `opl_init` aunque el ajuste esté en `off`, porque OPUS solo permite reservar al arrancar.

`opl_init` publica una sola vez el token `(1<<2)|SM_SHARP` después de preparar el motor. El token con época 0 queda reservado para el estado sin inicializar: mientras no haya token válido, el hook de pantalla no captura. Cada snapshot conserva el token con el que fue tomado.

Hasta 0.16.0 el manifiesto ofrecía también `fast`, `balanced` y `quality` (motor RGB565 `native_sender.c`), y cada cambio de modo creaba una época y una sesión nuevas. Las fuentes de perfiles anteriores se conservan en `src/final240`, `src/final320` y `src/native565`.

## Memoria y corrección 0.14.1

Desde 0.16.0 el motor RGB332 guarda en ese mismo pool sus buffers de imagen completa: RLE, salida LZ y candidato LZ (hasta 400×200). Solo main los usa. En 0.17.0, sin el motor RGB565, el pool contiene tres snapshots, referencia, RLE y dos buffers LZ: **560.008 bytes**. Los estáticos son **143.560 bytes** (6.996 de imagen + 136.564 de zero fill). LZSS4 sigue limitado a entradas de 65.534 bytes: en 376/400 columnas el keyframe usa LZ sobre el flujo RLE (transform 1) o RLE/raw, sin cambios de formato.

Una única reserva `opus_alloc` en `opl_init` (640.000 bytes hasta 0.16.0, 560.008 desde 0.17.0) guarda snapshots y referencia: memoria cero, alineación de 32 bytes y duración del arranque según el contrato de OPUS. Si falla, `opl_init` devuelve -1 antes de registrar callbacks. No hay reservas durante captura ni transmisión.

En 0.14.1 la imagen compilada contiene 11.592 bytes y 278.412 de zero fill: **290.004 bytes** de código/datos estáticos, más el pool dinámico de píxeles. La memoria total sigue cerca de 930 KB; el cambio evita colocar esos 640 KB en el segmento que contiene el código del hook.

0.14.0 tenía 929.880 bytes estáticos. En el core local auditado dev66, el pool cercano máximo era 693.856 bytes; el cargador lo colocaba lejos y el `CALL` quedaba fuera de alcance antes de `opl_init`. La corrección se recompila idéntica aquí. El SHA del core local auditado era `5394528efefcea4c2729d34f6f681dd42bb311d93fdb3b9bc2298835f935d566`; nunca se leyó la identidad del core instalado. Este repositorio no incluye ese `.ldr` ni presenta el análisis del core como una prueba autónoma ejecutable sin él.

## Formato y recuperación

Cada mensaje es SysEx con identificador no comercial `0x7d`, firma `SM`, versión, carga empaquetada a 7 bits y CRC16-CCITT. Los mensajes llevan sesión, secuencia, geometría, fragmentación y límites de tamaño. Los receptores validan CRC y límites antes de publicar píxeles.

- **v4 / RGB332:** regiones de 8×4 y referencia completa; elección entre raw, RLE y LZSS acotado según tamaño. Un cambio denso produce keyframe.
- **v5 / RGB565:** franjas independientes de 8 filas; color exacto de la captura física, actualización de las franjas cambiadas.
- **v3 / telemetría:** contadores separados de callbacks, capturas, planificación, envíos e interacción; no son ACK de recepción.

El codec LZSS tiene ventana de 4096 bytes, 4 candidatos de búsqueda, coincidencias de 3–18 bytes y un presupuesto de 8192 comparaciones por bloque de 1024 posiciones de entrada. La salida tiene capacidad explícita; si no cabe se elige una representación alternativa. La compresión y los envíos incrementales ocurren exclusivamente en main.

Se prioriza el snapshot más reciente sin mantener una cola creciente de imágenes antiguas. El intervalo compartido de transmisión es de al menos **5 ms desde el retorno de la llamada USB anterior**, también al cambiar entre motores. Desde 0.15.0, `send rate` permite enviar hasta 4 mensajes seguidos en una misma pasada tras ese intervalo; con x1 se mantiene un mensaje por pasada. Se comprueba la configuración USB antes de enviar. No se altera el transporte de audio ni se registran callbacks de audio.

`fw_midi_send` tiene retorno void: una llamada no prueba entrega y la cola puede descartar. No hay ACK/retransmisión por fragmento; keyframes periódicos, aproximadamente cada 8 segundos desde el último completado, permiten corregir una referencia perdida. Los decodificadores rechazan paquetes incoherentes y esperan una base válida. El protocolo robusto reduce errores observables, pero no convierte USB MIDI en un canal de vídeo garantizado.

El cierre del visor todavía no detiene la captura o el streaming si USB sigue configurado. Esa optimización necesitaría una señal de presencia del receptor; no se añade en esta exportación.
