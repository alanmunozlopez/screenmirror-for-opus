# Historial y procedencia

Este repositorio separa el plugin de los visores web y macOS. El workspace original, sus cinco commits y las ramas `main` e `improve/menu-legibility` se conservan aparte; no es necesario disponer de aquel historial para compilar aquí. El snapshot de desarrollo de referencia fue `e2e07295011a2d3d7a0d84b4500f41e082d6f69e`. Los archivos C, H, S y los manifiestos de los perfiles conservados mantienen sus bytes originales.

| Versión | Cambio y observación |
|---|---|
| 0.3–0.6 | Primeras capturas mono/color y resoluciones reducidas; la transmisión completa resultaba lenta. |
| 0.7 | Gray120 como referencia de respuesta; el salto de página de Drums se sentía antes que los cambios pequeños en Tape. |
| 0.8 | Actualización por regiones en gray120; el usuario la percibió más fluida. |
| 0.9 | Color 160×80 con regiones; legibilidad limitada y respuesta al límite cómodo para el usuario. |
| 0.10 | Compresión LZSS acotada y transporte por regiones: la variante 240×120 fue la mejor experiencia comunicada entonces. |
| 0.11 | Captura final antes del flush de firmware: incluye el menú OPUS que `opus_on_frame` no entregaba. Referencia rápida 240×120 preservada. |
| 0.12 | RGB332 a 320×160, menú y textos más legibles, mayor espera en cambios completos. |
| 0.13 | RGB565 a la resolución física, franjas independientes, visor HQ. |
| 0.14.0 | Un plugin con selección fast/balanced/quality y valor inicial fast. Falló su colocación en el core local auditado por exceso de BSS cercano. |
| 0.14.1 | Píxeles reservados una vez con `opus_alloc`; paquete 20.808 bytes, estáticos reducidos a 290.004 bytes. Imagen confirmada en el instrumento el 7 de noviembre de 2026. |
| 0.15.0 (experimental) | Ajuste `send rate` x1–x4 y captura RGB332 por bloques de 8 columnas (de ~20 ms a 5–14 ms en el hilo de pantalla). Medido en el instrumento el 10 de noviembre de 2026: `balanced` 492 → 54 ms por pantalla completa y `quality` 3,6 → 0,98 s de x1 a x4, sin pérdidas. |
| 0.16.0 (experimental) | Perfil `sharp`: RGB332 a resolución física sin escalar, predeterminado junto con x4. Buffers de compresión movidos al pool de `opus_alloc`; estáticos 180.240 bytes (12.640 + 167.600 zero fill). El usuario lo confirmó en su OP-1 el 10 de noviembre de 2026: 80 ms de mediana por pantalla completa, texto legible y audio sin cambios percibidos. |
| 0.17.0 (experimental) | Solo `sharp`: se retiran `fast`, `balanced` y `quality` (motor RGB565 v5) y el ajuste `mode`. Nuevo ajuste `screen mirror` on/off; sin USB o en `off` no se captura. Paquete de 22.728 a 12.668 bytes; estáticos 143.560 bytes (6.996 + 136.564 zero fill); reserva de píxeles de 640.000 a 560.008 bytes. Sin cambios de protocolo; la telemetría informa 0.17.0. Probado en el instrumento con OPUS build 5 y build 7 (beta 4.1) el 11 de noviembre de 2026. |

Se conservan los paquetes 0.11.0, 0.12.0 y 0.13.0 para referencia y recuperación compatible, junto con sus fuentes; no se incluye el paquete defectuoso 0.14.0. La valoración de fluidez histórica es subjetiva del usuario; no debe interpretarse como mediciones nuevas de latencia. El visor de escritorio actual tiene versión propia 0.14.0 y funciona con el plugin 0.14.1.
