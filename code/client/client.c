#if(1)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 1024

// 색상 정의
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int client_socket = -1;
volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t server_connected = 0;  // 서버 연결 상태
volatile sig_atomic_t quiz_active = 0;        // 퀴즈 활성 상태 (입력 루프 종료용)
pthread_t receive_thread;  // 서버 메시지 수신 스레드
pthread_t reconnect_thread;  // 서버 재연결 스레드
volatile sig_atomic_t reconnect_running = 0;  // 재연결 스레드 실행 상태

// 재연결 스레드에 전달할 정보
typedef struct {
    char server_ip[16];
    int port;
} ReconnectInfo;

// 전역 변수로 서버 정보 저장 (재연결 스레드에서 사용)
static char g_server_ip[16] = "127.0.0.1";
static int g_port = 8080;

// 함수 선언
void print_menu(void);
static void *receive_server_messages(void *arg);
static void *reconnect_to_server_thread(void *arg);

// SIGINT 핸들러 (정상 종료)
void sig_int_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n프로그램을 종료합니다...\n");
        keep_running = 0;
        if (client_socket != -1) {
            shutdown(client_socket, SHUT_RDWR);  // 소켓을 닫아서 recv가 반환되도록
        }
        // 스레드가 종료될 시간을 주기 위해 잠시 대기
        usleep(100000);  // 0.1초
        if (client_socket != -1) {
            close(client_socket);
        }
        exit(0);
    }
}

int connect_to_server(const char *server_ip, int port, int quiet) {
    struct sockaddr_in server_addr;
    
    // 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        if (!quiet) {
            perror("소켓 생성 실패");
        }
        return -1;
    }
    
    // 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        if (!quiet) {
            perror("잘못된 IP 주소");
        }
        close(client_socket);
        return -1;
    }
    
    // 서버에 연결
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        if (!quiet) {
            perror("서버 연결 실패");
        }
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    server_connected = 1;  // 연결 상태 설정
    if (!quiet) {
        printf("서버에 연결되었습니다: %s:%d\n", server_ip, port);
    }
    return 0;
}

// 서버 재연결 스레드: 주기적으로 서버에 연결 시도
static void *reconnect_to_server_thread(void *arg) {
    ReconnectInfo *info = (ReconnectInfo *)arg;
    char server_ip[16];
    int port;
    
    // 정보 복사
    strncpy(server_ip, info->server_ip, sizeof(server_ip) - 1);
    server_ip[sizeof(server_ip) - 1] = '\0';
    port = info->port;
    free(info);  // 메모리 해제
    
    // 재연결 스레드 시작 시 메시지는 이미 출력되었으므로 여기서는 출력하지 않음
    // (서버 종료 메시지나 연결 끊김 메시지에서 이미 출력됨)
    fflush(stdout);
    
    while (keep_running && !server_connected) {
        // 3초마다 재연결 시도
        sleep(3);
        
        if (server_connected) {
            // 이미 연결되었으면 종료
            break;
        }
        
        // 재연결 시도 (조용하게 시도)
        if (connect_to_server(server_ip, port, 1) == 0) {
            // 연결 성공
            printf(ANSI_COLOR_GREEN "서버에 재연결되었습니다: %s:%d\n" ANSI_COLOR_RESET, server_ip, port);
            
            // 수신 스레드 재시작
            if (pthread_create(&receive_thread, NULL, receive_server_messages, NULL) != 0) {
                perror("수신 스레드 재생성 실패");
                server_connected = 0;
                continue;
            }
            
            // 메뉴 출력
            printf("\n");
            print_menu();
            break;
        } else {
            // 연결 실패 시 메시지 출력 (이전 줄 비우고)
            printf("\r\033[2K");  // 현재 줄 지우기
            printf(ANSI_COLOR_RED "서버 연결 실패: Connection refused\n" ANSI_COLOR_RESET);
            fflush(stdout);
        }
        // 연결 실패 시 계속 시도
    }
    
    reconnect_running = 0;
    return NULL;
}

// 서버 메시지 수신 스레드: 서버로부터 오는 모든 메시지를 지속적으로 수신
static void *receive_server_messages(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    
    while (keep_running && client_socket != -1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            // 서버 종료 메시지 확인
            if (strstr(buffer, "SERVER_SHUTDOWN")) {
                // 화면 정리 후 메시지 출력
                printf("\n");
                printf(ANSI_COLOR_RED "서버가 종료되었습니다.\n" ANSI_COLOR_RESET);
                printf(ANSI_COLOR_RED "서버와 연결 시도 중 ...\n" ANSI_COLOR_RESET);
                printf(ANSI_COLOR_RED "종료하려면 Ctrl+C를 눌러주세요.\n" ANSI_COLOR_RESET);
                server_connected = 0;  // 연결 상태만 변경, 프로그램은 계속 실행
                
                // 소켓 닫기
                if (client_socket != -1) {
                    close(client_socket);
                    client_socket = -1;
                }
                
                // 재연결 스레드가 실행 중이 아니면 시작
                if (!reconnect_running && keep_running) {
                    reconnect_running = 1;
                    ReconnectInfo *info = malloc(sizeof(ReconnectInfo));
                    if (info) {
                        strncpy(info->server_ip, g_server_ip, sizeof(info->server_ip) - 1);
                        info->server_ip[sizeof(info->server_ip) - 1] = '\0';
                        info->port = g_port;
                        if (pthread_create(&reconnect_thread, NULL, reconnect_to_server_thread, info) != 0) {
                            perror("재연결 스레드 생성 실패");
                            free(info);
                            reconnect_running = 0;
                        } else {
                            pthread_detach(reconnect_thread);
                        }
                    }
                }
                break;
            }

            // QUIZ WRONG 메시지: answer: 프롬프트를 지우고 그 위에 메시지 덮어쓰기
            if (strstr(buffer, "QUIZ WRONG")) {
                printf("\r\033[2K");  // 현재 줄 지우기 (answer: 프롬프트 포함)
                printf(ANSI_COLOR_RED "[INFO] %s" ANSI_COLOR_RESET, buffer);
                printf("answer: ");  // 메시지 출력 후 다시 answer: 프롬프트 표시
                fflush(stdout);
            } else if (strstr(buffer, "QUIZ CORRECT")) {
                //퀴즈 정답 메시지: 버퍼를 지우고 메시지 출력 후 대기
                printf("\r\033[2K");  // 현재 줄 지우기
                printf(ANSI_COLOR_BLUE "[INFO] %s" ANSI_COLOR_RESET, buffer);
                fflush(stdout);
                usleep(500000);  // 0.5초 대기
                quiz_active = 0;  // 퀴즈 입력 루프 종료 (먼저 설정하여 입력 루프가 종료되도록)
                // 메뉴 대시보드 출력 (퀴즈 종료 후 자동 복귀)
                printf("\n");
                print_menu();  // 메뉴 대시보드 출력
            } else if (strstr(buffer, "QUIZ RESULT")) {
                // 퀴즈 결과 메시지 (TIMEOVER 또는 CORRECT): 버퍼를 지우고 메시지 출력 후 대기
                printf("\r\033[2K");  // 현재 줄 지우기
                printf(ANSI_COLOR_BLUE "[INFO] %s" ANSI_COLOR_RESET, buffer);
                fflush(stdout);
                usleep(500000);  // 0.5초 대기
                quiz_active = 0;  // 퀴즈 입력 루프 종료 (먼저 설정하여 입력 루프가 종료되도록)
                // 메뉴 대시보드 출력 (퀴즈 종료 후 자동 복귀)
                printf("\n");
                print_menu();  // 메뉴 대시보드 출력
            } else {
                // 일반 메시지: 현재 입력 프롬프트 지우기
                printf("\r\033[2K"); 

                // 서버 메시지 꾸며서 출력
                if (strstr(buffer, "COMPLETE")) {
                    printf(ANSI_COLOR_GREEN "[SERVER] ✔ %s" ANSI_COLOR_RESET, buffer);
                } else if (strstr(buffer, "CDS_SENSOR")) {
                    printf(ANSI_COLOR_YELLOW "[EVENT] 🔔 %s" ANSI_COLOR_RESET, buffer);
                } else {
                    printf(ANSI_COLOR_BLUE "[INFO] %s" ANSI_COLOR_RESET, buffer);
                }

                // 퀴즈 모드일 때는 answer, 아니면 Select 프롬프트 표시
                if (quiz_active) {
                    printf("answer: ");
                } else {
                    printf("Select: ");
                }
                fflush(stdout);
            }
        
        } else {
            // Ctrl+C로 종료 중이 아니면 재연결 시도
            if (keep_running) {
                printf(ANSI_COLOR_RED "\n서버와 연결이 끊어졌습니다.\n" ANSI_COLOR_RESET);
                printf(ANSI_COLOR_RED "서버와 연결 시도 중 ...\n" ANSI_COLOR_RESET);
                printf(ANSI_COLOR_RED "종료하려면 Ctrl+C를 눌러주세요.\n" ANSI_COLOR_RESET);
            }
            server_connected = 0;  // 연결 상태만 변경, 프로그램은 계속 실행
            
            // 소켓 닫기
            if (client_socket != -1) {
                close(client_socket);
                client_socket = -1;
            }
            
            // 재연결 스레드가 실행 중이 아니면 시작 (Ctrl+C로 종료 중이 아닐 때만)
            if (!reconnect_running && keep_running) {
                reconnect_running = 1;
                ReconnectInfo *info = malloc(sizeof(ReconnectInfo));
                if (info) {
                    strncpy(info->server_ip, g_server_ip, sizeof(info->server_ip) - 1);
                    info->server_ip[sizeof(info->server_ip) - 1] = '\0';
                    info->port = g_port;
                    if (pthread_create(&reconnect_thread, NULL, reconnect_to_server_thread, info) != 0) {
                        perror("재연결 스레드 생성 실패");
                        free(info);
                        reconnect_running = 0;
                    } else {
                        pthread_detach(reconnect_thread);
                    }
                }
            }
            break;
        }
    }
    return NULL;
}

int send_command(const char *command) {
    // 서버 연결 상태 확인
    if (!server_connected || client_socket == -1) {
        printf(ANSI_COLOR_RED "서버와 연결이 되어있지 않습니다.\n" ANSI_COLOR_RESET);
        printf(ANSI_COLOR_RED "종료하려면 Ctrl+C를 입력해주세요.\n" ANSI_COLOR_RESET);
        fflush(stdout);
        return -1;
    }
    
    if (send(client_socket, command, strlen(command), 0) < 0) {
        perror("명령 전송 실패");
        server_connected = 0;  // 전송 실패 시 연결 상태 변경
        
        // 소켓 닫기
        if (client_socket != -1) {
            close(client_socket);
            client_socket = -1;
        }
        
        // 재연결 스레드가 실행 중이 아니면 시작 (Ctrl+C로 종료 중이 아닐 때만)
        if (!reconnect_running && keep_running) {
            reconnect_running = 1;
            ReconnectInfo *info = malloc(sizeof(ReconnectInfo));
            if (info) {
                strncpy(info->server_ip, g_server_ip, sizeof(info->server_ip) - 1);
                info->server_ip[sizeof(info->server_ip) - 1] = '\0';
                info->port = g_port;
                if (pthread_create(&reconnect_thread, NULL, reconnect_to_server_thread, info) != 0) {
                    perror("재연결 스레드 생성 실패");
                    free(info);
                    reconnect_running = 0;
                } else {
                    pthread_detach(reconnect_thread);
                }
            }
        }
        return -1;
    }
    // 응답은 receive_server_messages 스레드에서 처리
    return 0;
}

void print_menu() {
    // \033[H\033[J : 화면을 지우고 커서를 맨 위로 (필요 시 주석 해제)
    printf("\033[H\033[J"); 

    printf(ANSI_COLOR_BLUE "======================================\n");
    printf("       DEVICE CONTROL DASHBOARD       \n");
    printf("======================================\n" ANSI_COLOR_RESET);
    printf(" 1. LED ON        |  6. CDS SENSOR ON\n");
    printf(" 2. LED OFF       |  7. CDS SENSOR OFF\n");
    printf(" 3. LED LEVEL ON  |  8. 7SEGMENT DISPLAY\n");
    printf(" 4. BUZZER ON     |  9. 7SEGMENT COUNTDOWN\n");
    printf(" 5. BUZZER OFF    | 10. 7SEGMENT STOP\n");
    printf("--------------------------------------\n");
    printf("11. QUIZ (프로젝트 점수 맞추기)\n");
    printf("--------------------------------------\n");
    printf(" 0. Exit 프로그램 종료\n");
    printf("======================================\n");
    printf("Select: ");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    char server_ip[16] = "127.0.0.1";  // 기본값: localhost
    int port = 8080;
    char input[BUFFER_SIZE];
    int choice;
    
    // 명령행 인자 처리
    if (argc >= 2) {
        strncpy(server_ip, argv[1], 15);
        server_ip[15] = '\0';
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    
    // 전역 변수에 서버 정보 저장 (재연결 스레드에서 사용)
    strncpy(g_server_ip, server_ip, sizeof(g_server_ip) - 1);
    g_server_ip[sizeof(g_server_ip) - 1] = '\0';
    g_port = port;
    
    // 시그널 핸들러 등록
    // SIGINT만 정상 종료 처리
    signal(SIGINT, sig_int_handler);
    
    // 다른 시그널은 무시 (강제 종료 방지, 하지만 종료는 안 함)
    signal(SIGTERM, SIG_IGN);  // SIGTERM 무시
    signal(SIGHUP, SIG_IGN);   // SIGHUP 무시
    signal(SIGQUIT, SIG_IGN);  // SIGQUIT 무시
    signal(SIGUSR1, SIG_IGN);  // SIGUSR1 무시
    signal(SIGUSR2, SIG_IGN);  // SIGUSR2 무시
    
    // 서버에 연결
    if (connect_to_server(server_ip, port, 0) < 0) {
        return 1;
    }
    
    // 서버 메시지 수신 스레드 시작
    if (pthread_create(&receive_thread, NULL, receive_server_messages, NULL) != 0) {
        perror("수신 스레드 생성 실패");
        close(client_socket);
        return 1;
    }
    
    // 메인 루프
    while (keep_running) {
        // 퀴즈가 활성화되어 있지 않을 때만 메뉴 출력 (중복 출력 방지)
        // 퀴즈 종료 시 수신 스레드에서 이미 메뉴를 출력했으므로 여기서는 출력하지 않음
        if (!quiz_active) {
            // 서버 연결 상태 확인: 연결이 끊겼을 때는 메시지 출력
            // 연결이 정상일 때만 메뉴 출력
            if (server_connected && client_socket != -1) {
                print_menu();
            }
        }
        
        // 퀴즈가 활성화되어 있으면 입력을 받지 않음 (퀴즈 입력 루프에서 처리)
        if (quiz_active) {
            // 퀴즈 입력 루프가 실행 중이므로 잠시 대기
            usleep(100000);  // 0.1초 대기
            continue;
        }
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // 입력이 숫자인지 확인 (개행 문자 제외)
        int is_number = 1;
        int input_len = strlen(input);
        if (input_len > 0 && input[input_len - 1] == '\n') {
            input_len--;  // 개행 문자 제외
        }
        
        // 빈 입력인지 확인
        if (input_len == 0) {
            // 빈 입력은 무시하고 다시 입력 받기
            continue;
        }
        
        // 숫자인지 확인 (음수 부호와 숫자만 허용)
        for (int i = 0; i < input_len; i++) {
            if (i == 0 && input[i] == '-') {
                continue;  // 첫 번째 문자가 음수 부호면 허용
            }
            if (input[i] < '0' || input[i] > '9') {
                is_number = 0;
                break;
            }
        }
        
        // 숫자가 아니면 메시지 출력하고 다시 입력 받기
        if (!is_number) {
            printf(ANSI_COLOR_RED "숫자를 입력해주세요.\n" ANSI_COLOR_RESET);
            fflush(stdout);
            continue;
        }
        
        choice = atoi(input);
        
        switch (choice) {
            case 0:
                printf("프로그램을 종료합니다.\n");
                keep_running = 0;
                break;
            case 1:
                send_command("LED_ON\n");
                break;
            case 2:
                send_command("LED_OFF\n");
                break;
            case 3:
                {
                    char brightness[BUFFER_SIZE];
                    int brightness_level;
                    int valid_input = 0;
                    
                    while (!valid_input) {
                        printf("밝기 선택 (1:최저, 2:중간, 3:최대): ");
                        fgets(brightness, sizeof(brightness), stdin);
                        brightness_level = atoi(brightness);
                        
                        if (brightness_level >= 1 && brightness_level <= 3) {
                            valid_input = 1;
                            snprintf(input, sizeof(input), "LED_BRIGHTNESS %d\n", brightness_level);
                            send_command(input);
                        } else {
                            printf(ANSI_COLOR_RED "잘못된 입력입니다. 1, 2, 3 중 하나를 선택해주세요.\n" ANSI_COLOR_RESET);
                        }
                    }
                }
                break;
            case 4:
                send_command("BUZZER_ON\n");
                break;
            case 5:
                send_command("BUZZER_OFF\n");
                break;
            case 6:
                send_command("SENSOR_ON\n");
                break;
            case 7:
                send_command("SENSOR_OFF\n");
                break;
            case 8:
                {
                    char number[BUFFER_SIZE];
                    printf("표시할 숫자 입력 (0-9): ");
                    fgets(number, sizeof(number), stdin);
                    snprintf(input, sizeof(input), "SEGMENT_DISPLAY %s", number);
                    send_command(input);
                }
                break;
            case 9:
                {
                    char number[BUFFER_SIZE];
                    printf("카운트다운 시작 숫자 입력 (0-9): ");
                    fgets(number, sizeof(number), stdin);
                    snprintf(input, sizeof(input), "SEGMENT_COUNTDOWN %s", number);
                    send_command(input);
                }
                break;
            case 10:
                send_command("SEGMENT_STOP\n");
                break;
            case 11:
                {
                    // 퀴즈 시작
                    quiz_active = 1;  // 퀴즈 활성 상태 설정
                    if (send_command("QUIZ_START\n") < 0) {
                        // 서버 연결이 안 되어있으면 퀴즈 시작 불가
                        quiz_active = 0;
                        break;
                    }
                    
                    while (keep_running && server_connected && quiz_active) {
                        // 퀴즈가 끝났는지 먼저 확인
                        if (!quiz_active) {
                            break;
                        }
                        
                        char answer[BUFFER_SIZE];
                        printf("answer: ");
                        fflush(stdout);
                        
                        // fgets는 블로킹되므로, 퀴즈가 끝났는지 주기적으로 확인할 수 없음
                        // 대신 입력을 받은 후 즉시 quiz_active를 확인하여 종료
                        if (!fgets(answer, sizeof(answer), stdin)) {
                            break;
                        }
                        
                        // 퀴즈가 끝났는지 확인 (입력 받은 직후)
                        if (!quiz_active) {
                            // 퀴즈가 끝났으므로 입력을 무시하고 메뉴로 복귀
                            // 메뉴는 이미 수신 스레드에서 출력되었으므로 여기서는 그냥 종료
                            break;
                        }
                        
                        // 빈 줄이면 퀴즈 입력 종료
                        if (answer[0] == '\n') {
                            break;
                        }
                        
                        // 서버 연결 상태 확인
                        if (!server_connected || client_socket == -1) {
                            printf(ANSI_COLOR_RED "서버와의 연결이 끊겼습니다. 퀴즈를 종료합니다.\n" ANSI_COLOR_RESET);
                            break;
                        }
                        
                        snprintf(input, sizeof(input), "QUIZ_ANSWER %s", answer);
                        if (send_command(input) < 0) {
                            // 명령 전송 실패 시 루프 종료
                            break;
                        }
                        
                        // QUIZ RESULT 메시지를 받았는지 다시 확인 (응답 후)
                        if (!quiz_active) {
                            // 퀴즈가 끝났으므로 메뉴로 복귀
                            break;
                        }
                    }
                    // quiz_active는 이미 수신 스레드에서 0으로 설정되었을 수 있음
                    quiz_active = 0;  // 확실히 종료
                    // 퀴즈 종료 후 메뉴로 자동 복귀
                    // 메뉴는 이미 수신 스레드에서 출력되었으므로 여기서는 그냥 종료
                    // 메인 루프로 돌아가면 다음 입력을 받을 수 있음
                }
                break;
            default:
                printf(ANSI_COLOR_RED "잘못된 입력입니다. 숫자를 입력해주세요.\n" ANSI_COLOR_RESET);
                fflush(stdout);
                break;
        }
    }
    
    // 수신 스레드 종료 대기
    if (client_socket != -1) {
        shutdown(client_socket, SHUT_RDWR);  // 소켓을 닫아서 recv가 반환되도록
    }
    pthread_join(receive_thread, NULL);
    
    close(client_socket);
    return 0;
}

#else 
#endif