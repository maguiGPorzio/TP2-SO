#include "usrlib.h"

#define MAX_SEM_NAME_LENGTH 64 // TODO: se comparte con Kernel
#define SEM_PREFIX "mvar_"
#define SEM_EMPTY_SUFFIX "empty_"
#define SEM_FULL_SUFFIX  "full_"

#define LETTER_POOL "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define LETTER_POOL_SIZE (sizeof(LETTER_POOL) - 1)

#define COLOR_COUNT 4
#define MIN_SLEEP_MS 10
#define SLEEP_JITTER_MS 30

static volatile char mvar_value = 0;
static char sem_empty_name[MAX_SEM_NAME_LENGTH];
static char sem_full_name[MAX_SEM_NAME_LENGTH];

static const int color_fds[COLOR_COUNT] = {STDOUT, STDGREEN, STDBLUE, STDMAGENTA, STDYELLOW};

static void build_sem_name(char *target, const char *suffix, uint64_t pid);
static void setup_sem_names(void);
static int attach_to_sync_objects(void);
static void random_pause(void);
static char letter_for_writer(int index);
static int spawn_writer(int index);
static int spawn_reader(int index);
static int writer_process(int argc, char *argv[]);
static int reader_process(int argc, char *argv[]);

int mvar_main(int argc, char *argv[]) {
    if (argc != 2) {
        print_err("Uso: mvar <num_escritores> <num_lectores>\n");
        return ERROR;
    }

    int num_writers = satoi(argv[0]);
    int num_readers = satoi(argv[1]);

    if (num_writers <= 0 || num_readers <= 0) {
        print_err("mvar: los parámetros deben ser enteros positivos.\n");
        print_err("Uso: mvar <num_escritores> <num_lectores>\n");
        return ERROR;
    }

    if (num_writers > (int)LETTER_POOL_SIZE) {
        print_err("mvar: cantidad máxima de escritores soportada es 62.\n");
        return ERROR;
    }

    setup_sem_names();

    for (int i = 0; i < num_writers; i++) {
        if (spawn_writer(i) < 0) {
            print_err("mvar: error creando un escritor.\n");
            return ERROR;
        }
    }

    for (int i = 0; i < num_readers; i++) {
        if (spawn_reader(i) < 0) {
            print_err("mvar: error creando un lector.\n");
            return ERROR;
        }
    }

    printf("mvar: %d escritores y %d lectores lanzados.\n", num_writers, num_readers);
    return OK;
}

static void build_sem_name(char *target, const char *suffix, uint64_t pid) {
    char pid_buf[DECIMAL_BUFFER_SIZE];
    num_to_str(pid, pid_buf, 10);

    int idx = 0;
    const char *prefix = SEM_PREFIX;

    while (prefix[idx] != 0 && idx < MAX_SEM_NAME_LENGTH - 1) {
        target[idx] = prefix[idx];
        idx++;
    }

    for (int i = 0; suffix[i] != 0 && idx < MAX_SEM_NAME_LENGTH - 1; i++) {
        target[idx++] = suffix[i];
    }

    for (int i = 0; pid_buf[i] != 0 && idx < MAX_SEM_NAME_LENGTH - 1; i++) {
        target[idx++] = pid_buf[i];
    }

    target[idx] = 0;
}

static void setup_sem_names(void) {
    uint64_t pid = sys_getpid();
    build_sem_name(sem_empty_name, SEM_EMPTY_SUFFIX, pid);
    build_sem_name(sem_full_name, SEM_FULL_SUFFIX, pid);
}

static int attach_to_sync_objects(void) {
    if (sys_sem_open(sem_empty_name, 1) < 0) {
        return ERROR;
    }
    if (sys_sem_open(sem_full_name, 0) < 0) {
        return ERROR;
    }
    return OK;
}

static void random_pause(void) {
    uint64_t delay = MIN_SLEEP_MS + GetUniform(SLEEP_JITTER_MS);
    sys_sleep(delay);
}

static char letter_for_writer(int index) {
    if (index < (int)LETTER_POOL_SIZE) {
        return LETTER_POOL[index];
    }
    return '?';
}

static int spawn_writer(int index) {
    char idx_buf[DECIMAL_BUFFER_SIZE];
    num_to_str((uint64_t)index, idx_buf, 10);
    char *writer_argv[] = {idx_buf, NULL};

    int pid = (int)sys_create_process(&writer_process, 1, (const char **)writer_argv, "mvar_writer", NULL);
    return pid < 0 ? ERROR : OK;
}

static int spawn_reader(int index) {
    char idx_buf[DECIMAL_BUFFER_SIZE];
    num_to_str((uint64_t)index, idx_buf, 10);
    char *reader_argv[] = {idx_buf, NULL};

    int pid = (int)sys_create_process(&reader_process, 1, (const char **)reader_argv, "mvar_reader", NULL);
    return pid < 0 ? ERROR : OK;
}

static int writer_process(int argc, char *argv[]) {
    if (argc != 1 || argv[0] == NULL) {
        return ERROR;
    }

    if (attach_to_sync_objects() < 0) {
        print_err("writer: no se pudieron abrir los semáforos.\n");
        return ERROR;
    }

    int idx = (int)satoi(argv[0]);
    if (idx < 0 || idx >= (int)LETTER_POOL_SIZE) {
        idx = 0;
    }
    char letter = letter_for_writer(idx);

    while (1) {
        random_pause();
        sys_sem_wait(sem_empty_name);
        /* No mutex used: write directly to the single mvar */
        mvar_value = letter;
        sys_sem_post(sem_full_name);
    }
    return OK;
}

static int reader_process(int argc, char *argv[]) {
    if (argc != 1 || argv[0] == NULL) {
        return ERROR;
    }

    if (attach_to_sync_objects() < 0) {
        print_err("reader: no se pudieron abrir los semáforos.\n");
        return ERROR;
    }

    int idx = (int)satoi(argv[0]);
    if (idx < 0) {
        idx = 0;
    }
    int color = color_fds[idx % COLOR_COUNT];

    while (1) {
        random_pause();
        sys_sem_wait(sem_full_name);

        /* No mutex used: read and clear mvar directly */
        char c = mvar_value;
        mvar_value = 0;

        sys_sem_post(sem_empty_name);

        sys_write(color, &c, 1);
        char space = ' ';
        sys_write(color, &space, 1);
    }

    return OK;
}