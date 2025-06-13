// sds_json_parser.c

#include "sds_json_types.h" // 우리가 정의한 구조체 및 함수 프로토타입
#include "cJSON.h"          // cJSON 라이브러리
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// --- 정적 헬퍼 함수 프로토타입 선언 ---
static bool parse_waypoint(const cJSON* waypoint_json_value, SdsJson_WayPoint_t* waypoint_c);
static bool parse_traffic_object(const cJSON* traffic_obj_json, SdsJson_TrafficObject_t* traffic_obj_c);
static bool parse_conflict_position(const cJSON* conflict_pos_json, SdsJson_ConflictPosition_t* conflict_pos_c);
static bool parse_approach_traffic_info_data(const cJSON* ati_data_json, SdsJson_ApproachTrafficInfoData_t* ati_data_c);

// --- 헬퍼 함수 구현 ---

// WayPoint 객체 하나를 파싱하여 SdsJson_WayPoint_t 구조체에 채움
static bool parse_waypoint(const cJSON* waypoint_json_value, SdsJson_WayPoint_t* waypoint_c) {
    if (!cJSON_IsObject(waypoint_json_value) || !waypoint_c) return false;

    const cJSON* lat = cJSON_GetObjectItemCaseSensitive(waypoint_json_value, "lat");
    const cJSON* lon = cJSON_GetObjectItemCaseSensitive(waypoint_json_value, "lon");
    const cJSON* elevation = cJSON_GetObjectItemCaseSensitive(waypoint_json_value, "elevation");
    const cJSON* time_offset = cJSON_GetObjectItemCaseSensitive(waypoint_json_value, "timeOffset");
    const cJSON* speed = cJSON_GetObjectItemCaseSensitive(waypoint_json_value, "speed");
    const cJSON* heading = cJSON_GetObjectItemCaseSensitive(waypoint_json_value, "heading");

    if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon) || !cJSON_IsNumber(time_offset) || !cJSON_IsNumber(speed)) {
        fprintf(stderr, "Error: WayPoint mandatory fields missing or wrong type.\n");
        return false; // 필수 필드 누락
    }

    waypoint_c->lat = lat->valuedouble;
    waypoint_c->lon = lon->valuedouble;
    waypoint_c->time_offset = time_offset->valuedouble;
    waypoint_c->speed = speed->valuedouble;

    if (cJSON_IsNumber(elevation)) {
        waypoint_c->elevation = elevation->valuedouble;
        waypoint_c->has_elevation = true;
    } else {
        waypoint_c->has_elevation = false;
        waypoint_c->elevation = 0.0; // 기본값 또는 정의되지 않음 표시
    }

    if (cJSON_IsNumber(heading)) {
        waypoint_c->heading = heading->valuedouble;
        waypoint_c->has_heading = true;
    } else {
        waypoint_c->has_heading = false;
        waypoint_c->heading = 0.0; // 기본값
    }
    return true;
}

// TrafficObject (HostObject 또는 RemoteObject)를 파싱하여 SdsJson_TrafficObject_t 구조체에 채움
static bool parse_traffic_object(const cJSON* traffic_obj_json, SdsJson_TrafficObject_t* traffic_obj_c) {
    if (!cJSON_IsObject(traffic_obj_json) || !traffic_obj_c) return false;

    const cJSON* obj_type = cJSON_GetObjectItemCaseSensitive(traffic_obj_json, "ObjectType");
    const cJSON* obj_id = cJSON_GetObjectItemCaseSensitive(traffic_obj_json, "ObjectID");
    const cJSON* is_shared = cJSON_GetObjectItemCaseSensitive(traffic_obj_json, "IsDrivingIntentShared");
    const cJSON* intent = cJSON_GetObjectItemCaseSensitive(traffic_obj_json, "IGIntersectionIntent");
    const cJSON* waypoint_list_json = cJSON_GetObjectItemCaseSensitive(traffic_obj_json, "WayPointList");

    if (!cJSON_IsString(obj_type) || !obj_type->valuestring ||
        !cJSON_IsString(obj_id)   || !obj_id->valuestring   ||
        !cJSON_IsBool(is_shared)  || !cJSON_IsNumber(intent) ||
        !cJSON_IsArray(waypoint_list_json)) {
        fprintf(stderr, "Error: TrafficObject mandatory fields missing or wrong type.\n");
        return false;
    }

    strncpy(traffic_obj_c->object_type, obj_type->valuestring, sizeof(traffic_obj_c->object_type) - 1);
    traffic_obj_c->object_type[sizeof(traffic_obj_c->object_type) - 1] = '\0';
    strncpy(traffic_obj_c->object_id, obj_id->valuestring, sizeof(traffic_obj_c->object_id) - 1);
    traffic_obj_c->object_id[sizeof(traffic_obj_c->object_id) - 1] = '\0';
    traffic_obj_c->is_driving_intent_shared = cJSON_IsTrue(is_shared);
    traffic_obj_c->ig_intersection_intent = intent->valueint;

    traffic_obj_c->num_way_points = cJSON_GetArraySize(waypoint_list_json);
    if (traffic_obj_c->num_way_points > 0) {
        traffic_obj_c->way_point_list = (SdsJson_WayPoint_t*)calloc(traffic_obj_c->num_way_points, sizeof(SdsJson_WayPoint_t));
        if (!traffic_obj_c->way_point_list) {
            perror("Failed to allocate memory for WayPointList");
            traffic_obj_c->num_way_points = 0;
            return false;
        }
        cJSON* waypoint_item_json_container = NULL; // {"WayPoint": {...}}
        int idx = 0;
        cJSON_ArrayForEach(waypoint_item_json_container, waypoint_list_json) {
            const cJSON* waypoint_json_value = cJSON_GetObjectItemCaseSensitive(waypoint_item_json_container, "WayPoint");
            if (!parse_waypoint(waypoint_json_value, &traffic_obj_c->way_point_list[idx++])) {
                // 파싱 실패 시 이미 할당된 way_point_list 해제 필요
                free(traffic_obj_c->way_point_list);
                traffic_obj_c->way_point_list = NULL;
                traffic_obj_c->num_way_points = 0;
                return false;
            }
        }
    } else {
        traffic_obj_c->way_point_list = NULL;
    }
    return true;
}

// ConflictPosition 객체를 파싱하여 SdsJson_ConflictPosition_t 구조체에 채움
static bool parse_conflict_position(const cJSON* conflict_pos_json, SdsJson_ConflictPosition_t* conflict_pos_c) {
    if (!cJSON_IsObject(conflict_pos_json) || !conflict_pos_c) return false;

    const cJSON* lat = cJSON_GetObjectItemCaseSensitive(conflict_pos_json, "lat");
    const cJSON* lon = cJSON_GetObjectItemCaseSensitive(conflict_pos_json, "lon");
    const cJSON* elevation = cJSON_GetObjectItemCaseSensitive(conflict_pos_json, "elevation");

    if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
        fprintf(stderr, "Error: ConflictPosition mandatory fields (lat, lon) missing or wrong type.\n");
        return false;
    }

    conflict_pos_c->lat = lat->valuedouble;
    conflict_pos_c->lon = lon->valuedouble;

    if (cJSON_IsNumber(elevation)) {
        conflict_pos_c->elevation = elevation->valuedouble;
        conflict_pos_c->has_elevation = true;
    } else {
        conflict_pos_c->has_elevation = false;
        conflict_pos_c->elevation = 0.0;
    }
    return true;
}

// ApproachTrafficInfo 데이터 객체를 파싱하여 SdsJson_ApproachTrafficInfoData_t 구조체에 채움
static bool parse_approach_traffic_info_data(const cJSON* ati_data_json, SdsJson_ApproachTrafficInfoData_t* ati_data_c) {
    if (!cJSON_IsObject(ati_data_json) || !ati_data_c) return false;

    const cJSON* cvib_dir_code = cJSON_GetObjectItemCaseSensitive(ati_data_json, "CVIBDirCode");
    const cJSON* conflict_pos_json = cJSON_GetObjectItemCaseSensitive(ati_data_json, "ConflictPos");
    const cJSON* pet_json = cJSON_GetObjectItemCaseSensitive(ati_data_json, "PET");
    const cJSON* pet_threshold = cJSON_GetObjectItemCaseSensitive(ati_data_json, "PET_Threshold");
    const cJSON* host_obj_json = cJSON_GetObjectItemCaseSensitive(ati_data_json, "HostObject");
    const cJSON* remote_obj_json = cJSON_GetObjectItemCaseSensitive(ati_data_json, "RemoteObject");

    if (!cJSON_IsNumber(cvib_dir_code) || !cJSON_IsNumber(pet_threshold) || !cJSON_IsObject(host_obj_json)) {
         fprintf(stderr, "Error: ApproachTrafficInfoData mandatory fields missing or wrong type.\n");
        return false;
    }

    ati_data_c->cvib_dir_code = cvib_dir_code->valueint;
    ati_data_c->pet_threshold = pet_threshold->valuedouble;

    // 선택적 ConflictPos 파싱
    if (cJSON_IsObject(conflict_pos_json)) {
        ati_data_c->conflict_pos = (SdsJson_ConflictPosition_t*)calloc(1, sizeof(SdsJson_ConflictPosition_t));
        if (!ati_data_c->conflict_pos) {
            perror("Failed to allocate memory for ConflictPosition");
            return false;
        }
        if (!parse_conflict_position(conflict_pos_json, ati_data_c->conflict_pos)) {
            free(ati_data_c->conflict_pos);
            ati_data_c->conflict_pos = NULL;
            return false; // ConflictPos 파싱 실패
        }
    } else {
        ati_data_c->conflict_pos = NULL;
    }

    // 선택적 PET 파싱
    if (cJSON_IsNumber(pet_json)) {
        ati_data_c->pet = pet_json->valuedouble;
        ati_data_c->has_pet = true;
    } else {
        ati_data_c->has_pet = false;
        ati_data_c->pet = 0.0;
    }

    // HostObject 파싱 (필수)
    if (!parse_traffic_object(host_obj_json, &ati_data_c->host_object)) {
        // HostObject 파싱 실패 시, 할당된 conflict_pos 해제 필요
        if(ati_data_c->conflict_pos) free(ati_data_c->conflict_pos);
        ati_data_c->conflict_pos = NULL;
        return false;
    }

    // 선택적 RemoteObject 파싱
    if (cJSON_IsObject(remote_obj_json)) {
        ati_data_c->remote_object = (SdsJson_TrafficObject_t*)calloc(1, sizeof(SdsJson_TrafficObject_t));
        if (!ati_data_c->remote_object) {
            perror("Failed to allocate memory for RemoteObject");
            // 이전에 할당된 자원들 해제 필요
            if(ati_data_c->conflict_pos) free(ati_data_c->conflict_pos);
            if(ati_data_c->host_object.way_point_list) free(ati_data_c->host_object.way_point_list);
            return false;
        }
        if (!parse_traffic_object(remote_obj_json, ati_data_c->remote_object)) {
            // RemoteObject 파싱 실패 시, 할당된 자원들 해제 필요
            free(ati_data_c->remote_object); // 방금 할당한 remote_object
            ati_data_c->remote_object = NULL;
            if(ati_data_c->conflict_pos) free(ati_data_c->conflict_pos);
            if(ati_data_c->host_object.way_point_list) free(ati_data_c->host_object.way_point_list);
            return false;
        }
    } else {
        ati_data_c->remote_object = NULL;
    }

    return true;
}


// --- 공개 함수 구현 ---

SdsJson_MainMessage_t* sds_json_parse_message(const char* json_string) {
    if (!json_string) return NULL;

    cJSON *root_json = cJSON_Parse(json_string);
    if (root_json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error parsing JSON before: %s\n", error_ptr);
        } else {
            fprintf(stderr, "Error parsing JSON, but cJSON_GetErrorPtr() returned NULL.\n");
        }
        return NULL;
    }

    SdsJson_MainMessage_t* msg_data = (SdsJson_MainMessage_t*)calloc(1, sizeof(SdsJson_MainMessage_t));
    if (!msg_data) {
        perror("Failed to allocate memory for SdsJson_MainMessage_t");
        cJSON_Delete(root_json);
        return NULL;
    }

    bool parse_ok = true;

    const cJSON *msg_count = cJSON_GetObjectItemCaseSensitive(root_json, "MsgCount");
    const cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(root_json, "Timestamp");
    const cJSON *ati_list_json = cJSON_GetObjectItemCaseSensitive(root_json, "ApproachTrafficInfoList");

    if (cJSON_IsNumber(msg_count)) {
        msg_data->msg_count = msg_count->valueint;
    } else {
        fprintf(stderr, "Error: MsgCount missing or not a number.\n");
        parse_ok = false;
    }

    if (cJSON_IsString(timestamp) && timestamp->valuestring) {
        strncpy(msg_data->timestamp, timestamp->valuestring, sizeof(msg_data->timestamp) - 1);
        msg_data->timestamp[sizeof(msg_data->timestamp) - 1] = '\0';
    } else {
        fprintf(stderr, "Error: Timestamp missing or not a string.\n");
        parse_ok = false;
    }

    if (parse_ok && cJSON_IsArray(ati_list_json)) {
        msg_data->num_approach_traffic_info = cJSON_GetArraySize(ati_list_json);
        if (msg_data->num_approach_traffic_info > 0) {
            msg_data->approach_traffic_info_list = (SdsJson_ApproachTrafficInfoData_t*)calloc(msg_data->num_approach_traffic_info, sizeof(SdsJson_ApproachTrafficInfoData_t));
            if (!msg_data->approach_traffic_info_list) {
                perror("Failed to allocate memory for ApproachTrafficInfoList");
                msg_data->num_approach_traffic_info = 0;
                parse_ok = false;
            } else {
                cJSON* ati_item_json_container = NULL; // {"ApproachTrafficInfo": {...}}
                int idx = 0;
                cJSON_ArrayForEach(ati_item_json_container, ati_list_json) {
                    const cJSON* ati_data_json_value = cJSON_GetObjectItemCaseSensitive(ati_item_json_container, "ApproachTrafficInfo");
                    if (!parse_approach_traffic_info_data(ati_data_json_value, &msg_data->approach_traffic_info_list[idx++])) {
                        fprintf(stderr, "Error parsing one of the ApproachTrafficInfo objects.\n");
                        parse_ok = false;
                        break; // 중단
                    }
                }
            }
        } else {
             msg_data->approach_traffic_info_list = NULL; // 빈 배열일 경우
        }
    } else if (parse_ok) { // parse_ok가 true인데도 ati_list_json이 배열이 아닌 경우
        fprintf(stderr, "Error: ApproachTrafficInfoList missing or not an array.\n");
        parse_ok = false;
    }


    cJSON_Delete(root_json); // cJSON 객체는 항상 해제

    if (!parse_ok) {
        free_sds_json_main_message(msg_data); // 부분적으로 할당된 C 구조체 해제
        return NULL;
    }

    return msg_data;
}

void free_sds_json_main_message(SdsJson_MainMessage_t* msg_data) {
    if (!msg_data) return;

    if (msg_data->approach_traffic_info_list) {
        for (int i = 0; i < msg_data->num_approach_traffic_info; ++i) {
            SdsJson_ApproachTrafficInfoData_t* ati_data = &msg_data->approach_traffic_info_list[i];
            
            // host_object의 waypoint_list 해제
            if (ati_data->host_object.way_point_list) {
                free(ati_data->host_object.way_point_list);
            }
            // remote_object 해제 (존재한다면)
            if (ati_data->remote_object) {
                if (ati_data->remote_object->way_point_list) {
                    free(ati_data->remote_object->way_point_list);
                }
                free(ati_data->remote_object);
            }
            // conflict_pos 해제 (존재한다면)
            if (ati_data->conflict_pos) {
                free(ati_data->conflict_pos);
            }
        }
        free(msg_data->approach_traffic_info_list);
    }
    free(msg_data);
}
