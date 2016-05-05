#include <stdio.h>
#include <libgen.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <alloca.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <syscall.h>

#define ARGS_COUNT 4
#define RECURSIVE


typedef enum {
    ST_NULL,
    ST_BUSY,
    ST_FREE
} thread_status_t;

typedef struct {
    thread_status_t *thread_status;
    char *filename;
} thread_params_t;

typedef struct {
    int entries_count;
    ssize_t total_bytes;
} find_result_t;

char *MODULE_NAME;
char *BYTES_SEQUENCE;
pthread_t *THREADS;
thread_status_t *THREADS_STATUS;

int find_bytes(const char *filename, const char *bytes_sequence, find_result_t *find_result);

int wait_for_thread(int threads_count);

void print_error(const char *module_name, const char *error_msg, const char *file_name) {
    fprintf(stderr, "%s: %s %s\n", module_name, error_msg, file_name ? file_name : "");
}

void print_result(const char *filename, const find_result_t find_result) {
    printf("%ld %s %ld %d\n", syscall(SYS_gettid), filename, find_result.total_bytes, find_result.entries_count);
}

void *worker(void *args) {
    thread_params_t *thread_params = (thread_params_t *) args;
    find_result_t find_result;
    if (find_bytes(thread_params->filename, BYTES_SEQUENCE, &find_result) != -1){
        print_result(thread_params->filename, find_result);
    };
    *(thread_params->thread_status) = ST_FREE;
    while(*(thread_params->thread_status) != ST_NULL);
    free(thread_params->filename);
    free(thread_params);
    return NULL;
}

int find_bytes(const char *filename, const char *bytes_sequence, find_result_t *find_result) {
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

void file_path(char *dest, const char *const path, const char *const name) {
    strcpy(dest, path);
    strcat(dest, "/");
    strcat(dest, name);
}

void search_sequence(const char *path, const int threads_count) {
    DIR *dir_stream;
    struct dirent *dir_entry;
    if (!(dir_stream = opendir(path))) {
        print_error(MODULE_NAME, strerror(errno), path);
        return;
    }

    thread_params_t *thread_params;
    pthread_t thread;

    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE);

    errno = 0;
    while ((dir_entry = readdir(dir_stream)) != NULL) {
        char *entry_name = dir_entry->d_name;

        if (!strcmp(".", entry_name) || !strcmp("..", entry_name))
            continue;

        char *full_path = malloc(strlen(entry_name) + strlen(path) + 2);
        file_path(full_path, path, entry_name);
        struct stat entry_info;
        if (lstat(full_path, &entry_info) == -1) {
            print_error(MODULE_NAME, strerror(errno), full_path);
            errno = 0;
            continue;
        }

#ifdef RECURSIVE
        if (S_ISDIR(entry_info.st_mode)) {
            search_sequence(full_path, threads_count);
        } else {
#endif
        if (S_ISREG(entry_info.st_mode)) {
            int thread_id = wait_for_thread(threads_count);
            THREADS_STATUS[thread_id] = ST_NULL;
            pthread_join(THREADS[thread_id], NULL);
            thread_params = malloc(sizeof(thread_params_t));
            thread_params->thread_status = &(THREADS_STATUS[thread_id]);
            thread_params->filename = full_path;
            memset(&thread, 0, sizeof(pthread_t));
            THREADS_STATUS[thread_id] = ST_BUSY;
            if (pthread_create(&thread, &pthread_attr, &worker, thread_params) == -1) {
                print_error(MODULE_NAME, strerror(errno), NULL);
                return;
            };
            THREADS[thread_id] = thread;
        }
#ifdef RECURSIVE
        }
#endif
    }
    if (errno) {
        print_error(MODULE_NAME, strerror(errno), path);
    }

    if (closedir(dir_stream) == -1) {
        print_error(MODULE_NAME, strerror(errno), path);
    }

}

int wait_for_thread(int threads_count) {
    int i = 0;
    while (THREADS_STATUS[i] == ST_BUSY) {
        i++;
        if (i == threads_count) {
            i = 0;
        }
    }
    return i;
}

char all_finished(int threads_count) {
    int i;
    for (i = 0; (i < threads_count); i++){
        if (THREADS_STATUS[i] != ST_FREE){
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int threads_count;
    MODULE_NAME = basename(argv[0]);

    if (argc != ARGS_COUNT) {
        print_error(MODULE_NAME, "Wrong number of parameters", NULL);
        return 1;
    }

    if ((threads_count = atoi(argv[3])) < 1) {
        print_error(MODULE_NAME, "Thread number must be bigger than 1.", NULL);
        return 1;
    }

    THREADS_STATUS = calloc(sizeof(thread_status_t), (size_t) threads_count);
    for (int i = 0; i < threads_count; i++) {
        THREADS_STATUS[i] = ST_NULL;
    }
    BYTES_SEQUENCE = argv[2];
    THREADS = calloc(sizeof(pthread_t), (size_t)threads_count);

    search_sequence(argv[1], threads_count);
    while (!all_finished(threads_count));
//    while (all_finished(threads_count));
//
//
//    find_result_t find_result;
//    int result = find_bytes(argv[1], argv[2], &find_result);
//    printf("%d %ld\n", find_result.entries_count, find_result.total_bytes);

}