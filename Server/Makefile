# Variáveis de Configuração
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
# Adicionamos a biblioteca aqui
LIBS = -lsodium
TARGET = vpn_server

# Caminhos
SRC_DIR = src
OBJ_DIR = obj

# Encontra todos os ficheiros .c na pasta src
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Define os ficheiros .o correspondentes dentro da pasta obj
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Regra padrão
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compilação dos ficheiros objeto (.c -> .o)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Cria a pasta 'obj' automaticamente se não existir
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Limpeza
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean