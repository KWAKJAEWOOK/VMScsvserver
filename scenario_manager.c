#include "scenario_manager.h" // VMS_ScenarioRule_t, VMS_ScenarioList_t 정의
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

VMS_ScenarioList_t* load_scenarios_from_csv(const char* csv_filepath) {
    FILE* file = fopen(csv_filepath, "r");
    if (!file) {
        perror("Failed to open scenario CSV file");
        fprintf(stderr, "Scenario file path attempted: %s\n", csv_filepath);
        return NULL;
    }

    VMS_ScenarioList_t* list = (VMS_ScenarioList_t*)calloc(1, sizeof(VMS_ScenarioList_t));
    if (!list) {
        perror("Failed to allocate ScenarioList");
        fclose(file);
        return NULL;
    }

    char line[1024];
    // 첫 줄(헤더)은 건너뛰기
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Scenario file is empty or cannot be read.\n");
        fclose(file);
        free(list);
        return NULL;
    }

    while (fgets(line, sizeof(line), file)) {
        // 줄 끝의 개행 문자 제거
        line[strcspn(line, "\r\n")] = 0;
        // if (strlen(line) < 5) continue; // 데이터가 거의 없는 라인 스킵

        list->count++;
        VMS_ScenarioRule_t* new_rules = (VMS_ScenarioRule_t*)realloc(list->rules, sizeof(VMS_ScenarioRule_t) * list->count);
        if (!new_rules) {
            perror("Failed to realloc scenario rules");
            free_scenario_list(list); // 이전에 할당된 부분까지 정리
            fclose(file);
            return NULL;
        }
        list->rules = new_rules;
        VMS_ScenarioRule_t* r = &list->rules[list->count - 1];
        memset(r, 0, sizeof(VMS_ScenarioRule_t)); // 새로 할당된 메모리 초기화

        // CSV의 각 필드를 파싱
        int items = sscanf(line, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
               &r->event_id, &r->entry_direction_code, &r->egress_direction_code,
               &r->conflict_direction_code, &r->A1, &r->A2, &r->A3,
               &r->B1, &r->B2, &r->B3, &r->C1, &r->C2, &r->C3,
               &r->D1, &r->D2, &r->D3);

        if (items < 16) { // 16개 필드가 모두 있어야 함
            fprintf(stderr, "Warning: Skipping malformed line in scenario.csv: %s\n", line);
            list->count--; // 잘못된 라인은 카운트에서 제외하고, realloc된 메모리는 그대로 둠
        }
    }

    fclose(file);
    printf("[ScenarioManager] Loaded %d rules from %s\n", list->count, csv_filepath);
    return list;
}

void free_scenario_list(VMS_ScenarioList_t* list) {
    if (!list) return;
    if (list->rules) {
        free(list->rules);
    }
    free(list);
}
