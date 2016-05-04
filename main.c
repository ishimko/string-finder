#include <stdio.h>
#include <libgen.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define ARGS_COUNT 3

typedef enum {
    ST_NULL = 0,
    ST_BUSY,
    ST_FREE
} thread_status_t;

typedef struct {
    char *thread_status;
    char *filename;
} thread_params_t;

typedef struct {
    int entries_count;
    ssize_t total_bytes;
} find_result_t;

char *MODULE_NAME;
char *BYTES_SEQUENCE;

int find_bytes(const char *filename, const char *bytes_sequence, find_result_t* find_result);

void print_error(const char *module_name, const char *error_msg, const char *file_name) {
    fprintf(stderr, "%s: %s %s\n", module_name, error_msg, file_name ? file_name : "");
}

void print_result(const char *filename, const find_result_t find_result){
    printf("%lud %s %ld %d\n", pthread_self(), filename, find_result.total_bytes, find_result.entries_count);
}

void *worker(void* args){
    thread_params_t* thread_params = (thread_params_t *)args;
    find_result_t find_result;
    find_bytes(thread_params->filename, BYTES_SEQUENCE, &find_result);
    print_result(thread_params->filename, find_result);
}

int find_bytes(const char *filename, const char *bytes_sequence, find_result_t* find_result) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        print_error(MODULE_NAME, strerror(errno), filename);
        return -1;
    }

    int i = 0;
    int entries_count = 0;
    ssize_t total_bytes = 0;
    size_t sequence_len = strlen(bytes_sequence);
    char c = 0;

    ssize_t bytes_read;
    while ((bytes_read = read(fd, &c, sizeof(char))) > 0) {
        total_bytes++;
        (bytes_sequence[i] == c) ? (i++) : (i = 0);
        if (sequence_len == i) {
            entries_count++;
            i = 0;
        }
    }

    if (bytes_read == -1) {
        print_error(MODULE_NAME, strerror(errno), filename);
        return -1;
    }

    if (close(fd) == -1) {
        print_error(MODULE_NAME, strerror(errno), filename);
        return -1;
    }

    find_result->entries_count = entries_count;
    find_result->total_bytes = total_bytes;

    return 0;
}

int main(int argc, char *argv[]) {
    MODULE_NAME = basename(argv[0]);

    if (argc != ARGS_COUNT) {
        print_error(MODULE_NAME, "Wrong number of parameters", NULL);
        return 1;
    }

    find_result_t find_result;
    int result = find_bytes(argv[1], argv[2], &find_result);
    printf("%d %ld\n", find_result.entries_count, find_result.total_bytes);

}