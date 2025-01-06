#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define WEB_DIR "./www" // Directory to serve static files

volatile int running = 1;

void handle_sigint(int sig) {
    running = 0;
    printf("\nShutting down server...\n");
}

void handle_client(int client_socket);
void send_response(int client_socket, const char *status, const char *content_type, const char *content);
void send_file(int client_socket, const char *filepath);
const char *get_content_type(const char *path);

int main() {
    signal(SIGINT, handle_sigint);

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d...\n", PORT);

    // Main loop to accept and handle clients
    while (running) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket < 0) {
            if (!running) break;
            perror("Client accept failed");
            continue;
        }

        // Create a thread for each client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, (void *)handle_client, (void *)(intptr_t)client_socket) != 0) {
            perror("Failed to create thread");
            close(client_socket);
        }
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE], method[16], path[256], protocol[16];
    char filepath[512];

    // Read HTTP request from client
    int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        perror("Failed to read request");
        close(client_socket);
        return;
    }

    buffer[bytes_read] = '\0';

    // Parse the request
    sscanf(buffer, "%s %s %s", method, path, protocol);

    // Sanitize path to prevent directory traversal
    if (strstr(path, "..") != NULL) {
        send_response(client_socket, "400 Bad Request", "text/plain", "400 Bad Request");
        close(client_socket);
        return;
    }

    // Only handle GET requests
    if (strcmp(method, "GET") != 0) {
        send_response(client_socket, "405 Method Not Allowed", "text/plain", "405 Method Not Allowed");
        close(client_socket);
        return;
    }

    // Construct file path
    snprintf(filepath, sizeof(filepath), "%s%s", WEB_DIR, path);

    // Default to index.html if root is requested
    if (strcmp(path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", WEB_DIR);
    }

    // Check if the file exists and serve it
    struct stat file_stat;
    if (stat(filepath, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        send_file(client_socket, filepath);
    } else {
        send_response(client_socket, "404 Not Found", "text/plain", "404 Not Found");
    }

    close(client_socket);
}

void send_response(int client_socket, const char *status, const char *content_type, const char *content) {
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             status, content_type, strlen(content));
    write(client_socket, header, strlen(header));
    write(client_socket, content, strlen(content));
}

void send_file(int client_socket, const char *filepath) {
    char header[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    int file_fd = open(filepath, O_RDONLY);

    if (file_fd < 0) {
        perror("File open failed");
        send_response(client_socket, "500 Internal Server Error", "text/plain", "500 Internal Server Error");
        return;
    }

    // Send HTTP response header
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             get_content_type(filepath));
    write(client_socket, header, strlen(header));

    // Send file content
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(client_socket, buffer, bytes_read);
        if (bytes_written < 0) {
            perror("Failed to send file data");
            break;
        }
    }
    if (bytes_read < 0) {
        perror("Failed to read file");
    }

    close(file_fd);
}

const char *get_content_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css")) return "text/css";
    if (strstr(path, ".js")) return "application/javascript";
    if (strstr(path, ".png")) return "image/png";
    if (strstr(path, ".jpg") || strstr(path, ".jpeg")) return "image/jpeg";
    if (strstr(path, ".gif")) return "image/gif";
    return "text/plain"; // Default to plain text for unknown types
}
