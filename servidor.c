#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define PORTA 8080
#define ARQUIVO_A "A.txt"
#define ARQUIVO_B "B.txt"

typedef struct {
  int caracteres;
  int erro_leitura;
  int erro_escrita;
  pthread_mutex_t *mutex_b;
} DadosRequisicao;

static pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;

static void data_hora_atual(char *destino, size_t tamanho) {
  time_t agora = time(NULL);
  struct tm horario;
  localtime_r(&agora, &horario);
  strftime(destino, tamanho, "%d/%m/%Y %H:%M:%S", &horario);
}

static void *ler_arquivo_a(void *arg) {
  DadosRequisicao *dados = arg;
  int arquivo = open(ARQUIVO_A, O_RDONLY);
  if (arquivo < 0) {
    dados->erro_leitura = 1;
    return NULL;
  }

  char buffer[4096];
  ssize_t lidos;
  dados->caracteres = 0;
  while ((lidos = read(arquivo, buffer, sizeof(buffer))) > 0)
    dados->caracteres += (int)lidos;

  if (lidos < 0)
    dados->erro_leitura = 1;
  close(arquivo);
  return NULL;
}

static void *registrar_em_b(void *arg) {
  DadosRequisicao *dados = arg;
  char quando[32];
  char linha[160];
  data_hora_atual(quando, sizeof(quando));
  snprintf(linha, sizeof(linha),
           "PID: %ld | Data/Hora: %s | Requisicao recebida\n", (long)getpid(),
           quando);

  pthread_mutex_lock(dados->mutex_b);
  FILE *arquivo = fopen(ARQUIVO_B, "a");
  if (arquivo == NULL) {
    dados->erro_escrita = 1;
  } else {
    int falhou = fputs(linha, arquivo) == EOF;
    if (fclose(arquivo) != 0)
      falhou = 1;
    if (falhou)
      dados->erro_escrita = 1;
  }
  pthread_mutex_unlock(dados->mutex_b);
  return NULL;
}

static void atender_cliente(int cliente) {
  char requisicao[128];
  (void)recv(cliente, requisicao, sizeof(requisicao) - 1, 0);

  DadosRequisicao dados = {0, 0, 0, &mutex_b};
  pthread_t thread_leitura;
  pthread_t thread_escrita;

  int leitura_criada =
      pthread_create(&thread_leitura, NULL, ler_arquivo_a, &dados) == 0;
  int escrita_criada =
      pthread_create(&thread_escrita, NULL, registrar_em_b, &dados) == 0;
  if (!leitura_criada || !escrita_criada) {
    const char *erro = "ERRO: nao foi possivel criar as duas threads.\n";
    if (leitura_criada)
      pthread_join(thread_leitura, NULL);
    if (escrita_criada)
      pthread_join(thread_escrita, NULL);
    send(cliente, erro, strlen(erro), 0);
    close(cliente);
    return;
  }

  pthread_join(thread_leitura, NULL);
  pthread_join(thread_escrita, NULL);

  char *resposta;
  if (dados.erro_leitura || dados.erro_escrita) {
    resposta = strdup("ERRO: falha ao ler A.txt ou escrever B.txt.\n");
  } else {
    int tamanho = snprintf(NULL, 0,
                           "OK: requisicao processada. A.txt possui %d "
                           "caracteres.\n",
                           dados.caracteres);
    resposta = malloc((size_t)tamanho + 1);
    if (resposta != NULL)
      snprintf(resposta, (size_t)tamanho + 1,
               "OK: requisicao processada. A.txt possui %d caracteres.\n",
               dados.caracteres);
  }
  if (resposta != NULL) {
    send(cliente, resposta, strlen(resposta), 0);
    free(resposta);
  }
  close(cliente);
}

int main(void) {
  signal(SIGPIPE, SIG_IGN);
  int servidor = socket(AF_INET, SOCK_STREAM, 0);
  if (servidor < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  int reutilizar = 1;
  setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &reutilizar,
             sizeof(reutilizar));

  struct sockaddr_in endereco = {0};
  endereco.sin_family = AF_INET;
  endereco.sin_addr.s_addr = htonl(INADDR_ANY);
  endereco.sin_port = htons(PORTA);

  if (bind(servidor, (struct sockaddr *)&endereco, sizeof(endereco)) < 0 ||
      listen(servidor, 16) < 0) {
    perror("bind/listen");
    close(servidor);
    return EXIT_FAILURE;
  }

  printf("Servidor ouvindo na porta %d...\n", PORTA);
  fflush(stdout);
  while (1) {
    int cliente = accept(servidor, NULL, NULL);
    if (cliente < 0) {
      if (errno == EINTR)
        continue;
      perror("accept");
      break;
    }
    atender_cliente(cliente);
  }
  close(servidor);
  return EXIT_SUCCESS;
}
