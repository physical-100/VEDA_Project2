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
#define CDS_CHECK_INTERVAL 100  // CDS 센서 체크 간격 (밀리초)
#define MAX_CLIENTS 32          // 최대 클라이언트 수

int server_socket = -1;
volatile int cds_monitor_running = 0;  // CDS 모니터링 스레드 실행 플래그
volatile int cds_thread_created = 0;   // CDS 모니터링 스레드 생성 여부
pthread_t cds_monitor_thread;         // CDS 모니터링 스레드 ID
pthread_mutex_t cds_monitor_mutex = PTHREAD_MUTEX_INITIALIZER;  // CDS 모니터링 제어 뮤텍스

volatile int segment_countdown_running = 0;  // 7SEG 카운트다운 스레드 실행 플래그
volatile int segment_thread_created = 0;     // 7SEG 카운트다운 스레드 생성 여부
pthread_t segment_countdown_thread;         // 7SEG 카운트다운 스레드 ID
pthread_mutex_t segment_countdown_mutex = PTHREAD_MUTEX_INITIALIZER;  // 7SEG 카운트다운 제어 뮤텍스

// 연결된 클라이언트 목록 관리
typedef struct ClientList {
    int socket_fd;
    struct ClientList *next;
} ClientList;

ClientList *client_list_head = NULL;  // 클라이언트 목록 헤드
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;  // 클라이언트 목록 접근 보호

// ===== 통합 장치 라이브러리용 함수 포인터 타입 정의 =====
typedef int (*device_init_all_t)(void);

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
    void *device_handle;  // 통합 라이브러리 핸들

    device_init_all_t     device_init_all;

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

// 함수 선언 (forward declaration)
static void *cds_monitor_thread_func(void *arg);
static void *segment_countdown_thread_func(void *arg);
static void add_client_to_list(int socket_fd);
static void remove_client_from_list(int socket_fd);
static void broadcast_to_clients(const char *message);

// ===== 시그널 핸들러 (나중에 데몬 프로세스로 변환할 때 사용) =====
void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        printf("\n서버 종료 중...\n");
        
        // CDS 모니터링 스레드 종료
        if (cds_thread_created) {
            cds_monitor_running = 0;
            pthread_join(cds_monitor_thread, NULL);
        }
        
        // 7SEG 카운트다운 스레드 종료
        if (segment_thread_created) {
            segment_countdown_running = 0;
            pthread_join(segment_countdown_thread, NULL);
        }
        
        if (server_socket != -1) {
            close(server_socket);
        }
        // 통합 라이브러리 언로드
        if (g_libs.device_handle) {
            dlclose(g_libs.device_handle);
        }
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

    // 통합 장치 라이브러리 로드
    libs->device_handle = load_library("exec/lib/libdevice_manage.so");
    if (!libs->device_handle) {
        fprintf(stderr, "통합 장치 라이브러리 로드 실패\n");
        return -1;
    }

    // 전체 초기화 함수
    libs->device_init_all = (device_init_all_t)dlsym(libs->device_handle, "device_init_all");

    // LED 함수들
    libs->led_init = (led_init_t)dlsym(libs->device_handle, "led_init");
    libs->led_on   = (led_on_t)dlsym(libs->device_handle, "led_on");
    libs->led_off  = (led_off_t)dlsym(libs->device_handle, "led_off");
    libs->led_set_brightness = (led_set_brightness_t)dlsym(libs->device_handle, "led_set_brightness");

    // BUZZER 함수들
    libs->buzzer_init = (buzzer_init_t)dlsym(libs->device_handle, "buzzer_init");
    libs->buzzer_on   = (buzzer_on_t)dlsym(libs->device_handle, "buzzer_on");
    libs->buzzer_off  = (buzzer_off_t)dlsym(libs->device_handle, "buzzer_off");

    // 7SEG 함수들
    libs->segment_init      = (segment_init_t)dlsym(libs->device_handle, "segment_init");
    libs->segment_display   = (segment_display_t)dlsym(libs->device_handle, "segment_display");
    libs->segment_countdown = (segment_countdown_t)dlsym(libs->device_handle, "segment_countdown");

    // CDS 센서 함수들
    libs->sensor_init      = (sensor_init_t)dlsym(libs->device_handle, "sensor_init");
    libs->sensor_get_value = (sensor_get_value_t)dlsym(libs->device_handle, "sensor_get_value");

    // 필수 장치 심볼 로딩 확인
    if (!libs->led_on || !libs->led_off || !libs->buzzer_on || !libs->buzzer_off ||
        !libs->segment_display || !libs->sensor_get_value) {
        fprintf(stderr, "필수 장치 심볼 로딩 실패\n");
        return -1;
    }
    
    // 전체 장치 초기화
    if (libs->device_init_all) {
        libs->device_init_all();
    }
    
    return 0;
}

// 클라이언트 목록에 추가
static void add_client_to_list(int socket_fd) {
    ClientList *new_client = malloc(sizeof(ClientList));
    if (!new_client) {
        perror("클라이언트 목록 노드 할당 실패");
        return;
    }
    
    new_client->socket_fd = socket_fd;
    
    pthread_mutex_lock(&client_list_mutex);
    new_client->next = client_list_head;
    client_list_head = new_client;
    pthread_mutex_unlock(&client_list_mutex);
}

// 클라이언트 목록에서 제거 (내부 함수, 뮤텍스 잠금 전제)
static void remove_client_from_list_locked(int socket_fd) {
    ClientList **curr = &client_list_head;
    while (*curr) {
        if ((*curr)->socket_fd == socket_fd) {
            ClientList *to_remove = *curr;
            *curr = (*curr)->next;
            free(to_remove);
            break;
        }
        curr = &((*curr)->next);
    }
}

// 클라이언트 목록에서 제거 (공개 함수)
static void remove_client_from_list(int socket_fd) {
    pthread_mutex_lock(&client_list_mutex);
    remove_client_from_list_locked(socket_fd);
    pthread_mutex_unlock(&client_list_mutex);
}

// 모든 연결된 클라이언트에 메시지 브로드캐스트
static void broadcast_to_clients(const char *message) {
    if (!message) return;
    
    pthread_mutex_lock(&client_list_mutex);
    
    ClientList *curr = client_list_head;
    ClientList *prev = NULL;
    
    while (curr) {
        // 소켓이 유효한지 확인하고 메시지 전송
        if (send(curr->socket_fd, message, strlen(message), 0) < 0) {
            // 전송 실패 시 목록에서 제거 (연결이 끊어진 것으로 간주)
            ClientList *to_remove = curr;
            if (prev) {
                prev->next = curr->next;
            } else {
                client_list_head = curr->next;
            }
            curr = curr->next;
            free(to_remove);
            continue;
        }
        prev = curr;
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&client_list_mutex);
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
        // 입력한 숫자를 그냥 표시만 함 (즉시 처리)
        int number = atoi(cmd + 16);
        if (number < 0 || number > 9) {
            return "SEGMENT DISPLAY FAILED (범위: 0-9)\n";
        }
        libs->segment_display(number);
        return "SEGMENT DISPLAY OK\n";
    } else if (strncmp(cmd, "SEGMENT_COUNTDOWN", 17) == 0) {
        // 입력한 숫자부터 카운트다운 시작
        int number = atoi(cmd + 18);
        if (number < 0 || number > 9) {
            return "SEGMENT COUNTDOWN FAILED (범위: 0-9)\n";
        }
        
        // 7SEG 카운트다운 스레드 시작
        pthread_mutex_lock(&segment_countdown_mutex);
        if (!segment_thread_created) {
            // 스레드가 아직 생성되지 않았으면 생성
            segment_countdown_running = 1;
            // 스레드에 전달할 숫자를 저장하기 위한 동적 할당
            int *start_number = malloc(sizeof(int));
            if (!start_number) {
                pthread_mutex_unlock(&segment_countdown_mutex);
                return "SEGMENT COUNTDOWN FAILED (메모리 할당 실패)\n";
            }
            *start_number = number;
            
            if (pthread_create(&segment_countdown_thread, NULL, segment_countdown_thread_func, start_number) == 0) {
                segment_thread_created = 1;
                pthread_mutex_unlock(&segment_countdown_mutex);
                return "SEGMENT COUNTDOWN OK\n";
            } else {
                perror("7SEG 카운트다운 스레드 생성 실패");
                segment_countdown_running = 0;
                free(start_number);
                pthread_mutex_unlock(&segment_countdown_mutex);
                return "SEGMENT COUNTDOWN FAILED\n";
            }
        } else {
            // 스레드가 이미 실행 중이면 거부
            pthread_mutex_unlock(&segment_countdown_mutex);
            return "SEGMENT COUNTDOWN ALREADY RUNNING\n";
        }
    } else if (strncmp(cmd, "SEGMENT_STOP", 12) == 0) {
        // 7SEG 카운트다운 스레드 중지
        pthread_mutex_lock(&segment_countdown_mutex);
        if (segment_thread_created && segment_countdown_running) {
            segment_countdown_running = 0;
            pthread_mutex_unlock(&segment_countdown_mutex);
            // 스레드 종료 대기
            pthread_join(segment_countdown_thread, NULL);
            segment_thread_created = 0;
            //libs->segment_display(0);
            return "SEGMENT STOP OK\n";
        } else {
            pthread_mutex_unlock(&segment_countdown_mutex);
            return "SEGMENT NOT RUNNING\n";
        }
    } else if (strncmp(cmd, "SENSOR_ON", 9) == 0) {
        // CDS 센서 모니터링 스레드 시작
        pthread_mutex_lock(&cds_monitor_mutex);
        if (!cds_thread_created) {
            // 스레드가 아직 생성되지 않았으면 생성
            if (libs->sensor_init && libs->sensor_get_value) {
                cds_monitor_running = 1;
                if (pthread_create(&cds_monitor_thread, NULL, cds_monitor_thread_func, NULL) == 0) {
                    cds_thread_created = 1;
                    
                    pthread_mutex_unlock(&cds_monitor_mutex);
                    return "SENSOR ON OK\n";
                } else {
                    perror("CDS 모니터링 스레드 생성 실패");
                    cds_monitor_running = 0;
                    pthread_mutex_unlock(&cds_monitor_mutex);
                    return "SENSOR ON FAILED\n";
                }
            } else {
                pthread_mutex_unlock(&cds_monitor_mutex);
                return "SENSOR LIBRARY NOT AVAILABLE\n";
            }
        } else {
            // 스레드가 이미 생성되어 있으면 실행 플래그만 활성화
            if (!cds_monitor_running) {
                cds_monitor_running = 1;
                printf("CDS 센서 모니터링 재개됨 (상태 초기화)\n");
                // 재시작 시 상태 초기화를 위해 짧은 대기 후 센서 값 다시 읽기
                pthread_mutex_unlock(&cds_monitor_mutex);
                return "SENSOR ON OK\n";
            } else {
                pthread_mutex_unlock(&cds_monitor_mutex);
                return "SENSOR ALREADY ON\n";
            }
        }
    } else if (strncmp(cmd, "SENSOR_OFF", 10) == 0) {
        pthread_mutex_lock(&cds_monitor_mutex);
        if (cds_thread_created) {
            cds_monitor_running = 0; // 플래그를 꺼서 루프 탈출 유도
            pthread_mutex_unlock(&cds_monitor_mutex); // Join 대기를 위해 뮤텍스 해제
            
            pthread_join(cds_monitor_thread, NULL); // 스레드가 완전히 종료될 때까지 대기
            
            pthread_mutex_lock(&cds_monitor_mutex);
            cds_thread_created = 0; // 이제 확실히 새로 생성 가능한 상태
            pthread_mutex_unlock(&cds_monitor_mutex);
            
            printf("CDS 센서 모니터링 완전히 종료됨\n");
            return "SENSOR OFF OK\n";
        }
        pthread_mutex_unlock(&cds_monitor_mutex);
        return "SENSOR ALREADY OFF\n";
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

    // 클라이언트 목록에 추가
    add_client_to_list(client_socket);

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

    // 클라이언트 목록에서 제거
    remove_client_from_list(client_socket);
    
    close(client_socket);
    printf("클라이언트 연결 종료: %s:%d\n",
           inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    return NULL;
}

// CDS 센서 모니터링 스레드: 지속적으로 센서 값을 읽어 LED 자동 제어
static void *cds_monitor_thread_func(void *arg)
{
    (void)arg;  // 사용하지 않는 매개변수 경고 제거
    
    printf("CDS 센서 모니터링 스레드 시작\n");
    
    // 센서 초기화
    if (g_libs.sensor_init && g_libs.sensor_init() < 0) {
        fprintf(stderr, "CDS 센서 초기화 실패\n");
        return NULL;
    }
    
    int last_value = -1;  // 이전 센서 값 (중복 제어 방지)
    int first_read = 1;   // 첫 읽기 플래그 (재시작 시 초기화)
    
    while (cds_monitor_running) {
        if (g_libs.sensor_get_value) {
            int value = 0;
            if (g_libs.sensor_get_value(&value) == 0) {
                // 첫 읽기이거나 센서 값이 변경되었을 때만 LED 제어 및 클라이언트에 알림
                if (first_read || value != last_value) {
                    first_read = 0;  // 첫 읽기 완료
                    char broadcast_msg[BUFFER_SIZE];
                    
                    if (value == 0) {
                        // 빛이 감지됨 (value == 0) → LED OFF
                        if (g_libs.led_off) {
                            g_libs.led_off();
                            printf("[CDS 모니터] 빛 감지됨 → LED OFF\n");
                        }
                        snprintf(broadcast_msg, sizeof(broadcast_msg), "CDS_SENSOR: LIGHT_DETECTED (LED OFF)\n");
                    } else {
                        // 빛이 없음 (value == 1) → LED ON
                        if (g_libs.led_on) {
                            g_libs.led_on();
                            printf("[CDS 모니터] 빛 없음 → LED ON\n");
                        }
                        snprintf(broadcast_msg, sizeof(broadcast_msg), "CDS_SENSOR: NO_LIGHT (LED ON)\n");
                    }
                    
                    // 모든 연결된 클라이언트에 브로드캐스트
                    broadcast_to_clients(broadcast_msg);
                    
                    last_value = value;
                }
            }
        }
        
        // CDS_CHECK_INTERVAL 밀리초 대기
        usleep(CDS_CHECK_INTERVAL * 1000);
    }
    
    // 스레드 종료 시 last_value 초기화 (재시작 시 정상 동작을 위해)
    last_value = -1;
    printf("CDS 센서 모니터링 스레드 종료\n");
    return NULL;
}

// 7SEG 카운트다운 스레드: segment_display를 반복 호출하여 카운트다운 실행
static void *segment_countdown_thread_func(void *arg)
{
    int start_number = *((int *)arg);
    free(arg);  // 메모리 해제
    
    printf("7SEG 카운트다운 스레드 시작: %d부터 시작\n", start_number);
    
    if (start_number < 0) start_number = 0;
    if (start_number > 9) start_number = 9;
    
    // segment_display를 반복 호출하여 카운트다운
    for (int n = start_number; n >= 0 && segment_countdown_running; --n) {
        if (g_libs.segment_display) {
            g_libs.segment_display(n);
        }
        
        // 0이 되었을 때 부저 울림
        if (n == 0) {
            if (g_libs.buzzer_on) {
                g_libs.buzzer_on();
            }
            usleep(500000);  // 0.5초 대기
            if (g_libs.buzzer_off) {
                g_libs.buzzer_off();
            }
            break;  // 0에서 부저 울리고 종료
        }
        
        // 1초 대기 (스레드 중지 확인)
        for (int i = 0; i < 10 && segment_countdown_running; ++i) {
            usleep(100000);  // 0.1초씩 10번 체크 (총 1초)
        }
    }
    
    // 카운트다운 완료 알림
    if (segment_countdown_running) {
        broadcast_to_clients("SEGMENT_COUNTDOWN: COMPLETE\n");
    } else {
        broadcast_to_clients("SEGMENT_COUNTDOWN: STOPPED\n");
    }
    
    // 스레드 종료
    pthread_mutex_lock(&segment_countdown_mutex);
    segment_countdown_running = 0;
    segment_thread_created = 0;
    pthread_mutex_unlock(&segment_countdown_mutex);
    
    printf("7SEG 카운트다운 스레드 종료\n");
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

    // CDS 센서 라이브러리 확인 (스레드는 SENSOR_ON 명령으로 시작)
    if (g_libs.sensor_init && g_libs.sensor_get_value) {
        printf("CDS 센서 라이브러리 로드됨 (SENSOR_ON 명령으로 모니터링 시작 가능)\n");
    } else {
        fprintf(stderr, "경고: CDS 센서 라이브러리가 없어 자동 제어 기능을 사용할 수 없습니다.\n");
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

