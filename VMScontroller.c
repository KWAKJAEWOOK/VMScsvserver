// VMScontroller.c

#include "VMScontroller.h"
#include "VMSprotocol.h"
#include "minIni.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 헬퍼 함수 프로토타입 (Forward Declarations) 추가
static double calculate_bearing(double lat1, double lon1, double lat2, double lon2);
static double find_closest_bearing(const double* target_bearings, int count, double bearing);
static double get_closest_target_bearing(
    const double* target_bearings, int target_count,
    double lat_c, double lon_c,
    double lat_p, double lon_p
);
static int Change_CVIBDirCode(int cvibDirCode);

bool vms_controller_load_config(const char* config_filepath, VMS_TextParamConfig_t* out_config) {
    if (!config_filepath || !out_config) return false;
    memset(out_config, 0, sizeof(VMS_TextParamConfig_t));

    // 섹션 이름 정의
    const char* server_section = "서버 설정";
    const char* text_section = "텍스트 프로토콜 파라미터";
    const char* coord_section = "기준 좌표";
    const char* dir_section = "방향 코드";
    const char* msg_section = "메시지 템플릿";

    // 서버 설정
    ini_gets(server_section, "ListenIP", "127.0.0.1", out_config->listen_ip, sizeof(out_config->listen_ip), config_filepath);
    out_config->listen_port = (int)ini_getl(server_section, "ListenPort", 9999, config_filepath);

    // 텍스트 파라미터 로드
    ini_gets(text_section, "RST", "1", out_config->rst, sizeof(out_config->rst), config_filepath);
    ini_gets(text_section, "SPD", "3", out_config->spd, sizeof(out_config->spd), config_filepath);
    ini_gets(text_section, "NEN", "0", out_config->nen, sizeof(out_config->nen), config_filepath);
    ini_gets(text_section, "LNE", "1", out_config->lne, sizeof(out_config->lne), config_filepath);
    ini_gets(text_section, "YSZ", "2", out_config->ysz, sizeof(out_config->ysz), config_filepath);
    ini_gets(text_section, "EFF", "090009000900", out_config->eff, sizeof(out_config->eff), config_filepath);
    ini_gets(text_section, "DLY", "3", out_config->dly, sizeof(out_config->dly), config_filepath);
    ini_gets(text_section, "FIX", "1", out_config->fix, sizeof(out_config->fix), config_filepath);
    ini_gets(text_section, "DEFALT_FONT", "$f00", out_config->default_font, sizeof(out_config->default_font), config_filepath);
    ini_gets(text_section, "DEFAULT_COLOR", "$c00", out_config->default_color, sizeof(out_config->default_color), config_filepath);

    // 기준 좌표 로드
    out_config->center_latitude = (double)ini_getf(coord_section, "CenterLatitude", 0.0, config_filepath);
    out_config->center_longitude = (double)ini_getf(coord_section, "CenterLongitude", 0.0, config_filepath);

    // 방향 코드 로드
    out_config->direction_codes[0] = (int)ini_getl(dir_section, "DirCode1", 45, config_filepath);
    out_config->direction_codes[1] = (int)ini_getl(dir_section, "DirCode2", 135, config_filepath);
    out_config->direction_codes[2] = (int)ini_getl(dir_section, "DirCode3", 225, config_filepath);
    out_config->direction_codes[3] = (int)ini_getl(dir_section, "DirCode4", 315, config_filepath);

    // 메시지 템플릿 로드
    ini_gets(msg_section, "Message0", "-", out_config->msg_template0, sizeof(out_config->msg_template0), config_filepath);
    ini_gets(msg_section, "Message1", "차량 접근(Speed:%.1f)", out_config->msg_template1, sizeof(out_config->msg_template1), config_filepath);
    ini_gets(msg_section, "Message2", "차량 진입", out_config->msg_template2, sizeof(out_config->msg_template2), config_filepath);
    ini_gets(msg_section, "Message3", "차량 통과 예상(Speed:%.1f)", out_config->msg_template3, sizeof(out_config->msg_template3), config_filepath);
    ini_gets(msg_section, "Message4", "$c01주의! 충돌 위험! (PET:%.2f)", out_config->msg_template4, sizeof(out_config->msg_template4), config_filepath);

    return true;
}

// 메시지 결정 로직의 기본 틀
VMS_HostObjectState_List_t* vms_controller_process_json_to_state(
    const SdsJson_MainMessage_t* parsed_message,
    const VMS_TextParamConfig_t* text_config
) {
    if (!parsed_message || !text_config) return NULL;

    // 반환할 객체 상태 리스트 초기화
    VMS_HostObjectState_List_t* state_list = (VMS_HostObjectState_List_t*)calloc(1, sizeof(VMS_HostObjectState_List_t));
    if (!state_list) {
        perror("Failed to allocate VMS_HostObjectState_List_t");
        return NULL;
    }
    // 상태 리스트의 객체 배열 동적 할당 (ApproachTrafficInfo 개수만큼 미리 할당)
    state_list->count = parsed_message->num_approach_traffic_info;
    if (state_list->count > 0) {
        state_list->hostobjects = (VMS_HostObjectState_t*)calloc(state_list->count, sizeof(VMS_HostObjectState_t));
        if (!state_list->hostobjects) {
            perror("Failed to allocate VMS_HostObjectState_t array");
            free(state_list);
            return NULL;
        }
    }

    printf("[VMSController] Processing JSON to state objects for MsgCount: %d\n", parsed_message->msg_count);
    
    // config에서 기준 좌표 및 방향 타겟 읽기
    double targets[] = {text_config->direction_codes[0], text_config->direction_codes[1], text_config->direction_codes[2], text_config->direction_codes[3]};
    double lat_c = text_config->center_latitude;
    double lon_c = text_config->center_longitude;

    // ApproachTrafficInfoList 순회하며 각 객체 상태 분석
    for (int i = 0; i < parsed_message->num_approach_traffic_info; ++i) {
        const SdsJson_ApproachTrafficInfoData_t* ati = &parsed_message->approach_traffic_info_list[i];
        VMS_HostObjectState_t* current_state = &state_list->hostobjects[i]; // 채워 넣을 상태 객체

        // 1. 기본 정보 저장
        strncpy(current_state->object_id, ati->host_object.object_id, sizeof(current_state->object_id) - 1);
        current_state->last_msg_count = parsed_message->msg_count;

        // 2. WayPoint 기반 방향 코드 계산
        if (ati->host_object.num_way_points > 0) {
            const SdsJson_WayPoint_t* first_wp = &ati->host_object.way_point_list[0];
            const SdsJson_WayPoint_t* last_wp = &ati->host_object.way_point_list[ati->host_object.num_way_points - 1];
            
            int first_wp_group = (int)get_closest_target_bearing(targets, 4, lat_c, lon_c, first_wp->lat, first_wp->lon);
            int last_wp_group = (int)get_closest_target_bearing(targets, 4, lat_c, lon_c, last_wp->lat, last_wp->lon);
            
            current_state->entry_direction_code = first_wp_group;
            current_state->egress_direction_code = last_wp_group;

            // CVIBDirCode와 GPS 기반 방위각 비교 (로깅)
            int cvib_degree = Change_CVIBDirCode(ati->cvib_dir_code);
            if (cvib_degree != -1 && cvib_degree != first_wp_group) {
                printf("  [Warning] Mismatch for %s: CVIBDir degree(%d) != GPS degree(%d)\n",
                       current_state->object_id, cvib_degree, first_wp_group);
            }
        } else {
            current_state->entry_direction_code = 0; // 정보 없음
            current_state->egress_direction_code = 0;
        }

        // 3. 충돌 정보 분석
        current_state->has_conflict = false;
        current_state->remote_obj_direction_code = 0;

        if (ati->conflict_pos && ati->remote_object && ati->remote_object->num_way_points > 0) {
            current_state->has_conflict = true;
            const SdsJson_WayPoint_t* remote_wp = &ati->remote_object->way_point_list[0];
            current_state->remote_obj_direction_code = (int)get_closest_target_bearing(targets, 4, lat_c, lon_c, remote_wp->lat, remote_wp->lon);
        }
    }

    return state_list;
}

// free 함수
void free_vms_object_state_list(VMS_HostObjectState_List_t* state_list) {
    if (!state_list) return;
    if (state_list->hostobjects) {
        free(state_list->hostobjects);
    }
    free(state_list);
}

void free_vms_message_list(VMS_MessageList_t* message_list) {
    if (!message_list) return;
    if (message_list->messages) {
        for (int i = 0; i < message_list->count; ++i) {
            if (message_list->messages[i].payload_str) {
                free(message_list->messages[i].payload_str);
            }
            if (message_list->messages[i].target_group_ids) {
                free(message_list->messages[i].target_group_ids);
            }
        }
        free(message_list->messages);
    }
    free(message_list);
}


// 첫 번째 웨이포인트 값을 통해 그룹 번호(방위각)를 결정하는 헬퍼 함수
#define DEG2RAD(deg) ((deg) * M_PI / 180.0)
#define RAD2DEG(rad) ((rad) * 180.0 / M_PI)

// 방위각 계산 함수
double calculate_bearing(double lat1, double lon1, double lat2, double lon2) {
    double dLon = DEG2RAD(lon2 - lon1);
    lat1 = DEG2RAD(lat1);
    lat2 = DEG2RAD(lat2);

    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) -
               sin(lat1) * cos(lat2) * cos(dLon);
    double brng = atan2(y, x);
    brng = RAD2DEG(brng);
    brng = fmod((brng + 360.0), 360.0);  // Normalize to 0–360
    // printf("방위각 계산 결과: %f\n", brng);
    return brng;
}

// 가장 가까운 방위각 찾기
double find_closest_bearing(const double* target_bearings, int count, double bearing) {
    double closest = target_bearings[0];
    double min_diff = fabs(bearing - closest);
    if (min_diff > 180) min_diff = 360 - min_diff;

    for (int i = 1; i < count; ++i) {
        double diff = fabs(bearing - target_bearings[i]);
        if (diff > 180) diff = 360 - diff; // circular distance

        if (diff < min_diff) {
            min_diff = diff;
            closest = target_bearings[i];
        }
    }

    return closest;
}

// 통합 헬퍼 함수
double get_closest_target_bearing(
    const double* target_bearings, int target_count,
    double lat_c, double lon_c,
    double lat_p, double lon_p
) {
    double bearing = calculate_bearing(lat_c, lon_c, lat_p, lon_p);
    return find_closest_bearing(target_bearings, target_count, bearing);
}

// CVIBDirCode 방위각 변환 함수
int Change_CVIBDirCode(int cvib_code_id) {
    switch (cvib_code_id) {
        case 10:
            return 0;
        case 50:
            return 45;
        case 20:
            return 90;
        case 60:
            return 135;
        case 30:
            return 180;
        case 70:
            return 225;
        case 40:
            return 270;
        case 80:
            return 315;
        default:
            return -1;
    }
}
