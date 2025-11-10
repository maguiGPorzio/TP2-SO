# TP2 - Sistemas Operativos (ramOS)

## Instrucciones de compilación y ejecución
1. **Requisitos**: Docker Desktop activo.
2. **Crear el contenedor** (una sola vez): `./create.sh` genera el container `tpe_so_2q2025` montando el repo en `/root`.
3. **Compilar**:
   - `./compile.sh` reconstruye `Toolchain/` y el kernel dentro del container anterior.
   - `./compile.sh buddy` recompila habilitando el allocator buddy.
4. **Ejecutar**: `./run.sh` levanta `qemu-system-x86_64` con `Image/x64BareBonesImage.qcow2`. 
5. **Workflow recomendado**: `./compile.sh && ./run.sh` luego de modificar kernel o userland. Para un full clean manual puede usarse `docker exec -it tpe_so_2q2025 make -C /root clean`.

## Instrucciones de replicación
### Comandos builtin de la shell
| Comando | Parámetros | Descripción |
| --- | --- | --- |
| `clear` | — | Limpia el framebuffer en modo texto.
| `help` | — | Lista builtins, programas de usuario y recuerda que `&` lanza procesos en background.
| `kill` | `<pid>` | Envía `sys_kill` al PID indicado. Protege `init` y la shell; reporta códigos de error diferenciados.

### Programas de usuario
| Programa | Parámetros | Descripción / Uso |
| --- | --- | --- |
| `cat` | `[args...]` | Imprime los argumentos a STDOUT y emite EOF para destrabar pipes.
| `red` | — | Copia STDIN a STDERR; útil para probar dualidad de file descriptors.
| `rainbow` | — | Lee STDIN y reparte cada char round-robin entre STDOUT, STDBLUE, STDMAGENTA, etc.
| `time` | — | Lee la RTC vía `sys_time` y muestra hh:mm:ss en BCD.
| `date` | — | Similar a `time` pero muestra dd/mm/yy.
| `ps` | — | Snapshot de procesos (`PID`, estado, prio, PPID, FDs, stack pointers) usando `sys_processes_info`.
| `printa` | — | Loop infinito que imprime `a` en verde (stress de scheduler). Igual para `printb` con `b` magenta.
| `text` | — | Mensaje fijo "Hola 1", pensado para pipes sencillos.
| `loop` | — | Imprime "Hola" con el PID cada 2 s; sirve para ver `sys_sleep`.
| `nice` | `<pid> <prio>` | Cambia prioridad (1–10) del proceso objetivo mediante `sys_nice`.
| `wc` | — | Cuenta líneas de STDIN hasta EOF.
| `mem` | — | Consulta `sys_mem_info` e imprime memoria total/usada/libre y bloques asignados.
| `block` | `<pid>` | Llama a `sys_block` para pausar el proceso indicado.
| `unblock` | `<pid>` | Reactiva un proceso previamente bloqueado.
| `filter` | — | Consume STDIN y emite solo consonantes hasta leer `-`.
| `mvar` | `<writers> <readers>` | Test de sincronización multivariable: comparte un char mediante dos semáforos nombrados.

### Tests de la cátedra
| Test | Parámetros | Descripción |
| --- | --- | --- |
| `test_mm` | `<max_memory>` | Stress del memory manager: reserva/libera bloques hasta el límite pedido y reporta `sys_mem_info`.
| `test_prio` | `<max_iterations>` | Crea 3 procesos que cuentan hasta `max_iterations`, varía prioridades y verifica fairness/blocking.
| `test_processes` | `<max_processes>` | Spawnea `max_processes` instancias de `endless_loop` y luego las mata/bloquea/desbloquea aleatoriamente.
| `test_sync` | `<iterations> <use_semaphore>` | Corre pares de productores/consumidores sobre `global`. Valor 0 reproduce race condition; 1 sincroniza con semáforo `sem`.

### Caracteres especiales para pipes y background
- Pipe: un único `|` en la línea separa dos programas. Solo se admite un pipe por comando (`left | right`).
- Background: un `&` al final del comando (luego de los argumentos) marca el proceso o pipeline como background. La shell quita el `&` antes de parsear y adopta los PID vía `sys_adopt_init_as_parent` para que `init` recoja los zombies.

### Atajos de teclado
- `Ctrl+C`: `Kernel/drivers/keyboard.c` invoca `scheduler_kill_foreground_process()`; solo afecta al proceso en foreground.
- `Ctrl+D`: inyecta un EOF en el FD en foreground. Si es STDIN escribe directamente en el buffer; si viene de un pipe, envía `EOF` al write-end correspondiente.
- `+` / `-`: atajos propios de la shell para agrandar o achicar la fuente (redibuja prompt e input actual).

### Ejemplos de uso (extra tests)
```bash
# Mostrar procesos y memoria disponibles
ps
mem

# Probar pipes y filtros de colores
rainbow | wc
cat hola mundo | filter

# Sincronización multi-lector/escritor
mvar 2 3

### Requerimientos faltantes o parcialmente implementados
- Solo se soporta un pipe por línea. Falta el encadenado `cmd1 | cmd2 | cmd3`.
- No hay historial ni navegación con flechas; el teclado ignora UP/DOWN, por lo que no se puede re-ejecutar el último comando.
- No existe soporte para pipes nombrados ni para compartir un identificador acordado entre procesos no relacionados (requisito opcional del enunciado).

## Limitaciones conocidas
- `INPUT_MAX` es 128 bytes; líneas más largas se truncan silenciosamente.
- Los builtins siempre se ejecutan foreground y no heredan pipes, por lo que `clear | wc` no tiene efecto.
- El parser no valida comillas ni escape de espacios; cada argumento se separa únicamente por whitespace.
- Las métricas de `mem` reportan enteros truncados (no double), solo útiles como orientación.

## Citas y uso de IA
- `Userland/tests/test_mm.c`, `test_prio.c`, `test_processes.c` y `test_sync.c` derivan directamente de los tests publicados por la cátedra y fueron adaptados mínimamente.
