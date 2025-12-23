#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define BUFFER_SIZE 1024

int client_socket = -1;
volatile sig_atomic_t keep_running = 1;

// 시그널 핸들러 (SIGINT만 종료)
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n프로그램을 종료합니다...\n");
        keep_running = 0;
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

int send_command(const char *command) {
    char buffer[BUFFER_SIZE];
    
    if (send(client_socket, command, strlen(command), 0) < 0) {
        perror("명령 전송 실패");
        return -1;
    }
    
    // 서버 응답 수신
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("서버 응답: %s", buffer);
    } else if (bytes_received == 0) {
        printf("서버 연결이 끊어졌습니다.\n");
        return -1;
    } else {
        perror("응답 수신 실패");
        return -1;
    }
    
    return 0;
}

void print_menu() {
    printf("\n[ Device Control Menu ]\n");
    printf("1. LED ON\n");
    printf("2. LED OFF\n");
    printf("3. Set Brightness\n");
    printf("4. BUZZER ON (play melody)\n");
    printf("5. BUZZER OFF (stop)\n");
    printf("6. SENSOR ON (감시 시작)\n");
    printf("7. SENSOR OFF (감시 종료)\n");
    printf("8. SEGMENT DISPLAY (숫자 표시 후 카운트다운)\n");
    printf("9. SEGMENT STOP (카운트다운 중단)\n");
    printf("0. Exit\n");
    printf("\nSelect: ");
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
                    printf("밝기 선택 (1:최저, 2:중간, 3:최대): ");
                    fgets(brightness, sizeof(brightness), stdin);
                    snprintf(input, sizeof(input), "LED_BRIGHTNESS %s", brightness);
                    send_command(input);
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
                send_command("SEGMENT_STOP\n");
                break;
            default:
                printf("잘못된 선택입니다.\n");
                break;
        }
    }
    
    close(client_socket);
    return 0;
}

