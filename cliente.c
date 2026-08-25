#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORTA 8080

static int enviar_requisicao(const char *endereco_ip) {
  int cliente = socket(AF_INET, SOCK_STREAM, 0);
  if (cliente < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in endereco = {0};
  endereco.sin_family = AF_INET;
  endereco.sin_port = htons(PORTA);
  if (inet_pton(AF_INET, endereco_ip, &endereco.sin_addr) != 1) {
    fprintf(stderr, "Endereco IP invalido: %s\n", endereco_ip);
    close(cliente);
    return 1;
  }

  if (connect(cliente, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
    perror("connect");
    close(cliente);
    return 1;
  }

  const char *mensagem = "REQUISICAO\n";
  send(cliente, mensagem, strlen(mensagem), 0);

  char resposta[256];
  ssize_t recebidos = recv(cliente, resposta, sizeof(resposta) - 1, 0);
  if (recebidos > 0) {
    resposta[recebidos] = '\0';
    printf("%s", resposta);
  } else {
    fprintf(stderr, "Servidor nao retornou uma resposta.\n");
    close(cliente);
    return 1;
  }
  close(cliente);
  return 0;
}

int main(int argc, char **argv) {
  const char *ip = "127.0.0.1";
  int repeticoes = 1;
  if (argc >= 2)
    ip = argv[1];
  if (argc >= 3)
    repeticoes = atoi(argv[2]);
  if (repeticoes < 1) {
    fprintf(stderr, "Uso: %s [IP_DO_SERVIDOR] [REPETICOES]\n", argv[0]);
    return EXIT_FAILURE;
  }

  for (int i = 1; i <= repeticoes; i++) {
    printf("[%d/%d] ", i, repeticoes);
    if (enviar_requisicao(ip) != 0)
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}