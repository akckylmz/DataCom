#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

typedef struct {
    char name[50];
    int stock;
} Book;

Book inventory[] = {
    {"HarryP", 10},
    {"CaliK", 5},
    {"Gray", 15}
};
int inventory_size = sizeof(inventory) / sizeof(inventory[0]);

pthread_mutex_t lock;
int connected_clients = 0;

void *client_handler(void *socket_desc) {
    int client_sock = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    int read_size;

    pthread_mutex_lock(&lock);
    connected_clients++;
    pthread_mutex_unlock(&lock);

    while ((read_size = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Client request: %s\n", buffer);

        char response[BUFFER_SIZE] = {0};

        pthread_mutex_lock(&lock);
        if (strcmp(buffer, "What are your books?") == 0) {
            strcpy(response, "Book stock:\n");
            for (int i = 0; i < inventory_size; i++) {
                char line[100];
                snprintf(line, sizeof(line), "%d %s\n", inventory[i].stock, inventory[i].name);
                strcat(response, line);
            }
        } else if (strcmp(buffer, "How many people are in the store?") == 0) {
            snprintf(response, sizeof(response), "There are %d people in the store.", connected_clients);
        } else {
            int found = 0;
            for (int i = 0; i < inventory_size; i++) {
                if (strcmp(buffer, inventory[i].name) == 0) {
                    if (inventory[i].stock > 0) {
                        inventory[i].stock--;
                        snprintf(response, sizeof(response), "Here is your %s book.", inventory[i].name);
                    } else {
                        snprintf(response, sizeof(response), "We do not have %s in stock.", inventory[i].name);
                    }
                    found = 1;
                    break;
                }
            }
            if (!found) {
                snprintf(response, sizeof(response), "Book %s not found in inventory.", buffer);
            }
        }
        pthread_mutex_unlock(&lock);

        send(client_sock, response, strlen(response), 0);
    }

    pthread_mutex_lock(&lock);
    connected_clients--;
    pthread_mutex_unlock(&lock);

    close(client_sock);
    free(socket_desc);
    return NULL;
}

int main() {
    int server_sock, client_sock, *new_sock;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(client);

    pthread_mutex_init(&lock, NULL);

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    listen(server_sock, MAX_CLIENTS);
    printf("Server listening on port %d...\n", PORT);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client, &client_len))) {
        printf("New connection accepted.\n");
        pthread_t client_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;

        if (pthread_create(&client_thread, NULL, client_handler, (void *)new_sock) < 0) {
            perror("Could not create thread");
            exit(EXIT_FAILURE);
        }
        pthread_detach(client_thread);
    }

    if (client_sock < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_destroy(&lock);
    return 0;
}
