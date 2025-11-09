
// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

// Test de pipes con nombre
#include "usrlib.h"
#include "syscalls.h"

#define PIPE_NAME "test_pipe"

// Proceso que escribe en el pipe
static int writer_process(int argc, char *argv[]) {
    int fds[2];
    
    // Abrir pipe con nombre
    if (sys_open_named_pipe(PIPE_NAME, fds) < 0) {
        printf("Writer: Failed to open pipe\n");
        return -1;
    }
    
    sys_close_fd(fds[0]);
    printf("Writer: Pipe opened, write_fd=%d\n", fds[1]);
    
    // Escribir varios mensajes
    const char *messages[] = {
        "Hello from writer!\n",
        "This is message 2\n",
        "Final message\n"
    };
    
    for (int i = 0; i < 3; i++) {
        printf("Writer: Sending message %d\n", i + 1);
        const char *msg = messages[i];
        int len = strlen(msg);
        if (sys_write(fds[1], msg, len) < 0) {
            printf("Writer: Failed to write\n");
            break;
        }
        sys_sleep(500); // Esperar un poco entre mensajes
    }
    
    printf("Writer: Closing pipe\n");
    sys_close_fd(fds[1]);
    
    return 0;
}

// Proceso que lee del pipe
static int reader_process(int argc, char *argv[]) {
    int fds[2];
    
    // Abrir el mismo pipe con nombre
    if (sys_open_named_pipe(PIPE_NAME, fds) < 0) {
        printf("Reader: Failed to open pipe\n");
        return -1;
    }
    
    sys_close_fd(fds[1]); // Cerrar el extremo de escritura que no vamos a usar
    printf("Reader: Pipe opened, read_fd=%d\n", fds[0]);
    
    // Leer hasta EOF
    char buffer[256];
    int total_read = 0;
    
    while (1) {
        int bytes = sys_read(fds[0], buffer, sizeof(buffer) - 1);
        
        if (bytes <= 0) {
            printf("Reader: Got EOF (%d bytes)\n", bytes);
            break;
        }
        
        buffer[bytes] = '\0';
        printf("Reader: Received %d bytes: %s", bytes, buffer);
        total_read += bytes;
    }
    
    printf("Reader: Total read: %d bytes\n", total_read);
    printf("Reader: Closing pipe\n");
    sys_close_fd(fds[0]);
    
    return 0;
}

int test_pipes(int argc, char *argv[]) {
    printf("\n=== Test de Pipes con Nombre ===\n\n");
    
    // Crear el proceso escritor
    printf("Main: Creating writer process\n");
    int writer_pid = sys_create_process(writer_process, 0, NULL, "writer", NULL);
    if (writer_pid < 0) {
        printf("Main: Failed to create writer\n");
        return -1;
    }
    
    
    // Crear el proceso lector
    printf("Main: Creating reader process\n");
    int reader_pid = sys_create_process(reader_process, 0, NULL, "reader", NULL);
    if (reader_pid < 0) {
        printf("Main: Failed to create reader\n");
        return -1;
    }
    
    // Esperar a que terminen ambos
    printf("Main: Waiting for processes to finish\n");
    sys_wait(writer_pid);
    sys_wait(reader_pid);
    
    printf("\n=== Test completado ===\n");
    
    return 0;
}