// VMSconnection_manager.h

#ifndef VMSCONNECTION_MANAGER_H
#define VMSCONNECTION_MANAGER_H

#include <pthread.h>

// 개별 서버 정보
typedef struct {
    char ip_address[16];
    int port;
    int socket_handle;   // TCP 연결 성공 시 소켓 디스크립터, 실패 시 -1
    int group_id_for_log; // 로그 출력을 위한 그룹 ID
} VMSServerInfo;

// 서버 그룹 정보
typedef struct {
    int group_id;          // INI 파일의 그룹 번호 (예: 1번 그룹)
    VMSServerInfo* servers; // 해당 그룹 내 서버 정보 배열 (동적 할당)
    int num_servers;       // 해당 그룹 내 서버 개수
} VMSServerGroup;

// 전체 VMS 서버 관리 구조체
typedef struct {
    VMSServerGroup* groups;   // 서버 그룹 배열 (동적 할당)
    int num_groups;          // 총 그룹 개수
    int total_servers_configured; // INI 파일 통해 설정된 총 서버 개수
    pthread_mutex_t mutex; // 공유 데이터 보호를 위한 뮤텍스
} VMSServers;

// 함수 프로토타입 선언
VMSServers* vms_manager_init(const char* ini_filepath);
void vms_manager_manage_connections(VMSServers* vms_servers);
void vms_manager_cleanup(VMSServers* vms_servers); // 리소스 해제 함수
extern volatile int keep_running_manager; // reader.c 에 정의된 전역 변수 사용

#endif // VMSCONNECTION_MANAGER_H
