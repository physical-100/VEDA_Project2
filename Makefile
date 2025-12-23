# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC
LDFLAGS = -lpthread -ldl

# 디렉토리 설정
SRC_CLIENT_DIR = code/client
SRC_SERVER_DIR = code/server
SRC_DEVICE_DIR = code/device_control
EXEC_DIR = exec
LIB_DIR = exec/lib

# 소스 파일
CLIENT_SRC = \
	$(SRC_CLIENT_DIR)/client.c
SERVER_SRC = \
	$(SRC_SERVER_DIR)/server.c

# 실행 파일
CLIENT_EXEC = $(EXEC_DIR)/client
SERVER_EXEC = $(EXEC_DIR)/server

# 기본 타겟
all: $(CLIENT_EXEC) $(SERVER_EXEC) libs
	@echo "빌드 완료!"

# 장치 라이브러리 빌드 (하위 Makefile 호출)
libs:
	@$(MAKE) -C $(SRC_DEVICE_DIR) libs

# 클라이언트 빌드
$(CLIENT_EXEC): $(CLIENT_SRC)
	@mkdir -p $(EXEC_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "클라이언트 빌드 완료: $@"

# 서버 빌드
$(SERVER_EXEC): $(SERVER_SRC)
	@mkdir -p $(EXEC_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "서버 빌드 완료: $@"

# 정리
clean:
	rm -f $(CLIENT_EXEC) $(SERVER_EXEC)
	rm -f $(LIB_DIR)/*.so
	@$(MAKE) -C $(SRC_DEVICE_DIR) clean
	@echo "정리 완료!"

# 재빌드
rebuild: clean all

# 개별 빌드 타겟
client: $(CLIENT_EXEC)
	@echo "클라이언트만 빌드 완료!"

server: $(SERVER_EXEC)
	@echo "서버만 빌드 완료!"

# 실행 파일/라이브러리 확인
check:
	@echo "=== 실행 파일 ==="
	@ls -lh $(EXEC_DIR)/ 2>/dev/null || echo "실행 파일이 없습니다."
	@echo "=== 장치 라이브러리 ==="
	@ls -lh $(LIB_DIR)/ 2>/dev/null || echo "라이브러리가 없습니다."

.PHONY: all clean rebuild check client server libs

