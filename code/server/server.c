#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int server_socket = -1;

// ===== 장치 라이브러리용 함수 포인터 타입 정의 =====
typedef int (*led_init_t)(void);
typedef int (*led_on_t)(void);
typedef int (*led_off_t)(void);
typedef int (*led_set_brightness_t)(int);

typedef int (*buzzer_init_t)(void);
typedef int (*buzzer_on_t)(void);
typedef int (*buzzer_off_t)(void);

typedef int (*segment_init_t)(void);
typedef int (*segment_display_t)(int);
typedef int (*segment_countdown_t)(int);

typedef int (*sensor_init_t)(void);
typedef int (*sensor_get_value_t)(int *);

typedef struct DeviceLibs {
    void *led_handle;
    void *buzzer_handle;
    void *seg7_handle;
    void *cds_handle;

    led_init_t            led_init;
    led_on_t              led_on;
    led_off_t             led_off;
    led_set_brightness_t  led_set_brightness;

    buzzer_init_t         buzzer_init;
    buzzer_on_t           buzzer_on;
    buzzer_off_t          buzzer_off;

    segment_init_t        segment_init;
    segment_display_t     segment_display;
    segment_countdown_t   segment_countdown;

    sensor_init_t         sensor_init;
    sensor_get_value_t    sensor_get_value;
} DeviceLibs;

DeviceLibs g_libs = {0};

typedef struct ClientContext {
    int socket_fd;
    struct sockaddr_in addr;
} ClientContext;

// ===== 시그널 핸들러 (나중에 데몬 프로세스로 변환할 때 사용) =====
void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        printf("\n서버 종료 중...\n");
        if (server_socket != -1) {
            close(server_socket);
        }
        // 라이브러리 언로드
        if (g_libs.led_handle)    dlclose(g_libs.led_handle);
        if (g_libs.buzzer_handle) dlclose(g_libs.buzzer_handle);
        if (g_libs.seg7_handle)   dlclose(g_libs.seg7_handle);
        if (g_libs.cds_handle)    dlclose(g_libs.cds_handle);
        exit(0);
    }
}

static void *load_library(const char *path) {
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "라이브러리 로드 실패 (%s): %s\n", path, dlerror());
    }
    return handle;
}

static int load_symbols(DeviceLibs *libs) {
    dlerror(); // 에러 상태 초기화

    // LED
    libs->led_handle = load_library("exec/lib/libwiringLED.so");
    if (libs->led_handle) {
        libs->led_init = (led_init_t)dlsym(libs->led_handle, "led_init");
        libs->led_on   = (led_on_t)dlsym(libs->led_handle, "led_on");
        libs->led_off  = (led_off_t)dlsym(libs->led_handle, "led_off");
        libs->led_set_brightness =
            (led_set_brightness_t)dlsym(libs->led_handle, "led_set_brightness");
    }

    // BUZZER
    libs->buzzer_handle = load_library("exec/lib/libwiringBuzzer.so");
    if (libs->buzzer_handle) {
        libs->buzzer_init = (buzzer_init_t)dlsym(libs->buzzer_handle, "buzzer_init");
        libs->buzzer_on   = (buzzer_on_t)dlsym(libs->buzzer_handle, "buzzer_on");
        libs->buzzer_off  = (buzzer_off_t)dlsym(libs->buzzer_handle, "buzzer_off");
    }

    // 7SEG
    libs->seg7_handle = load_library("exec/lib/libwiring7Seg.so");
    if (libs->seg7_handle) {
        libs->segment_init      = (segment_init_t)dlsym(libs->seg7_handle, "segment_init");
        libs->segment_display   = (segment_display_t)dlsym(libs->seg7_handle, "segment_display");
        libs->segment_countdown = (segment_countdown_t)dlsym(libs->seg7_handle, "segment_countdown");
    }

    // CDS
    libs->cds_handle = load_library("exec/lib/libwiringCDS.so");
    if (libs->cds_handle) {
        libs->sensor_init      = (sensor_init_t)dlsym(libs->cds_handle, "sensor_init");
        libs->sensor_get_value = (sensor_get_value_t)dlsym(libs->cds_handle, "sensor_get_value");
    }

    // 최소한 LED/BUZZER/SEG7 가 준비되었는지 확인 (필요시 조건 조정)
    if (!libs->led_on || !libs->led_off || !libs->buzzer_on || !libs->buzzer_off ||
        !libs->segment_display) {
        fprintf(stderr, "필수 장치 심볼 로딩 실패\n");
        return -1;
    }
    return 0;
}

// 클라이언트 명령을 장치 제어 함수로 매핑
static const char *handle_command(DeviceLibs *libs, const char *cmd) {
    if (!cmd) return "INVALID COMMAND\n";

    if (strncmp(cmd, "LED_ON", 6) == 0) {
        libs->led_on();
        return "LED ON OK\n";
    } else if (strncmp(cmd, "LED_OFF", 7) == 0) {
        libs->led_off();
        return "LED OFF OK\n";
    } else if (strncmp(cmd, "LED_BRIGHTNESS", 14) == 0) {
        int level = atoi(cmd + 15);
        libs->led_set_brightness(level);
        return "LED BRIGHTNESS OK\n";
    } else if (strncmp(cmd, "BUZZER_ON", 9) == 0) {
        libs->buzzer_on();
        return "BUZZER ON OK\n";
    } else if (strncmp(cmd, "BUZZER_OFF", 10) == 0) {
        libs->buzzer_off();
        return "BUZZER OFF OK\n";
    } else if (strncmp(cmd, "SEGMENT_DISPLAY", 15) == 0) {
        int number = atoi(cmd + 16);
        libs->segment_countdown(number);
        return "SEGMENT DISPLAY OK\n";
    } else if (strncmp(cmd, "SEGMENT_STOP", 12) == 0) {
        // 간단히 0 표시 후 정리 (별도 stop 로직은 추후 확장)
        libs->segment_display(0);
        return "SEGMENT STOP OK\n";
    } else if (strncmp(cmd, "SENSOR_ON", 9) == 0) {
        // 간단히 현재 센서 값 한 번 읽고 로그만 출력 (지속 감시는 추후 구현)
        if (libs->sensor_init && libs->sensor_get_value) {
            int value = 0;
            libs->sensor_get_value(&value);
            printf("조도 센서 값: %d\n", value);
        }
        return "SENSOR ON (stub) OK\n";
    } else if (strncmp(cmd, "SENSOR_OFF", 10) == 0) {
        // 아직 별도 동작 없음 (추후 감시 쓰레드 종료 등 구현)
        return "SENSOR OFF (stub) OK\n";
    }

    return "UNKNOWN COMMAND\n";
}

// 클라이언트별 처리 스레드: 클라이언트가 끊을 때까지 반복 수신/응답
static void *client_thread(void *arg)
{
    ClientContext *ctx = (ClientContext *)arg;
    int client_socket = ctx->socket_fd;
    struct sockaddr_in addr = ctx->addr;
    free(ctx);

    char buffer[BUFFER_SIZE];

    printf("클라이언트 연결됨: %s:%d\n",
           inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            // 클라이언트 종료 또는 오류
            break;
        }

        buffer[bytes_received] = '\0';
        printf("수신된 메시지: %s\n", buffer);

        const char *response = handle_command(&g_libs, buffer);
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket);
    printf("클라이언트 연결 종료: %s:%d\n",
           inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    return NULL;
}

int main(int argc, char *argv[]) {
    (void)argc;  // 사용하지 않는 매개변수 경고 제거
    (void)argv;
    int client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 시그널 핸들러 등록
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // 장치 라이브러리 로딩
    if (load_symbols(&g_libs) < 0) {
        fprintf(stderr, "장치 라이브러리 로딩 실패. 서버를 종료합니다.\n");
        return 1;
    }

    // 소켓 생성
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("소켓 생성 실패");
        exit(1);
    }

    // 소켓 옵션 설정 (재사용 가능하도록)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt 실패");
        exit(1);
    }

    // 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("바인딩 실패");
        exit(1);
    }

    // 리스닝
    if (listen(server_socket, 5) < 0) {
        perror("리스닝 실패");
        exit(1);
    }

    printf("서버가 포트 %d에서 대기 중...\n", PORT);

    // 클라이언트 연결 대기 및 처리 (각 클라이언트는 스레드로 처리하며, 클라이언트가 끊을 때까지 유지)
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("연결 수락 실패");
            continue;
        }

        ClientContext *ctx = malloc(sizeof(ClientContext));
        if (!ctx) {
            perror("클라이언트 컨텍스트 할당 실패");
            close(client_socket);
            continue;
        }
        ctx->socket_fd = client_socket;
        ctx->addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, ctx) != 0) {
            perror("클라이언트 스레드 생성 실패");
            close(client_socket);
            free(ctx);
            continue;
        }
        pthread_detach(tid); // 스레드 리소스 자동 회수
    }

    close(server_socket);
    return 0;
}

