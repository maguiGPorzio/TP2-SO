# TP2 - Sistemas Operativos (ramOS)

Implementamos un mini kernel de 64 bits con scheduler de prioridades, administración de memoria dinámica (dos variantes), semáforos con nombre, pipes (anónimos y nombrados) y una shell con soporte de procesos en foreground/background, pipes y atajos de teclado. Incluimos los tests de la cátedra y programas de usuario propios para demostrar cada requerimiento.

## Instrucciones de compilación y ejecución
1. Requisitos: Docker activo y QEMU instalado en el host.
2. Crear contenedor (una vez): `./create.sh` crea `tpe_so_2q2025` y monta el repo en `/root`.
3. Compilar:
   - `./compile.sh` construye Toolchain, Userland y Kernel en el contenedor.
   - `./compile.sh buddy` compila activando el Buddy allocator (`USE_BUDDY`).
4. Ejecutar: `./run.sh` lanza `qemu-system-x86_64` con `Image/x64BareBonesImage.qcow2` (512 MB) y backend de audio adecuado. Al boot, `init` crea la `shell` y luego queda en `hlt` como proceso idle.
5. Ciclo de trabajo: `./compile.sh && ./run.sh` tras cambios. Limpieza manual: `docker exec -it tpe_so_2q2025 make -C /root clean`.

## Instrucciones de replicación
### Comandos builtin de la shell
| Comando | Parámetros | Descripción |
| --- | --- | --- |
| `clear` | — | Limpia la pantalla en modo texto.
| `help` | — | Lista builtins y programas (recuerda que `&` corre en background).
| `kill` | `<pid>` | Matar proceso. Protege `init`/`shell`. Devuelve errores específicos.

### Programas de usuario
| Programa | Parámetros | Descripción / Uso |
| --- | --- | --- |
| `cat` | `[args...]` | Imprime argumentos a STDOUT y emite EOF.
| `red` | — | Copia STDIN a STDERR; útil para testear FDs.
| `rainbow` | — | Enruta caracteres de STDIN round‑robin por FDs de color.
| `time` | — | Muestra hh:mm:ss vía `sys_time`.
| `date` | — | Muestra dd/mm/yy vía `sys_date`.
| `ps` | — | Lista procesos: PID, estado, prio, PPID, FDs, stack pointers.
| `printa`/`printb` | — | Loops que imprimen ‘a’/‘b’ (stress de planificación).
| `text` | — | Imprime “Hola 1” (útil para pipes simples).
| `loop` | — | PID + mensaje cada 2 s (`sys_sleep`).
| `nice` | `<pid> <prio>` | Cambia prioridad del proceso (0 alta, 2 baja).
| `wc` | — | Cuenta líneas de STDIN hasta EOF.
| `mem` | — | Usa `sys_mem_info` para total/usada/libre y bloques.
| `block` | `<pid>` | `sys_block` del PID.
| `unblock` | `<pid>` | `sys_unblock` del PID.
| `filter` | — | Filtra vocales de STDIN hasta leer `-`.
| `mvar` | `<writers> <readers>` | MVARS con semáforos nombrados por PID.

### Tests de la cátedra
| Test | Parámetros | Descripción |
| --- | --- | --- |
| `test_mm` | `<max_memory>` | Stress de memoria: alloc/set/check/free + métricas.
| `test_prio` | `<max_iterations>` | Muestra fairness y efecto de `nice` (incluye bloqueados).
| `test_processes` | `<max_processes>` | Crear muchos, matar/bloquear/desbloquear aleatoriamente con `wait` de limpieza.
| `test_sync` | `<iterations> <use_semaphore>` | `0` reproduce carrera, `1` sincroniza con semáforo `sem`.

### Caracteres especiales para pipes y background
- Pipe: un `|` separa dos programas. Un solo pipe por línea (`left | right`). Kernel: buffer circular con semáforos por FD; al cerrar el último writer se despierta a los readers para que observen EOF.
- Background: `&` al final corre el proceso/pipeline en background. La shell adopta con `sys_adopt_init_as_parent` para que `init` recolecte.
- Pipes nombrados: expuestos por syscalls `sys_open_named_pipe`, `sys_close_fd`, `sys_pipes_info` para compartir por nombre entre procesos no relacionados. Aún no hay comando de shell para abrirlos, pero están disponibles para programas.

### Atajos de teclado
- `Ctrl+C`: mata el proceso en foreground (`scheduler_kill_foreground_process()`), devolviendo la `shell` al foreground.
- `Ctrl+D`: inyecta EOF al lector en foreground (STDIN o extremo de pipe).
- `+` / `-`: aumentan/reducen tamaño de fuente y redibujan prompt+input.

### Ejemplos, por fuera de los tests
- Planificación y prioridades:
  - `printa &` y `printb &` luego `ps` para ver RUNNING; `nice <pid_a> 2` baja prioridad de ‘a’.
  - `test_prio 10000000` y observar orden de finalización; repetir con `&` y ajustar con `nice`.
- Pipes y filtros:
  - `rainbow | wc` cuenta caracteres coloreados provenientes de STDIN.
  - `cat hola mundo | filter` produce `hl mnd` (sin vocales).
- Sincronización:
  - `test_sync 10000 0` suele terminar con `Final value != 0`.
  - `test_sync 10000 1` termina con `Final value: 0`.
- Memoria:
  - `mem` muestra total/used/free/blocks; `test_mm 1048576` imprime estado en cada iteración.

### Requerimientos faltantes o parcialmente implementados
- Solo un pipe por línea; falta encadenado `cmd1 | cmd2 | cmd3` en la shell.
- Sin historial de comandos ni navegación con flechas.
- Algunos programas (p.ej. `printa/printb`) no ceden CPU explícitamente; pueden sesgar la planificación si se abusan en foreground.
- Pipes nombrados: implementados en kernel/syscalls; falta comando de shell para abrir/cerrar por nombre.
- La shell permite background para programas dependientes de STDIN (ej. `red`/`filter`) y pueden quedar bloqueados sin input.

## Limitaciones
- `INPUT_MAX` 128 bytes; líneas más largas se truncan.
- Builtins no participan en pipelines (no leen/escriben por FDs redirigidos).
- Parser simple sin comillas ni escapes; separación por espacios.
- `mem` muestra métricas enteras aproximadas; no hay fraccionarios.

## Arquitectura y diseño (qué hicimos)
- Scheduler multicolas con prioridades y aging: tres colas (0 alta, 1 media, 2 baja), promoción por `AGING_THRESHOLD` y selección round‑robin por cola. `nice` reubica y ajusta `effective_priority`.
- Gestión de foreground/terminal: sólo el proceso foreground puede leer teclado; `Ctrl+C` mata foreground y resetea a shell; `Ctrl+D` envía EOF al consumidor correcto (STDIN o pipe).
- Procesos: `sys_create_process`, `sys_wait`, `sys_exit`, `sys_kill`, `sys_block/unblock`, `sys_nice`, adopción por `init` para background, y cierre de FDs abiertos al terminar.
- Pipes: anónimos y nombrados con buffer circular, semáforos por FD, conteo de readers/writers, propagación de EOF y limpieza consistente al matar procesos conectados.
- Semáforos con nombre: API `sys_sem_*` usada en `test_sync` y `mvar`.
- Memoria dinámica: allocator por lista libre (first‑fit con coalescing y guard `MAGIC_NUMBER`); alternativa Buddy (bloques 2^k) activable con `./compile.sh buddy`.
- Servicios del kernel: RTC (`sys_time/date`), timer/sleep, video texto (tamaño de fuente), speaker/beep y primitivas gráficas.

## Citas de código y uso de IA
- `Userland/tests/test_mm.c`, `test_prio.c`, `test_processes.c` y `test_sync.c` se basan en los tests de la cátedra y se adaptaron mínimamente.