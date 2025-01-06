# Linux Web Server

This project implements a simple Linux-based web server in C using UNIX system programming concepts. It serves static files like HTML, CSS, JavaScript, and images.

## Features
- Handles HTTP GET requests.
- Serves static files from a specified directory (`www`).
- Supports concurrent client connections using threads.
- Implements basic error handling and logging.

## Usage

### Compilation
```bash
gcc -o linux_web_server linux_web_server.c -lpthread
