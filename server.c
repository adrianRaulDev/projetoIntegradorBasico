#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Função para decodificar URL (transforma %20 em espaço, etc)
void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10); else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10); else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

void serve_static_file(int client_socket, const char *path) {
    char response[BUFFER_SIZE];
    char full_path[256];
    FILE *file;
    struct stat st;

    // Monta o caminho completo
    if (strstr(path, ".html")) {
        snprintf(full_path, sizeof(full_path), "./paginas%s", path);
    } else if (strstr(path, ".css") || strstr(path, ".js") ||
               strstr(path, ".png") || strstr(path, ".jpg") || strstr(path, ".jpeg")) {
        snprintf(full_path, sizeof(full_path), ".%s", path);
    } else {
        snprintf(full_path, sizeof(full_path), ".%s", path);
    }

    // Verifica se o arquivo existe e não é um diretório
    if (stat(full_path, &st) == -1 || S_ISDIR(st.st_mode)) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 19\r\n\r\n"
                 "Arquivo não encontrado");
        write(client_socket, response, strlen(response));
        return;
    }

    // Abre o arquivo
    file = fopen(full_path, "rb");
    if (!file) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 500 Internal Server Error\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 21\r\n\r\n"
                 "Erro ao abrir arquivo");
        write(client_socket, response, strlen(response));
        return;
    }

    // Define o tipo de conteúdo baseado na extensão
    const char *content_type = "application/octet-stream";
    if (strstr(path, ".html")) {
        content_type = "text/html";
    } else if (strstr(path, ".css")) {
        content_type = "text/css";
    } else if (strstr(path, ".js")) {
        content_type = "application/javascript";
    } else if (strstr(path, ".png")) {
        content_type = "image/png";
    } else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) {
        content_type = "image/jpeg";
    }

    // Envia o cabeçalho HTTP
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n\r\n",
             content_type, st.st_size);
    write(client_socket, response, strlen(response));

    // Envia o conteúdo do arquivo
    size_t bytes_read;
    while ((bytes_read = fread(response, 1, sizeof(response), file)) > 0) {
        write(client_socket, response, bytes_read);
    }

    fclose(file);
}

void salvar_pedido(const char *produtos, double valor_frete, const char *data_hora, int id_pedido) {
    double subtotal = 0.0;
    double total = 0.0;

    // Calcula o subtotal a partir dos preços dos produtos
    // Exemplo: parse os preços dos produtos da string "produtos"
    char produtos_copia[1024];
    strcpy(produtos_copia, produtos);
    char *token = strtok(produtos_copia, ",");
    while (token != NULL) {
        double preco = 0.0;
        sscanf(token, "%*[^()](%lf)", &preco); // Extrai o preço entre parênteses
        subtotal += preco;
        token = strtok(NULL, ",");
    }

    total = subtotal + valor_frete;

    // Salva o pedido no formato desejado
    FILE *arquivo = fopen("pedidos.txt", "a");
    if (arquivo != NULL) {
        fprintf(arquivo, "Pedido ID: %d\n", id_pedido);
        fprintf(arquivo, "Produto: %s\n", produtos);
        fprintf(arquivo, "Subtotal: %.2f\n", subtotal);
        fprintf(arquivo, "Valor do Frete: %.2f\n", valor_frete);
        fprintf(arquivo, "Total: %.2f\n", total);
        fprintf(arquivo, "Data e Hora: %s\n", data_hora);
        fprintf(arquivo, "--------------------------\n");
        fclose(arquivo);
    } else {
        perror("Erro ao abrir o arquivo de pedidos");
    }
}

int id_pedido = 1; // ID do pedido, incrementado a cada novo pedido

void inicializar_id_pedido() {
    FILE *arquivo = fopen("pedidos.txt", "r");
    if (arquivo == NULL || fgetc(arquivo) == EOF) { // Verifica se o arquivo está vazio
        id_pedido = 1; // Reinicia o ID
    }
    if (arquivo != NULL) {
        fclose(arquivo);
    }
}

void processar_pedido(const char *body) {
    char produtos[1024];
    char decoded_body[BUFFER_SIZE];
    double valor_frete;
    char data_hora[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // Pega data e hora
    strftime(data_hora, sizeof(data_hora), "%Y-%m-%d %H:%M:%S", t);

    // Decodifica o body (transforma %20 em espaço, etc)
    urldecode(decoded_body, body);

    // Faz o parsing do corpo do pedido
    sscanf(decoded_body, "produtos=%1023[^&]&frete=%lf", produtos, &valor_frete);

    // Salva o pedido
    salvar_pedido(produtos, valor_frete, data_hora, id_pedido);

    id_pedido++; // Incrementa ID
}

void handle_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    char method[16], path[256], protocol[16];
    char response[BUFFER_SIZE];
    char *body;

    // Lê a requisição do cliente
    read(client_socket, buffer, sizeof(buffer) - 1);

    // Faz o parsing da linha de requisição
    sscanf(buffer, "%s %s %s", method, path, protocol);

    // Se for POST para fazer pedido
    if (strcmp(method, "POST") == 0 && strcmp(path, "/fazer-pedido") == 0) {
        body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4; // Pula os caracteres "\r\n\r\n" para acessar o corpo da requisição

            // Chama a função processar_pedido
            processar_pedido(body);

            // Resposta de sucesso
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: 23\r\n\r\n"
                     "Pedido salvo com sucesso");
            write(client_socket, response, strlen(response));
        } else {
            // Caso o corpo esteja ausente
            snprintf(response, sizeof(response),
                     "HTTP/1.1 400 Bad Request\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: 24\r\n\r\n"
                     "Corpo da requisição vazio");
            write(client_socket, response, strlen(response));
        }
    }
    // Se for GET, serve arquivo
    else if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0) {
            strcpy(path, "/index.html");
        }
        serve_static_file(client_socket, path);
    }
    // Outros métodos = erro
    else {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 19\r\n\r\n"
                 "Página não encontrada");
        write(client_socket, response, strlen(response));
    }

    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Erro ao criar o socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erro ao associar o socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) == -1) {
        perror("Erro ao colocar o socket em escuta");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor rodando em http://localhost:%d\n", PORT);

    inicializar_id_pedido(); // Inicializa o ID do pedido lendo o arquivo

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Erro ao aceitar conexão");
            continue;
        }
        handle_request(client_socket);
    }

    close(server_socket);
    return 0;
}
