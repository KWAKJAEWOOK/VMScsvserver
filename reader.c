#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/socket.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/select.h>

#include "VMSconnection_manager.h"
#include "VMSprotocol.h"
#include "sds_json_types.h"
#include "VMScontroller.h"
#include "scenario_manager.h"

#define RCV_BUF_SIZE 1024*30 // 수신 버퍼 크기

// 스레드 종료를 제어하기 위한 전역 변수 (또는 VMSData 구조체에 포함 가능)
volatile int keep_running_manager = 1;

// vms_manager_manage_connections를 실행할 스레드 함수
void* connection_manager_thread_func(void* arg) {
    VMSServers* servers = (VMSServers*)arg;
    printf("[Thread] Connection manager thread started.\n");
    vms_manager_manage_connections(servers);
    printf("[Thread] Connection manager thread finishing.\n");
    return NULL;
}

// 특정 그룹의 모든 연결된 서버에게 메시지를 전송하는 함수 (뮤텍스 사용)
void send_message_to_group_thread_safe(VMSServers* all_servers, int target_group_id, const char* message, size_t message_len) {
    if (!all_servers || !message || message_len == 0) {
        fprintf(stderr, "[Sender] 잘못된 인자입니다.\n");
        return;
    }

    // 공유 데이터 접근 전 뮤텍스 잠금
    pthread_mutex_lock(&all_servers->mutex);

    VMSServerGroup* group_to_send = NULL;
    for (int i = 0; i < all_servers->num_groups; ++i) {
        if (all_servers->groups[i].group_id == target_group_id) {
            group_to_send = &all_servers->groups[i];
            break;
        }
    }

    if (!group_to_send) {
        fprintf(stderr, "[Sender] 그룹 ID %d 를 찾을 수 없습니다.\n", target_group_id);
        pthread_mutex_unlock(&all_servers->mutex); // 리턴 전 반드시 잠금 해제
        return;
    }

    printf("[Sender] 그룹 %d (%d개 서버)에 메시지 전송 시도 (뮤텍스 잠금 상태)...\n",
           target_group_id, group_to_send->num_servers);

    for (int i = 0; i < group_to_send->num_servers; ++i) {
        VMSServerInfo* server = &group_to_send->servers[i];
        int current_socket_handle = server->socket_handle; // 핸들 값 복사

        if (current_socket_handle != -1) {
            printf("[Sender]   -> %s:%d (그룹 %d, 핸들: %d) 에 전송 중...\n",
                   server->ip_address, server->port, server->group_id_for_log, current_socket_handle);

            ssize_t total_bytes_sent = 0;
            while ((size_t)total_bytes_sent < message_len) {
                ssize_t bytes_sent_this_call = send(current_socket_handle, // 복사된 핸들 사용
                                                    message + total_bytes_sent,
                                                    message_len - total_bytes_sent,
                                                    MSG_NOSIGNAL);

                if (bytes_sent_this_call < 0) {
                    fprintf(stderr, "[Sender]   ERROR: %s:%d 로 전송 실패 (에러: %s).\n",
                           server->ip_address, server->port, strerror(errno));
                    if(server->socket_handle == current_socket_handle) { // 아직 매니저가 바꾸지 않았다면
                        close(server->socket_handle);
                        server->socket_handle = -1;
                    }
                    break; 
                } else if (bytes_sent_this_call == 0) {
                     fprintf(stderr, "[Sender]   WARNING: %s:%d 로 전송 시 0 바이트 전송됨.\n",
                           server->ip_address, server->port);
                    if(server->socket_handle == current_socket_handle) {
                        close(server->socket_handle);
                        server->socket_handle = -1;
                    }
                    break;
                }
                total_bytes_sent += bytes_sent_this_call;
            }
            if ((size_t)total_bytes_sent == message_len) {
                printf("[Sender]   SUCCESS: %s:%d 로 %ld 바이트 전송 완료.\n",
                       server->ip_address, server->port, total_bytes_sent);
            }
        } else {
            printf("[Sender]   SKIP: %s:%d (그룹 %d)는 연결되지 않음 (핸들: -1).\n",
                   server->ip_address, server->port, server->group_id_for_log);
        }
    }
    // 모든 작업 완료 후 뮤텍스 잠금 해제
    pthread_mutex_unlock(&all_servers->mutex);
}

// 그룹별로 우선순위 높은 메시지 정보를 저장하는 구조체
typedef struct {
    int group_id;
    int message_template_id; // 메시지 템플릿 번호 (1, 2, 3, 4...)
    // 페이로드 생성에 필요한 동적 데이터를 여기에 추가 가능
    double speed;
    double pet;
    int dir_code;
} WinningMessage;

// 구조체 리스트
typedef struct {
    WinningMessage* messages;
    int count;
} WinningMessageList;

// WinningMessageList에 메시지 정보를 업데이트/추가하는 함수
// 동일한 group_id에 대해 더 높은 message_template_id가 들어오면 교체
static void upsert_winning_message(WinningMessageList* list, int group_id, int message_id, const SdsJson_ApproachTrafficInfoData_t* ati) {
    
    // 1. 이미 해당 그룹에 대한 결정이 있는지 확인
    for (int i = 0; i < list->count; ++i) {
        if (list->messages[i].group_id == group_id) {
            // 이미 존재. 메시지 ID가 더 높으면 업데이트
            if (message_id > list->messages[i].message_template_id) {
                list->messages[i].message_template_id = message_id;
                // 페이로드 생성에 필요한 데이터도 함께 업데이트
                if(ati && ati->host_object.num_way_points > 0) {
                     list->messages[i].speed = ati->host_object.way_point_list[0].speed;
                     list->messages[i].dir_code = ati->cvib_dir_code;
                }
                if(ati && ati->has_pet) {
                    list->messages[i].pet = ati->pet;
                }
            }
            return;
        }
    }
    // 2. 해당 그룹에 대한 결정이 없으면 새로 추가
    list->count++;
    WinningMessage* new_array = (WinningMessage*)realloc(list->messages, sizeof(WinningMessage) * list->count);
    if (!new_array) {
        perror("Failed to realloc WinningMessageList");
        list->count--;
        return;
    }
    list->messages = new_array;

    WinningMessage* new_decision = &list->messages[list->count - 1];
    new_decision->group_id = group_id;
    new_decision->message_template_id = message_id;
    // 페이로드 생성에 필요한 데이터 저장
    if(ati && ati->host_object.num_way_points > 0) {
        new_decision->speed = ati->host_object.way_point_list[0].speed;
        new_decision->dir_code = ati->cvib_dir_code;
    }
    if(ati && ati->has_pet) {
        new_decision->pet = ati->pet;
    }
}

// WinningMessageList 메모리 해제 함수
static void free_winning_message_list(WinningMessageList* list) {
    if (!list) return;
    if (list->messages) {
        free(list->messages);
    }
    free(list);
}

// 서버 리스닝 소켓을 설정하고 반환하는 함수
int setup_listening_socket(int port, const char* ip_addr_str) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { 
        perror("Failed to create listening socket");
        return -1; 
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // 설정된 IP 사용, "0.0.0.0" 이거나 비어있으면 INADDR_ANY
    if (ip_addr_str && strlen(ip_addr_str) > 0 && strcmp(ip_addr_str, "0.0.0.0") != 0) {
        if (inet_pton(AF_INET, ip_addr_str, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid ListenIP in config.ini: %s\n", ip_addr_str);
            close(server_sock);
            return -1;
        }
    } else {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind listening socket");
        close(server_sock);
        return -1; 
    }
    if (listen(server_sock, 5) < 0) { 
        perror("Listen failed");
        close(server_sock);
        return -1; 
    }

    printf("[TCPServer] Listening on %s:%d\n", ip_addr_str, port);
    return server_sock;
}

// 수신 버퍼에서 완전한 JSON 메시지(\n 으로 구분)를 추출하는 함수
char* extract_json_message(char* buffer) {
    char* newline = strchr(buffer, '!');
    // printf("\n [디버깅] %s\n%s\n", newline, buffer);
    if (newline == NULL) {
        return NULL; // 아직 완전한 메시지가 도착하지 않음
    }
    printf("\n\n    [디버깅] 개행 문자 발견\n\n");
    *newline = '\0'; // 개행 문자를 널 문자로 대체하여 문자열 분리
    char* json_message = strdup(buffer); // 분리된 JSON 메시지 복사
    // 버퍼의 나머지 부분을 앞으로 당김
    memmove(buffer, newline + 1, strlen(newline + 1) + 1);

    return json_message;
}

int main (int argc, char** argv)
{
    pthread_t conn_manager_tid; // 스레드 ID

    const char* ini_file_name = "vms_servers.ini";
    VMSServers* vms_servers = vms_manager_init(ini_file_name); // 여기서 뮤텍스 초기화됨
    if (vms_servers == NULL) {
        fprintf(stderr, "VMS 매니저 초기화 실패. 프로그램 종료\n");
        return 1;
    }
    printf("VMS 매니저 초기화 성공. 총 설정된 서버 수: %d, 그룹 수: %d\n",
           vms_servers->total_servers_configured, vms_servers->num_groups);

    VMS_TextParamConfig_t config;
    if (!vms_controller_load_config("config.ini", &config)) {
        fprintf(stderr, "Config.ini 로드 실패. 프로그램 종료\n");
        vms_manager_cleanup(vms_servers);
        return 1;
    }

    VMS_ScenarioList_t* scenario_list = load_scenarios_from_csv("scenario2.CSV");
    if (!scenario_list) {
        fprintf(stderr, "Scenario CSV 로드 실패. 프로그램 종료\n");
        vms_manager_cleanup(vms_servers);
        return 1;
    }

    printf("listen IP: %s.%d\n", config.listen_ip, config.listen_port);
    int listen_fd = setup_listening_socket(config.listen_port, config.listen_ip);
    if (listen_fd < 0) {
        free_scenario_list(scenario_list);
        vms_manager_cleanup(vms_servers);
        return 1;
    }

    // 연결 관리자 스레드 생성
    if (pthread_create(&conn_manager_tid, NULL, connection_manager_thread_func, vms_servers) != 0) {
        perror("VMSconnection_manager 스레스 생성 실패. 프로그램 종료\n");
        close(listen_fd);
        free_scenario_list(scenario_list);
        vms_manager_cleanup(vms_servers); // 뮤텍스도 여기서 destroy됨
        return 1;
    }

    sleep(1);   // 연결 대기를 위한 1초

    fd_set all_fds;
    int client_fd = -1;
    char recv_buffer[RCV_BUF_SIZE] = {0};

    // 직전 프레임의 최종 메시지 목록을 저장할 포인터
    WinningMessageList* prev_winning_list = (WinningMessageList*)calloc(1, sizeof(WinningMessageList));

    while (keep_running_manager) {
        FD_ZERO(&all_fds);
        FD_SET(listen_fd, &all_fds);
        int max_fd = listen_fd;

        if (client_fd != -1) {
            FD_SET(client_fd, &all_fds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
        int activity = select(max_fd + 1, &all_fds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("select() error");
            break;
        }
        if (activity == 0) continue;

        if (FD_ISSET(listen_fd, &all_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);

            if (new_socket >= 0) {
                printf("[TCPServer] New connection accepted from %s:%d (fd: %d)\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), new_socket);
                if (client_fd != -1) {
                    printf("[TCPServer] Closing previous connection (fd: %d).\n", client_fd);
                    close(client_fd);
                }
                client_fd = new_socket;
            } else {
                perror("accept failed");
            }
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &all_fds)) {
            char temp_buf[RCV_BUF_SIZE/2];
            ssize_t bytes_read = recv(client_fd, temp_buf, sizeof(temp_buf) - 1, 0);

            if (bytes_read > 0) {
                temp_buf[bytes_read] = '\0';
                if (strlen(recv_buffer) + bytes_read < sizeof(recv_buffer)) {
                    strcat(recv_buffer, temp_buf);
                } else {
                     fprintf(stderr, "Receive buffer overflow! Discarding data.\n");
                     recv_buffer[0] = '\0'; // 버퍼 비움
                }

                char* json_string;
                int innertimer=0;
                while ((json_string = extract_json_message(recv_buffer)) != NULL) {
                    if (innertimer++ >= 10) {
                        printf("[MainLoop] 내부 루프문 오류");
                        break; // or continue
                    }
                    SdsJson_MainMessage_t* parsed_message = sds_json_parse_message(json_string);
                    if (parsed_message) {
                        VMS_HostObjectState_List_t* state_list = vms_controller_process_json_to_state(parsed_message, &config);
                        WinningMessageList* winning_list = (WinningMessageList*)calloc(1, sizeof(WinningMessageList));
                        
                        if (state_list && winning_list) {
                            for (int i = 0; i < state_list->count; ++i) {
                                VMS_HostObjectState_t* obj_state = &state_list->hostobjects[i];
                                const SdsJson_ApproachTrafficInfoData_t* original_ati = &parsed_message->approach_traffic_info_list[i];

                                for (int j = 0; j < scenario_list->count; ++j) {
                                    VMS_ScenarioRule_t* rule = &scenario_list->rules[j];
                                    
                                    int rule_entry_dir = (rule->entry_direction_code > 0) ? config.direction_codes[rule->entry_direction_code - 1] : 0;
                                    int rule_egress_dir = (rule->egress_direction_code > 0) ? config.direction_codes[rule->egress_direction_code - 1] : 0;
                                    int rule_conflict_dir = (rule->conflict_direction_code > 0) ? config.direction_codes[rule->conflict_direction_code - 1] : 0;

                                    bool entry_match = (rule_entry_dir == obj_state->entry_direction_code);
                                    bool egress_match = (rule_egress_dir == obj_state->egress_direction_code);
                                    bool conflict_match = (obj_state->has_conflict ? (rule_conflict_dir == obj_state->remote_obj_direction_code) : (rule_conflict_dir == 0));

                                    if (entry_match && egress_match && conflict_match) {
                                        int groups[4] = { config.direction_codes[0], config.direction_codes[1], config.direction_codes[2], config.direction_codes[3] };
                                        int group_msgs1[4] = { rule->A1, rule->B1, rule->C1, rule->D1 };
                                        int group_msgs2[4] = { rule->A2, rule->B2, rule->C2, rule->D2 };
                                        int group_msgs3[4] = { rule->A3, rule->B3, rule->C3, rule->D3 };
                                        
                                        for (int k = 0; k < 4; ++k) {
                                            if (group_msgs1[k] >= 0) { upsert_winning_message(winning_list, groups[k], group_msgs1[k], original_ati); }
                                            if (group_msgs2[k] >= 0) { upsert_winning_message(winning_list, groups[k] + 1000, group_msgs2[k], original_ati); }
                                            if (group_msgs3[k] >= 0) { upsert_winning_message(winning_list, groups[k] + 2000, group_msgs3[k], original_ati); }
                                        }
                                    }
                                }
                            }
                            printf("\n--- Final Messages to Send (MsgCount: %d) ---\n", parsed_message->msg_count);
                            for (int i = 0; i < winning_list->count; ++i) {
                                WinningMessage* msg = &winning_list->messages[i];
                                bool send_this_message = true;
                                for (int j = 0; j < prev_winning_list->count; ++j) {
                                    WinningMessage* prev_msg = &prev_winning_list->messages[j];
                                    if (msg->group_id == prev_msg->group_id) {
                                        if (msg->message_template_id == prev_msg->message_template_id) {
                                            send_this_message = false;
                                        }
                                        break;
                                    }
                                }
                                if (send_this_message == true) {
                                    char payload_buffer[1024];
                                    char final_text[512];
                                    const char* templates[5] = { config.msg_template0, config.msg_template1, config.msg_template2, config.msg_template3, config.msg_template4 };

                                    if (msg->message_template_id >= 0 && msg->message_template_id < 5) {
                                        const char* template = templates[msg->message_template_id];
                                        if (msg->message_template_id == 1 || msg->message_template_id == 3) {
                                            snprintf(final_text, sizeof(final_text), template, msg->dir_code, msg->speed);
                                        } else if (msg->message_template_id == 4) {
                                            snprintf(final_text, sizeof(final_text), template, msg->pet);
                                        } else {
                                            snprintf(final_text, sizeof(final_text), "%s", template);
                                        }
                                        snprintf(payload_buffer, sizeof(payload_buffer), "RST=%s,SPD=%s,TXT=%s%s%s",
                                                config.rst, config.spd, config.default_font, config.default_color, final_text);
                                        uint16_t packet_len = 0;
                                        uint8_t* packet_data = create_text_control_packet(CMD_TYPE_INSERT, payload_buffer, &packet_len);
                                        if (packet_data && packet_len > 0) {
                                            printf("  ==> Sending to Group %d: %s\n", msg->group_id, payload_buffer);
                                            send_message_to_group_thread_safe(vms_servers, msg->group_id, (const char*)packet_data, packet_len);
                                            free(packet_data);
                                        }
                                    }
                                } else {
                                    printf("  ==> Skip Group (Same msg) %d\n", msg->group_id);
                                }
                            }
                            free_winning_message_list(prev_winning_list);
                            prev_winning_list = winning_list;
                        }
                        if (state_list) free_vms_object_state_list(state_list);
                        free_sds_json_main_message(parsed_message);
                        usleep(10000);
                    }
                    free(json_string);
                }
            } else {
                printf("[TCPServer] Client disconnected (fd: %d).\n", client_fd);
                close(client_fd);
                client_fd = -1;
                recv_buffer[0] = '\0';
            }
        }
    }

    printf("Main loop finished. Shutting down...\n");
    keep_running_manager = 0;
    
    if (client_fd != -1) close(client_fd);
    close(listen_fd);
    
    if (pthread_join(conn_manager_tid, NULL) != 0) { perror("Failed to join connection manager thread"); }
    else { printf("Connection manager thread joined successfully.\n"); }

    free_winning_message_list(prev_winning_list);
    free_scenario_list(scenario_list);
    vms_manager_cleanup(vms_servers);
    printf("All tasks completed. Exiting.\n");
    
    return 0;
}
