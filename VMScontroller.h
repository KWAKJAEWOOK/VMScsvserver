// VMScontroller.h

#ifndef VMS_CONTROLLER_H
#define VMS_CONTROLLER_H

#include "sds_json_types.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_MSG_TEMPLATE_LEN 256

// config.ini에서 읽어올 텍스트 프로토콜 파라미터들을 저장할 구조체
typedef struct {
    char listen_ip[16];
    int listen_port;
    char rst[8];
    char spd[8];
    char nen[8];
    char lne[8];
    char ysz[8];
    char eff[20];
    char dly[8];
    char fix[8];
    char default_font[10];
    char default_color[10];
    double center_latitude;
    double center_longitude;
    int direction_codes[4];
    char msg_template0[MAX_MSG_TEMPLATE_LEN];
    char msg_template1[MAX_MSG_TEMPLATE_LEN];
    char msg_template2[MAX_MSG_TEMPLATE_LEN];
    char msg_template3[MAX_MSG_TEMPLATE_LEN];
    char msg_template4[MAX_MSG_TEMPLATE_LEN];
} VMS_TextParamConfig_t;

// 전송할 페이로드와 대상 그룹 ID 목록을 담을 구조체
typedef struct {
    char* payload_str;       // 실제 전송될 페이로드 문자열 (TXT= 부분 제외하고 조립된 부분)
    int* target_group_ids;   // 이 페이로드를 전송할 그룹 ID들의 배열
    int num_target_groups;   // 대상 그룹의 수
    uint8_t command_type;    // VMS 프로토콜의 command type (예: CMD_TYPE_INSERT)
} VMS_MessageToSend_t;

// VMS_MessageToSend_t의 리스트 (동적 배열)
typedef struct {
    VMS_MessageToSend_t* messages;
    int count; // 리스트 내 메시지 수
} VMS_MessageList_t;


// 객체 ID에 따른 정보
typedef struct {
    char object_id[64];     // 객체 ID
    int last_msg_count;     // 메세지 프레임 카운트
    int entry_direction_code;   // 진입 방향 코드 (degree)
    int egress_direction_code;  // 진출 방향 코드
    bool has_conflict;      // 상충 유무
    int remote_obj_direction_code;  // 상충 예상 위치 (변수명 변경 필요)
} VMS_HostObjectState_t;

// 한 메세지 프레임에서 읽은 객체 리스트
typedef struct {
    VMS_HostObjectState_t* hostobjects;
    int count;
} VMS_HostObjectState_List_t;


/**
 * @brief config.ini 파일에서 텍스트 프로토콜 파라미터를 읽어 구조체에 저장합니다.
 * @param config_filepath config.ini 파일의 경로.
 * @param out_config 성공적으로 읽은 설정값을 저장할 VMS_TextParamConfig_t 구조체 포인터.
 * @return 성공 시 true, 실패 시 false.
 */
bool vms_controller_load_config(const char* config_filepath, VMS_TextParamConfig_t* out_config);

/**
 * @brief 파싱된 SDSM JSON 데이터와 설정을 바탕으로 VMS에 전송할 메시지 목록을 생성합니다.
 * @param parsed_message sds_json_parse_message 함수로부터 반환된 SdsJson_MainMessage_t 포인터.
 * @param text_config vms_controller_load_config 함수로 읽어온 텍스트 프로토콜 설정.
 * @return 생성된 VMS_MessageList_t 포인터 (동적 할당됨, 사용 후 free_vms_message_list 호출 필요).
 * 오류 발생 또는 생성할 메시지가 없으면 count가 0인 리스트나 NULL 반환 가능.
 */
VMS_MessageList_t* vms_controller_determine_messages(
    const SdsJson_MainMessage_t* parsed_message,
    const VMS_TextParamConfig_t* text_config
);

/**
 * @brief vms_controller_determine_messages 함수로 생성된 VMS_MessageList_t를 해제합니다.
 * @param message_list 해제할 VMS_MessageList_t 포인터.
 */
void free_vms_message_list(VMS_MessageList_t* message_list);

VMS_HostObjectState_List_t* vms_controller_process_json_to_state(
    const SdsJson_MainMessage_t* parsed_message,
    const VMS_TextParamConfig_t* text_config
);

void free_vms_object_state_list(VMS_HostObjectState_List_t* state_list);

#endif // VMS_CONTROLLER_H
