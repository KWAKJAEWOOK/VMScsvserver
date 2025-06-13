// sds_json_types.h

#ifndef SDS_JSON_TYPES_H
#define SDS_JSON_TYPES_H

#include <stdbool.h>
#include <stddef.h>

// WayPoint 구조체 정의
typedef struct {
    double lat;
    double lon;
    double elevation;           // 선택적 필드
    bool has_elevation;       // elevation 존재 여부 플래그
    double time_offset;
    double speed;
    double heading;             // 선택적 필드
    bool has_heading;         // heading 존재 여부 플래그
} SdsJson_WayPoint_t;

// HostObject 및 RemoteObject에 사용될 TrafficObject 구조체 정의
typedef struct {
    char object_type[32];     // 예시: "vehicle"
    char object_id[64];       // 예시: "Host1", "V2_SigL_Target1"
    bool is_driving_intent_shared;
    int  ig_intersection_intent;
    SdsJson_WayPoint_t* way_point_list; // WayPoint 객체의 동적 배열
    int num_way_points;               // WayPoint 배열의 크기
} SdsJson_TrafficObject_t;

// ConflictPos 구조체 정의
typedef struct {
    double lat;
    double lon;
    double elevation;           // 선택적 필드
    bool has_elevation;       // elevation 존재 여부 플래그
} SdsJson_ConflictPosition_t;

// ApproachTrafficInfo 객체 내부의 실제 정보를 담을 구조체
typedef struct {
    int  cvib_dir_code;
    SdsJson_ConflictPosition_t* conflict_pos; // 선택적 객체이므로 포인터로 선언 (NULL 가능)
    double pet;                     // 선택적 필드
    bool has_pet;                 // PET 존재 여부 플래그
    double pet_threshold;
    SdsJson_TrafficObject_t host_object;      // HostObject는 필수 객체로 가정
    SdsJson_TrafficObject_t* remote_object;   // RemoteObject는 선택적 객체이므로 포인터로 선언 (NULL 가능)
} SdsJson_ApproachTrafficInfoData_t;

// 최상위 JSON 객체 구조체 정의
typedef struct {
    int msg_count;
    char timestamp[30]; // "YYYY-MM-DD HH:MM:SS.SSS"
    SdsJson_ApproachTrafficInfoData_t* approach_traffic_info_list; // ApproachTrafficInfoData 객체의 동적 배열
    int num_approach_traffic_info;                               // ApproachTrafficInfoList 배열의 크기
} SdsJson_MainMessage_t;

// 함수 프로토타입 선언
SdsJson_MainMessage_t* sds_json_parse_message(const char* json_string);
void free_sds_json_main_message(SdsJson_MainMessage_t* msg_data);

#endif // SDS_JSON_TYPES_H
