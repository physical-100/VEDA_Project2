#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

// 색상 정의
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int client_socket = -1;
volatile sig_atomic_t keep_running = 1;
pthread_t receive_thread;  // 서버 메시지 수신 스레드

// 시그널 핸들러 (SIGINT만 종료)
void signal_handler(int sig) {
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
    // 다른 시그널은 무시
}

int connect_to_server(const char *server_ip, int port) {
    struct sockaddr_in server_addr;
    
    // 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("소켓 생성 실패");
        return -1;
    }
    
    // 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("잘못된 IP 주소");
        close(client_socket);
        return -1;
    }
    
    // 서버에 연결
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("서버 연결 실패");
        close(client_socket);
        return -1;
    }
    
    printf("서버에 연결되었습니다: %s:%d\n", server_ip, port);
    return 0;
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

            // 1. 현재 입력 프롬프트 지우기 (\r: 맨앞으로, \033[2K: 줄 지우기)
            printf("\r\033[2K"); 

            // 2. 서버 메시지 꾸며서 출력
            if (strstr(buffer, "COMPLETE")) {
                printf(ANSI_COLOR_GREEN "[SERVER] ✔ %s" ANSI_COLOR_RESET, buffer);
            } else if (strstr(buffer, "CDS_SENSOR")) {
                printf(ANSI_COLOR_YELLOW "[EVENT] 🔔 %s" ANSI_COLOR_RESET, buffer);
            } else {
                printf(ANSI_COLOR_BLUE "[INFO] %s" ANSI_COLOR_RESET, buffer);
            }

            // 3. 다시 입력 프롬프트 표시 (Select: 문자 뒤에 개행을 넣지 않음)
            printf("Select: ");
            fflush(stdout);
        }
        // ... 생략 (에러 처리) ...
    }
    return NULL;
}

int send_command(const char *command) {
    if (send(client_socket, command, strlen(command), 0) < 0) {
        perror("명령 전송 실패");
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
    printf(" 1. LED ON        |  6. SENSOR ON\n");
    printf(" 2. LED OFF       |  7. SENSOR OFF\n");
    printf(" 3. Brightness    |  8. SEGMENT DISPLAY\n");
    printf(" 4. BUZZER ON     |  9. SEGMENT COUNTDOWN\n");
    printf(" 5. BUZZER OFF    | 10. SEGMENT STOP\n");
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
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    
    // 시그널 핸들러 등록 (SIGINT만 처리)
    signal(SIGINT, signal_handler);
    signal(SIGTERM, SIG_IGN);  // SIGTERM 무시
    signal(SIGHUP, SIG_IGN);   // SIGHUP 무시
    
    // 서버에 연결
    if (connect_to_server(server_ip, port) < 0) {
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
        print_menu();
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
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
            default:
                printf("잘못된 선택입니다.\n");
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

