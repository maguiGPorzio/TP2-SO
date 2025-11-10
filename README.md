# TP2 - Sistemas Operativos (ramOS)

![logo](./ramos_logo.png)

An Operating Sistem by Rodri, Azu and Magui


Implementamos un mini kernel de 64 bits con scheduler de prioridades, administración de memoria dinámica (dos variantes), semáforos con nombre, pipes (anónimos y nombrados) y una shell con soporte de procesos en foreground/background, pipes y atajos de teclado. Incluimos los tests de la cátedra y programas de usuario propios para demostrar cada requerimiento.

## Instrucciones de compilación y ejecución
1. Requisitos: Docker activo y QEMU instalado en el host.
2. Crear contenedor (una vez): `./create.sh` crea `tpe_so_2q2025` y monta el repo en `/root`.
3. Compilar:
   - `./compile.sh` construye Toolchain, Userland y Kernel en el contenedor con memory manager default.
   - `./compile.sh buddy` compila activando el Buddy allocator (`USE_BUDDY`).
4. Ejecutar: `./run.sh` lanza `qemu-system-x86_64` con `Image/x64BareBonesImage.qcow2` (512 MB) y backend de audio adecuado. Al boot, `init` crea la `shell` y luego queda en `hlt` como proceso idle.
5. Ciclo de trabajo: `./compile.sh && ./run.sh` tras cambios. Limpieza manual: `docker exec -it tpe_so_2q2025 make -C /root clean`.

## Instrucciones de replicación
### Comandos builtin de la shell
| Comando | Parámetros | Descripción |
| --- | --- | --- |
| `clear` | — | Limpia la pantalla en modo texto.
| `help` | — | Lista builtins y programas y explica como mandar un proceso a background y como conectar procesos mediante un pipe.
| `username` | `<new_name>` | Permite cambiar el nombre de usuario que se ve como prompt en la shell.

### Programas de usuario
| Programa | Parámetros | Descripción / Uso |
| --- | --- | --- |
| `ps` | — | Lista procesos: PID, estado, prio, PPID, FDs, stack pointers.
| `mem` | — | Usa `sys_mem_info` para total/usada/libre y bloques.
| `pipes` | — | Lista pipes activos: ID, nombre, FDs, readers/writers, bytes buffered.
| `time` | — | Muestra hh:mm:ss vía `sys_time`.
| `date` | — | Muestra dd/mm/yy vía `sys_date`.
| `echo` | `[args...]` | Imprime a STDOUT argumentos separados por espacios y emite EOF.
| `printa` | — | Loop que imprime a STDOUT el caracter 'a' indefinidamente con un delay.
| `cat` | — | Lee de STDIN y lo imprime en STDOUT.
| `red` | — | Lee de STDIN y lo imprime en STDERR; útil para testear FDs y pipes.
| `rainbow` | — | Lee de STDIN y los imprime haciendo round‑robin por FDs de color.
| `filter` | — | Filtra vocales de STDIN hasta leer EOF.
| `wc` | — | Cuenta líneas, palabras y caracteres de STDIN hasta EOF.
| `mvar` | `<writers> <readers>` | MVARS con semáforos nombrados por PID.
| `kill` | `<pid1> [pid2...]` | hace `sys_kill` de los PID que recibe por parametro.
| `block` | `<pid> [pid2...]` | hace `sys_block` de los PID que recibe por parametro.
| `unblock` | `<pid> [pid2...]` | hace `sys_unblock` de los PID que recibe por parametro.
| `nice` | `<pid> <prio>` | Cambia prioridad del proceso (0 mas alta, 2 mas baja).

### Tests de la cátedra
| Test | Parámetros | Descripción |
| --- | --- | --- |
| `test_mm` | `<max_memory>` | Stress de memoria: alloc/set/check/free + métricas.
| `test_prio` | `<max_iterations>` | Muestra fairness y efecto de `nice` (incluye bloqueados).
| `test_processes` | `<max_processes>` | Crear muchos, matar/bloquear/desbloquear aleatoriamente con `wait` de limpieza.
| `test_sync` | `<iterations> <use_semaphore>` | `0` reproduce carrera, `1` sincroniza con semáforo `sem`.
| `test_pipes` | — | Agregado por nosotros para testear pipes con nombre, crea un writer y un reader que se comunican a traves de un pipe con nombre

### Caracteres especiales para pipes y background
- Pipe: un `|` separa dos programas. Un solo pipe por línea (`left | right`). Kernel: buffer circular con semáforos por FD; al cerrar el último writer se despierta a los readers para que observen EOF.
- Background: `&` al final corre el proceso/pipeline en background. La shell adopta con `sys_adopt_init_as_parent` para que `init` recolecte.


### Atajos de teclado
- `Ctrl+C`: mata el proceso en foreground (`scheduler_kill_foreground_process()`) y si este estuviera conectado mediante un pipe a otro proceso tambien lo mata (porque lo consideramos del foreground group), devolviendo la `shell` al foreground.
- `Ctrl+D`: inyecta EOF al STDIN del lector en foreground.
- `+` / `-`: aumentan/reducen tamaño de fuente y redibujan prompt+input.

### Ejemplos, por fuera de los tests

- **Procesos y prioridades:**
  1. **Usando printa y printb**
     - Ejecutar `printa &` y `printb &` para correr ambos en background.
     - Al inicio deberían imprimirse aproximadamente la misma cantidad de letras `a` y `b`, ya que ambos tienen igual prioridad.
     - Luego usar `ps` para obtener los *pid* de cada uno y cambiar la prioridad de uno de ellos con `nice <pid> <nueva_prioridad>`.
     - Se podrá observar que el proceso con prioridad más alta (número menor) empieza a imprimir con mayor frecuencia que el otro, reflejando el efecto del scheduler sobre la asignación de CPU.

  2. **Usando MVar**
     - Ejecutar `mvar 2 2` y luego `ps` para ver los *pid* de los readers y writers.
     - Cambiar la prioridad de alguno de ellos con `nice <pid> <nueva_prioridad>` y observar cómo afecta la frecuencia y el color de los caracteres impresos.
     - Recordar que **0 es la prioridad más alta** y **2 la más baja**.

- **Pipes y filtros:**
  - `ps | rainbow` escribe la salida de `ps` con muchos colores.
  - `echo hola mundo | filter` produce `hl mnd` (sin vocales).
  - `cat | wc` permite escribir (no verás en pantalla lo que escribes porque se redirige a `wc`), y al finalizar con `Ctrl+D` se muestran las líneas, palabras y caracteres escritos.
  - `printa | red &` imprime ‘a’ de manera indefinida con un delay en background; mientras tanto, `pipes` muestra los pipes activos, FDs y bytes en buffer.
  - `test_pipes` crea dos procesos que se comunican mediante un pipe nombrado `"test_pipe"`.

- **Sincronización:**
  - `test_sync 10000 0` suele terminar con `Final value != 0`.
  - `test_sync 10000 1` termina con `Final value: 0`.

- **Memoria:**
  - `mem` muestra total/used/free/blocks; podés probar creando y matando procesos para ver cómo varían los valores.
  - `test_mm 1048576` imprime el estado de memoria en cada iteración.


### Requerimientos faltantes o parcialmente implementados
- Solo un pipe por línea; falta encadenado `cmd1 | cmd2 | cmd3` en la shell.
- Sin historial de comandos ni navegación con flechas.


## Limitaciones
- `INPUT_MAX` 128 bytes; líneas más largas se truncan.
- Builtins no participan en pipelines (no leen/escriben por FDs redirigidos), esto hace que no se pueda pipear `help`.
- Parser simple sin comillas ni escapes; separación por espacios.
- `mem` muestra métricas enteras aproximadas; no hay fraccionarios.
- cuando se mata un proceso no se liberan los recursos hasta que no se le hace wait (solo se cierran los fds abiertos).
- El tamaño del heap es de 32MB, pudiendo ser mayor

## Arquitectura y diseño (qué hicimos)
- Scheduler multicolas con prioridades y aging: tres colas (0 alta, 1 media, 2 baja), promoción por `AGING_THRESHOLD` y selección round‑robin por cola. `nice` reubica y ajusta `effective_priority`.
- Gestión de foreground/terminal: sólo el proceso foreground puede leer teclado; `Ctrl+C` mata foreground y resetea a shell; `Ctrl+D` envía EOF al consumidor correcto (STDIN o pipe).
- Procesos: `sys_create_process`, `sys_wait`, `sys_exit`, `sys_kill`, `sys_block/unblock`, `sys_nice`, adopción por `init` para background, y cierre de FDs abiertos al terminar. init funciona como idle cuando no hay procesos ready para correr. Cuando el padre de un proceso es init, se liberan los recursos automáticamente.
- Pipes: anónimos y nombrados con buffer circular, semáforos por FD, conteo de readers/writers, propagación de EOF y limpieza consistente al matar procesos conectados.
- Pipes nombrados: expuestos por syscalls `sys_open_named_pipe`, `sys_close_fd`, `sys_pipes_info` para compartir por nombre entre procesos no relacionados. Para ver como se usan se puede ver `test_pipes.c`. 
- Semáforos con nombre: API `sys_sem_*` usada en `test_sync` y `mvar`.
- Memoria dinámica: allocator por lista libre (first‑fit con coalescing y guard `MAGIC_NUMBER`); alternativa Buddy (bloques 2^k) activable con `./compile.sh buddy`.
- Servicios del kernel: RTC (`sys_time/date`), timer/sleep, video texto (tamaño de fuente), speaker/beep y primitivas gráficas.

## Citas de código 
- `Userland/tests/test_mm.c`, `test_prio.c`, `test_processes.c` y `test_sync.c` se basan en los tests de la cátedra y se adaptaron mínimamente.


## Errores de PVS que dejamos
- Los referidos a bmfs.c, naiveConsole.c, main.c porque son del repositorio base de la cátedra de Arquitectura de Computadoras

- En keyboard.c: 
"Array overrun is possible. The value of 'scancode' index could reach 128."
Se ignora pues sino en la línea 116 entraría en el else if (scancode > BREAKCODE_OFFSET)

- En scheduler.c:
"A part of conditional expression is always true: c >= 'A'."

"Array underrun is possible. The value of 'pid' index could reach -1."
Esto nunca ocurre porque antes llamamos a pid_is_valid(pid)

- En rainbow.c, red.c, wc.c, cat.c y filter.c: "EOF should not be compared with a value of the 'char' type. The '(c = getchar())' should be of the 'int' type."
Se ignora porque en el entorno de compilación del TP, el tipo char es signed por defecto, por lo que la comparación con EOF (−1) funciona correctamente. El warning aplica solo a entornos donde char es unsigned, lo cual no ocurre en nuestra plataforma