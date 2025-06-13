#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// for Window. 리눅스에서 돌리려면 수정 필요
#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9999
#define MAX_FILENAME 256
#define BUFFER_SIZE 1024*30 // 10KB * 3
#define DELAY_MS 100

// 파일을 읽어서 버퍼에 저장
int read_file_to_buffer(const char* filename, char* buffer, size_t buffer_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) return -1;

    size_t read_size = fread(buffer, 1, buffer_size, file);
    fclose(file);
    return (int)read_size;
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char filename[MAX_FILENAME];
    int file_index;

    // Winsock 초기화
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    // 소켓 생성
    printf("Connect Request . . .\n");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // 서버에 연결
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);

    while (1) {
        file_index = 1;
        while (1) {
            snprintf(filename, MAX_FILENAME, "%d.json", file_index);
            int bytes_read = read_file_to_buffer(filename, buffer, BUFFER_SIZE);
            if (bytes_read <= 0) {
                break;
            }

            buffer[bytes_read] = '!';
            int total_bytes = bytes_read + 1;

            printf("First 4 bytes (hex): ");
            for (int i = 0; i < total_bytes; i++) {
                printf("%02X", (unsigned char)buffer[i]);
            }
            printf("\n");

            int sent = send(sock, buffer, total_bytes, 0);
            if (sent == SOCKET_ERROR) {
                printf("Send failed with error: %d\n", WSAGetLastError());
                closesocket(sock);
                WSACleanup();
                return 1;
            }

            printf("Sent %s (%d bytes)\n", filename, bytes_read);
            Sleep(DELAY_MS); // 100ms 대기
            file_index++;
        }
    }

    // 정리
    closesocket(sock);
    WSACleanup();
    return 0;
}
