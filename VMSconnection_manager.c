// VMSconnection_manager.c

#include "VMSconnection_manager.h"
#include "minIni.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // for isspace,isdigit

// Ubuntu 환경 소켓 및 네트워크 관련
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // for close(), usleep()
#include <errno.h>  // for errno, strerror()
#include <sys/time.h> // for struct timeval (select timeout)
#include <pthread.h> // pthread_mutex_lock/unlock 사용

#define MAX_INI_LINE_LENGTH 256
#define CONNECTION_RETRY_DELAY_MS 500 // 0.5s

// Helper function to convert IP string to uint32_t (network byte order)
static int ip_str_to_uint32(const char* ip_str, uint32_t* out_ip_int) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) == 1) {
        *out_ip_int = addr.s_addr;
        return 0;
    }
    return -1;
}

// Helper function to convert uint32_t (network byte order) to IP string
static int uint32_to_ip_str(uint32_t ip_int, char* out_ip_str, size_t str_len) {
    struct in_addr addr;
    addr.s_addr = ip_int;
    if (inet_ntop(AF_INET, &addr, out_ip_str, str_len) != NULL) {
        return 0;
    }
    return -1;
}

// select()를 사용하여 소켓이 여전히 유효한지 확인하는 함수
static int is_socket_alive_with_select(int sockfd) {
    if (sockfd < 0) {
        return 0;
    }
    fd_set read_fds;
    fd_set err_fds;
    struct timeval tv;
    int retval;

    FD_ZERO(&read_fds);
    FD_ZERO(&err_fds);
    FD_SET(sockfd, &read_fds);
    FD_SET(sockfd, &err_fds);

    tv.tv_sec = 0;
    tv.tv_usec = 0; // Non-blocking check

    retval = select(sockfd + 1, &read_fds, NULL, &err_fds, &tv);

    if (retval == -1) {
        perror("select() in is_socket_alive_with_select");
        return 0;
    }

    if (FD_ISSET(sockfd, &err_fds)) {
        int error_code = 0;
        socklen_t error_code_len = sizeof(error_code);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error_code, &error_code_len) == 0) {
            if (error_code != 0) {
                return 0;
            }
        } else {
            perror("getsockopt in is_socket_alive_with_select");
            return 0;
        }
    }

    if (FD_ISSET(sockfd, &read_fds)) {
        char buffer[1];
        ssize_t n = recv(sockfd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);
        if (n == 0) { // Peer closed connection
            return 0;
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 1; // Still alive, just no data
            }
            return 0; // Other recv error
        }
        return 1; // Data peeked, so alive
    }
    return 1; // select timed out (0), no error/read event implies alive for now
}

VMSServers* vms_manager_init(const char* ini_filepath) {
    VMSServers* vms_data = (VMSServers*)calloc(1, sizeof(VMSServers));
    if (!vms_data) {
        perror("Failed to allocate VMSServers");
        return NULL;
    }

    if (pthread_mutex_init(&vms_data->mutex, NULL) != 0) {
        perror("Failed to initialize mutex for VMSServers");
        free(vms_data);
        return NULL;
    }

    char section_name_buffer[MAX_INI_LINE_LENGTH];
    int section_idx = 0;

    printf("[VMSManager] Loading server config from: %s\n", ini_filepath);

    while (ini_getsection(section_idx++, section_name_buffer, sizeof(section_name_buffer), ini_filepath) > 0) {
        int current_group_id = -1;
        // 섹션 이름에서 그룹 ID 파싱 (예: "1번 그룹" -> 1)
        if (sscanf(section_name_buffer, "%d번 그룹", &current_group_id) == 1 ||
            sscanf(section_name_buffer, "%d", &current_group_id) == 1) { // 또는 단순히 숫자 섹션 이름 "[1]"
            // 유효한 그룹 ID 파싱 성공
        } else {
            fprintf(stderr, "[VMSManager] Warning: Could not parse group ID from section name: %s\n", section_name_buffer);
            continue; // 다음 섹션으로
        }

        char start_ip_str[16] = {0};
        char end_ip_str[16] = {0};
        char port_str[10] = {0}; // 포트번호도 문자열로 읽은 후 변환
        int port = 0;

        // 현재 섹션(current_group_id에 해당하는)에서 키 값들 읽기
        ini_gets(section_name_buffer, "시작 IP", "", start_ip_str, sizeof(start_ip_str), ini_filepath);
        ini_gets(section_name_buffer, "끝 IP", "", end_ip_str, sizeof(end_ip_str), ini_filepath);
        ini_gets(section_name_buffer, "PORT", "0", port_str, sizeof(port_str), ini_filepath);
        port = atoi(port_str);

        if (start_ip_str[0] == '\0' || end_ip_str[0] == '\0' || port == 0) {
            fprintf(stderr, "[VMSManager] Error: Missing Start IP, End IP, or Port in section [%s]\n", section_name_buffer);
            continue;
        }

        // 그룹 정보 추가 로직 (realloc 등)
        vms_data->num_groups++;
        VMSServerGroup* new_groups_ptr = (VMSServerGroup*)realloc(vms_data->groups, vms_data->num_groups * sizeof(VMSServerGroup));
        if (!new_groups_ptr) {
            perror("[VMSManager] Failed to realloc groups array");
            vms_manager_cleanup(vms_data);
            return NULL;
        }
        vms_data->groups = new_groups_ptr;
        VMSServerGroup* current_group_ptr = &vms_data->groups[vms_data->num_groups - 1];
        memset(current_group_ptr, 0, sizeof(VMSServerGroup));
        current_group_ptr->group_id = current_group_id;

        // IP 범위 처리 및 서버 정보 채우기
        uint32_t start_ip_int, end_ip_int;
        if (ip_str_to_uint32(start_ip_str, &start_ip_int) == 0 &&
            ip_str_to_uint32(end_ip_str, &end_ip_int) == 0) {
            
            start_ip_int = ntohl(start_ip_int);
            end_ip_int = ntohl(end_ip_int);

            if (end_ip_int < start_ip_int) {
                fprintf(stderr, "[VMSManager] Error in Group %d: End IP is less than Start IP.\n", current_group_id);
            } else {
                current_group_ptr->num_servers = (end_ip_int - start_ip_int) + 1;
                current_group_ptr->servers = (VMSServerInfo*)calloc(current_group_ptr->num_servers, sizeof(VMSServerInfo));
                if (!current_group_ptr->servers) {
                    perror("[VMSManager] Failed to allocate VMSServerInfo array");
                    vms_manager_cleanup(vms_data);
                    return NULL;
                }
                for (int i_s = 0; i_s < current_group_ptr->num_servers; ++i_s) {
                    uint32_t current_ip_host_order = start_ip_int + i_s;
                    uint32_to_ip_str(htonl(current_ip_host_order), current_group_ptr->servers[i_s].ip_address, sizeof(current_group_ptr->servers[i_s].ip_address));
                    current_group_ptr->servers[i_s].port = port;
                    current_group_ptr->servers[i_s].socket_handle = -1;
                    current_group_ptr->servers[i_s].group_id_for_log = current_group_id;
                    // init 시에는 VMSServerInfo의 state 등 다른 필드도 초기화 필요
                    // current_group_ptr->servers[i_s].state = VMS_STATE_DISCONNECTED; // 만약 state 필드가 있다면
                    // current_group_ptr->servers[i_s].last_attempt_time = 0;
                    vms_data->total_servers_configured++;
                }
                printf("[VMSManager] Group %d (%s) configured with %d servers (IPs: %s-%s, Port: %d)\n",
                       current_group_id, section_name_buffer, current_group_ptr->num_servers, start_ip_str, end_ip_str, port);
            }
        } else {
            fprintf(stderr, "[VMSManager] Error parsing IPs for Group %d: StartIP='%s', EndIP='%s'\n",
                    current_group_id, start_ip_str, end_ip_str);
        }
    } // end while (ini_getsection)

    if (vms_data->num_groups == 0) {
        fprintf(stderr, "[VMSManager] No server groups found in %s.\n", ini_filepath);
        // 설정된 서버가 없는 것이 오류가 아니라면 이 부분은 경고로 처리하거나,
        // vms_manager_cleanup 후 NULL 반환 대신 비어있는 vms_data를 반환할 수도 있습니다.
        // 현재는 num_groups가 0이어도 vms_data를 반환합니다.
    }
    
    return vms_data;
}

void vms_manager_manage_connections(VMSServers* vms_servers) {
    if (!vms_servers) { // num_groups == 0 인 경우도 아래에서 처리됨
        fprintf(stderr, "[ManagerThread] VMS Server data is not initialized.\n");
        return;
    }
    if (vms_servers->num_groups == 0 ) {
         fprintf(stderr, "[ManagerThread] No groups to manage.\n");
        // keep_running_manager가 false가 될 때까지 대기하거나, 바로 리턴할 수 있음.
        // 여기서는 주기적으로 체크하며 대기.
        while(keep_running_manager) {
            usleep(500 * 1000); // 0.5초
        }
        return;
    }


    printf("[ManagerThread] Starting VMS connection management loop...\n");

    while (keep_running_manager) {
        for (int i = 0; i < vms_servers->num_groups && keep_running_manager; ++i) {
            VMSServerGroup* group = &vms_servers->groups[i];
            for (int j = 0; j < group->num_servers && keep_running_manager; ++j) {
                VMSServerInfo* server = &group->servers[j];
                int local_socket_handle;
                int should_attempt_connection = 0;

                // --- CRITICAL SECTION START (READ) ---
                pthread_mutex_lock(&vms_servers->mutex);
                local_socket_handle = server->socket_handle;
                pthread_mutex_unlock(&vms_servers->mutex);
                // --- CRITICAL SECTION END (READ) ---

                if (local_socket_handle == -1) {
                    should_attempt_connection = 1;
                } else {
                    if (!is_socket_alive_with_select(local_socket_handle)) {
                        fprintf(stderr, "[ManagerThread] 연결 유실/오류 감지: 그룹 %d, %s:%d (핸들: %d). 이전 소켓 닫음.\n",
                               server->group_id_for_log, server->ip_address, server->port, local_socket_handle);
                        close(local_socket_handle); // 이전 핸들 닫기

                        // --- CRITICAL SECTION START (WRITE) ---
                        pthread_mutex_lock(&vms_servers->mutex);
                        // 핸들을 닫은 후, 현재 저장된 핸들이 우리가 닫은 핸들과 같은지 확인 (매우 짧은 시간 동안 다른 스레드가 변경했을 가능성 방지)
                        if (server->socket_handle == local_socket_handle) {
                           server->socket_handle = -1;   // 핸들 무효화
                        }
                        pthread_mutex_unlock(&vms_servers->mutex);
                        // --- CRITICAL SECTION END (WRITE) ---
                        should_attempt_connection = 1; // 재연결 필요
                    } else {
                        // printf("[ManagerThread] 연결 활성 유지 중: 그룹 %d, %s:%d (핸들: %d)\n",
                        //       server->group_id_for_log, server->ip_address, server->port, local_socket_handle);
                    }
                }
                
                if (should_attempt_connection && keep_running_manager) {
                    printf("[ManagerThread] 연결 시도: %d번 그룹, %s:%d\n",
                           server->group_id_for_log, server->ip_address, server->port);

                    int new_sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (new_sock < 0) {
                        fprintf(stderr, "[ManagerThread] 소켓 생성 실패 (그룹 %d, %s:%d): %s\n",
                                server->group_id_for_log, server->ip_address, server->port, strerror(errno));
                        continue; 
                    }

                    struct sockaddr_in serv_addr;
                    memset(&serv_addr, 0, sizeof(serv_addr));
                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_port = htons(server->port);

                    if (inet_pton(AF_INET, server->ip_address, &serv_addr.sin_addr) <= 0) {
                        fprintf(stderr, "[ManagerThread] 잘못된 주소 (그룹 %d): %s\n",
                                server->group_id_for_log, server->ip_address);
                        close(new_sock);
                        continue;
                    }
                    
                    // 블로킹 소켓이라 체크 시간이 오래 걸릴 수 있음
                    if (connect(new_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                        fprintf(stderr, "[ManagerThread] 연결 실패: %d번 그룹, %s:%d (에러: %s)\n",
                            server->group_id_for_log, server->ip_address, server->port, strerror(errno));
                        // server->socket_handle은 이미 -1이거나, 이전 체크에서 -1로 설정되었어야 함.
                        close(new_sock);
                    } else {
                        printf("[ManagerThread] 연결 성공: %d번 그룹, %s:%d (새 핸들: %d)\n",
                               server->group_id_for_log, server->ip_address, server->port, new_sock);
                        // --- CRITICAL SECTION START (WRITE) ---
                        pthread_mutex_lock(&vms_servers->mutex);
                        // 만약 이전 핸들이 아직 남아있다면 닫아준다.
                        if(server->socket_handle != -1 && server->socket_handle != new_sock) {
                            close(server->socket_handle);
                        }
                        server->socket_handle = new_sock;
                        pthread_mutex_unlock(&vms_servers->mutex);
                        // --- CRITICAL SECTION END (WRITE) ---
                    }
                }
                if (!keep_running_manager) break; // 서버 루프 즉시 중단
            } // End server loop
            if (!keep_running_manager) break; // 그룹 루프 즉시 중단
        } // End group loop

        // keep_running_manager 플래그에 더 자주 반응하기 위한 분할된 sleep
        for (int k_sleep = 0; k_sleep < (CONNECTION_RETRY_DELAY_MS / 100) && keep_running_manager; ++k_sleep) {
            usleep(100 * 1000); // 0.1초씩 나누어 잠
        }
    } // End while(keep_running_manager)
    printf("[ManagerThread] Connection management loop finished.\n");
}


void vms_manager_cleanup(VMSServers* vms_servers) {
    if (!vms_servers) return;

    printf("Cleaning up VMS connection manager...\n");

    for (int i = 0; i < vms_servers->num_groups; ++i) {
        VMSServerGroup* group = &vms_servers->groups[i];
        if (group->servers) {
            for (int j = 0; j < group->num_servers; ++j) {
                if (group->servers[j].socket_handle != -1) {
                    printf("Closing socket for Group %d, %s:%d (Handle: %d)\n",
                           group->servers[j].group_id_for_log, group->servers[j].ip_address,
                           group->servers[j].port, group->servers[j].socket_handle);
                    close(group->servers[j].socket_handle);
                    group->servers[j].socket_handle = -1;
                }
            }
            free(group->servers);
            group->servers = NULL;
        }
    }
    if (vms_servers->groups) { // groups가 NULL일 수도 있음 (ini파일이 비었거나 파싱실패 등)
        free(vms_servers->groups);
        vms_servers->groups = NULL;
    }
    
    vms_servers->num_groups = 0;
    vms_servers->total_servers_configured = 0;
    
    pthread_mutex_destroy(&vms_servers->mutex); // 뮤텍스 파괴
    
    free(vms_servers);
    printf("Cleanup complete.\n");
}
