#include "usrlib.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_WRITERS 26
#define MAX_READERS 26
#define SEM_NAME_MAX_LEN 64
#define BUSY_WAIT_MIN_SPINS 20000u
#define BUSY_WAIT_SPIN_VARIATION 40000u

typedef struct {
  int pid;
  char letter;
  bool active;
} writer_info_t;

typedef struct {
  int pid;
  int color_idx;
  bool active;
} reader_info_t;

static writer_info_t writers[MAX_WRITERS];
static reader_info_t readers[MAX_READERS];
static char writer_index_args[MAX_WRITERS][4];
static char reader_index_args[MAX_READERS][4];

static volatile char mvar_value = 0;

static char empty_sem_name[SEM_NAME_MAX_LEN];
static char full_sem_name[SEM_NAME_MAX_LEN];
static bool sem_ready = false;

static int configured_writers = 0;
static int configured_readers = 0;

static const uint64_t reader_color_fds[] = {
  STDGREEN,
  STDBLUE,
  STDCYAN,
  STDMAGENTA,
  STDYELLOW,
  STDOUT
};

static const char *reader_color_names[] = {
  "Verde",
  "Azul",
  "Cian",
  "Magenta",
  "Amarillo",
  "Blanco"
};

#define READER_COLOR_COUNT (sizeof(reader_color_fds) / sizeof(reader_color_fds[0]))

static void random_busy_wait(void);
static void build_sem_name(char *buffer, const char *suffix);
static void copy_string(char *dst, const char *src);
static void concat_string(char *dst, const char *src);
static void store_index_string(int value, char *target, uint32_t target_len);
static void init_worker_state(void);
static int init_semaphores(void);
static void close_semaphores(void);
static int spawn_writer(int idx);
static int spawn_reader(int idx);
static void kill_writer(int idx);
static void kill_reader(int idx);
static void kill_all_workers(void);
static void print_command_summary(void);
static void controller_loop(void);
static void report_invalid_target(char target_type, int idx);

static int writer_process(int argc, char *argv[]);
static int reader_process(int argc, char *argv[]);

int mvar_main(int argc, char *argv[]) {
  if (argc != 2) {
    print_err("Usage: mvar <writers> <readers>\n");
    return -1;
  }

  int writers_requested = (int)satoi(argv[0]);
  int readers_requested = (int)satoi(argv[1]);

  if (writers_requested <= 0 || readers_requested <= 0) {
    print_err("Both writers and readers must be positive integers\n");
    return -1;
  }

  if (writers_requested > MAX_WRITERS || readers_requested > MAX_READERS) {
    print_err("Too many writers or readers requested\n");
    printf("Max writers: %d, max readers: %d\n", MAX_WRITERS, MAX_READERS);
    return -1;
  }

  configured_writers = writers_requested;
  configured_readers = readers_requested;
  init_worker_state();

  if (init_semaphores() < 0) {
    return -1;
  }

  for (int i = 0; i < configured_writers; i++) {
    if (spawn_writer(i) < 0) {
      kill_all_workers();
      close_semaphores();
      return -1;
    }
  }

  for (int i = 0; i < configured_readers; i++) {
    if (spawn_reader(i) < 0) {
      kill_all_workers();
      close_semaphores();
      return -1;
    }
  }

  print_command_summary();
  controller_loop();
  kill_all_workers();
  close_semaphores();
  print("\nmvar finished.\n");
  return 0;
}

// Simple busy wait to simulate the random active delay the spec requires.
static void random_busy_wait(void) {
  volatile uint32_t dummy = 0;
  uint32_t spins = BUSY_WAIT_MIN_SPINS + GetUniform(BUSY_WAIT_SPIN_VARIATION);
  for (uint32_t i = 0; i < spins; i++) {
    dummy += i;
  }
  (void)dummy;
}

static void copy_string(char *dst, const char *src) {
  while ((*dst++ = *src++)) {
  }
}

static void concat_string(char *dst, const char *src) {
  while (*dst) {
    dst++;
  }
  copy_string(dst, src);
}

static void store_index_string(int value, char *target, uint32_t target_len) {
  char buffer[DECIMAL_BUFFER_SIZE];
  num_to_str((uint64_t)value, buffer, 10);
  uint32_t len = (uint32_t)strlen(buffer);
  if (len >= target_len) {
    len = target_len - 1;
  }
  for (uint32_t i = 0; i < len; i++) {
    target[i] = buffer[i];
  }
  target[len] = 0;
}

static void build_sem_name(char *buffer, const char *suffix) {
  char pid_buffer[DECIMAL_BUFFER_SIZE];
  num_to_str((uint64_t)sys_getpid(), pid_buffer, 10);
  copy_string(buffer, "mvar_");
  concat_string(buffer, pid_buffer);
  concat_string(buffer, "_");
  concat_string(buffer, suffix);
}

static void init_worker_state(void) {
  for (int i = 0; i < MAX_WRITERS; i++) {
    writers[i].pid = -1;
    writers[i].letter = 'A' + i;
    writers[i].active = false;
    writer_index_args[i][0] = 0;
  }

  for (int i = 0; i < MAX_READERS; i++) {
    readers[i].pid = -1;
    readers[i].color_idx = i % READER_COLOR_COUNT;
    readers[i].active = false;
    reader_index_args[i][0] = 0;
  }

  mvar_value = 0;
}

static int init_semaphores(void) {
  build_sem_name(empty_sem_name, "empty");
  build_sem_name(full_sem_name, "full");

  if (sys_sem_open(empty_sem_name, 1) < 0) {
    print_err("Failed to create empty semaphore\n");
    return -1;
  }

  if (sys_sem_open(full_sem_name, 0) < 0) {
    print_err("Failed to create full semaphore\n");
    sys_sem_close(empty_sem_name);
    return -1;
  }

  sem_ready = true;
  return 0;
}

static void close_semaphores(void) {
  if (!sem_ready) {
    return;
  }
  sys_sem_close(empty_sem_name);
  sys_sem_close(full_sem_name);
  sem_ready = false;
}

static int spawn_writer(int idx) {
  store_index_string(idx, writer_index_args[idx], sizeof(writer_index_args[idx]));
  const char *args[] = { writer_index_args[idx], NULL };
  int pid = (int)sys_create_process(&writer_process, 1, args, "mvar_writer", NULL);
  if (pid < 0) {
    print_err("Failed to create writer process\n");
    return -1;
  }
  writers[idx].pid = pid;
  writers[idx].active = true;
  return 0;
}

static int spawn_reader(int idx) {
  store_index_string(idx, reader_index_args[idx], sizeof(reader_index_args[idx]));
  const char *args[] = { reader_index_args[idx], NULL };
  int pid = (int)sys_create_process(&reader_process, 1, args, "mvar_reader", NULL);
  if (pid < 0) {
    print_err("Failed to create reader process\n");
    return -1;
  }
  readers[idx].pid = pid;
  readers[idx].color_idx = idx % READER_COLOR_COUNT;
  readers[idx].active = true;
  return 0;
}

static void report_invalid_target(char target_type, int idx) {
  printf("No %s %d available\n", target_type == 'W' ? "writer" : "reader", idx);
}

static void kill_writer(int idx) {
  if (idx < 0 || idx >= configured_writers) {
    report_invalid_target('W', idx);
    return;
  }

  if (!writers[idx].active) {
    printf("Writer %c is already stopped\n", 'A' + idx);
    return;
  }

  if (sys_kill(writers[idx].pid) < 0) {
    print_err("Failed to kill writer\n");
    return;
  }

  sys_wait(writers[idx].pid);
  writers[idx].active = false;
  printf("Writer %c terminated\n", 'A' + idx);
}

static void kill_reader(int idx) {
  if (idx < 0 || idx >= configured_readers) {
    report_invalid_target('R', idx);
    return;
  }

  if (!readers[idx].active) {
    printf("Reader %c is already stopped\n", 'a' + idx);
    return;
  }

  if (sys_kill(readers[idx].pid) < 0) {
    print_err("Failed to kill reader\n");
    return;
  }

  sys_wait(readers[idx].pid);
  readers[idx].active = false;
  printf("Reader %c terminated\n", 'a' + idx);
}

static void kill_all_workers(void) {
  for (int i = 0; i < configured_writers; i++) {
    if (writers[i].active) {
      sys_kill(writers[i].pid);
      sys_wait(writers[i].pid);
      writers[i].active = false;
    }
  }

  for (int i = 0; i < configured_readers; i++) {
    if (readers[i].active) {
      sys_kill(readers[i].pid);
      sys_wait(readers[i].pid);
      readers[i].active = false;
    }
  }
}

static void print_command_summary(void) {
  printf("\nRunning mvar with %d writer(s) and %d reader(s)\n", configured_writers, configured_readers);
  print("Commands:\n");
  if (configured_writers > 0) {
    printf("  [%c-%c] Kill writer (uppercase letter)\n", 'A', 'A' + configured_writers - 1);
  }
  if (configured_readers > 0) {
    printf("  [%c-%c] Kill reader (lowercase letter)\n", 'a', 'a' + configured_readers - 1);
  }
  print("  [q] Quit (kills remaining writers and readers)\n\n");

  print("Writers:\n");
  for (int i = 0; i < configured_writers; i++) {
    printf("  %c -> pid %d\n", 'A' + i, writers[i].pid);
  }

  print("Readers:\n");
  for (int i = 0; i < configured_readers; i++) {
    const char *color = reader_color_names[readers[i].color_idx];
    printf("  %c -> %s\n", 'a' + i, color);
  }

  print("\nPress the corresponding key to kill a reader/writer or 'q' to stop everything.\n");
}

static void controller_loop(void) {
  while (1) {
    char cmd = getchar();
    if (cmd == 'q') {
      break;
    }

    if (cmd >= 'A' && cmd < 'A' + configured_writers) {
      kill_writer(cmd - 'A');
      continue;
    }

    if (cmd >= 'a' && cmd < 'a' + configured_readers) {
      kill_reader(cmd - 'a');
      continue;
    }

    if (cmd == '\n' || cmd == '\r') {
      continue;
    }

    printf("\nUnknown command '%c'. Use uppercase letters for writers, lowercase for readers, 'q' to quit.\n", cmd);
  }
}

static int writer_process(int argc, char *argv[]) {
  if (argc != 1) {
    return -1;
  }

  int idx = (int)satoi(argv[0]);
  if (idx < 0 || idx >= MAX_WRITERS) {
    return -1;
  }

  if (sys_sem_open(empty_sem_name, 0) < 0) {
    return -1;
  }
  if (sys_sem_open(full_sem_name, 0) < 0) {
    sys_sem_close(empty_sem_name);
    return -1;
  }

  char value = 'A' + idx;

  while (1) {
    random_busy_wait();
    sys_sem_wait(empty_sem_name);
    mvar_value = value;
    sys_sem_post(full_sem_name);
  }

  return 0;
}

static int reader_process(int argc, char *argv[]) {
  if (argc != 1) {
    return -1;
  }

  int idx = (int)satoi(argv[0]);
  if (idx < 0 || idx >= MAX_READERS) {
    return -1;
  }

  if (sys_sem_open(empty_sem_name, 0) < 0) {
    return -1;
  }
  if (sys_sem_open(full_sem_name, 0) < 0) {
    sys_sem_close(empty_sem_name);
    return -1;
  }

  int color_idx = idx % (int)READER_COLOR_COUNT;

  while (1) {
    random_busy_wait();
    sys_sem_wait(full_sem_name);
    char value = mvar_value;
    mvar_value = 0;
    sys_sem_post(empty_sem_name);
    sys_write(reader_color_fds[color_idx], &value, 1);
  }

  return 0;
}
