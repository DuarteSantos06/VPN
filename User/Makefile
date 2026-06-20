# Variáveis de Configuração
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
# MUDANÇA 1: Adicionamos a flag de linkagem da libsodium
LIBS = -lsodium
TARGET = vpn_client

# Caminhos
SRC_DIR = src
OBJ_DIR = obj

# Encontra todos os ficheiros .c na pasta src
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Define os ficheiros .o correspondentes dentro da pasta obj
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Regra padrão
all: $(TARGET)

# MUDANÇA 2: Adicionamos a variável $(LIBS) no final do comando de linkagem
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compilação dos ficheiros objeto (.c -> .o)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Cria a pasta 'obj' automaticamente
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Limpeza
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean