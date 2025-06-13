// VMSprotocol.h

#ifndef VMS_PROTOCOL_H
#define VMS_PROTOCOL_H

#include <stdint.h> // for uint8_t, uint16_t, int16_t, uint32_t
#include <stddef.h> // for size_t

// M30 전광판 제어 프로토콜 (문자/제어) 관련 상수
#define TEXT_CONTROL_STX 0x02
#define TEXT_CONTROL_ETX 0x03

// 예시 명령어 타입
#define CMD_TYPE_INSERT      0x84 // 광고 추가
#define CMD_TYPE_POWER       0x86 // 전원 ON/OFF
#define CMD_TYPE_SIGNAL_A    0x87 // 신호 A ON/OFF
#define CMD_TYPE_SIGNAL_B    0x88 // 신호 B ON/OFF
#define CMD_TYPE_BRIGHTNESS  0x89 // 밝기

// 이미지 전송 프로토콜 관련 상수
#define IMAGE_START_BYTE 0xAA

// 예시 이미지 명령어
#define CMD_IMG_BUF_INIT 0xB0 // 이미지 버퍼 초기화
#define CMD_IMG_DATA_TX  0xB1 // 이미지 데이터 전송

// 이미지 타입
#define IMG_TYPE_BITMAP 1
#define IMG_TYPE_PNG    2

// 이미지 전송 프로토콜의 IMAGE_HEADER 구조체 [cite: 5]
typedef struct {
    uint8_t type;      // [1] Bitmap, [2] PNG
    uint8_t rsv[3];    // Reserved
    int16_t sx;        // 이미지 표출 위치 X
    int16_t sy;        // 이미지 표출 위치 Y
    int16_t width;     // 이미지 표출 가로 크기
    int16_t height;    // 이미지 표출 세로 크기
} IMAGE_HEADER;

/**
 * @brief M30 전광판 제어 프로토콜에 맞는 패킷을 생성합니다. (문자/제어용)
 * @param command_type 프로토콜의 명령어 (예: CMD_TYPE_INSERT, CMD_TYPE_POWER)
 * @param data_str DATA 필드에 들어갈 문자열. 내부적으로 각 문자를 2바이트(하위바이트 우선)로 변환합니다.
 * @param out_packet_len 생성된 전체 패킷의 길이가 저장될 포인터.
 * @return 성공 시 생성된 패킷 데이터 (동적 할당됨, 사용 후 free 필요), 실패 시 NULL.
 */
uint8_t* create_text_control_packet(uint8_t command_type, const char* data_str, uint16_t* out_packet_len);

/**
 * @brief 이미지 전송 프로토콜에 맞는 패킷을 생성합니다.
 * @param image_type 이미지 종류 (IMG_TYPE_BITMAP 또는 IMG_TYPE_PNG)
 * @param image_command 이미지 프로토콜 명령어 (예: CMD_IMG_DATA_TX)
 * @param sx 이미지 표출 X 좌표
 * @param sy 이미지 표출 Y 좌표
 * @param width 이미지 가로 크기
 * @param height 이미지 세로 크기
 * @param image_filepath 전송할 이미지 파일의 경로 (실행 파일과 같은 bin 디렉토리 내 파일 이름)
 * @param out_packet_len 생성된 전체 패킷의 길이가 저장될 포인터.
 * @return 성공 시 생성된 패킷 데이터 (동적 할당됨, 사용 후 free 필요), 실패 시 NULL.
 */
uint8_t* create_image_packet(uint8_t image_type, uint8_t image_command,
                             int16_t sx, int16_t sy, int16_t width, int16_t height,
                             const char* image_filepath, uint32_t* out_packet_len);

/**
 * @brief CMD_IMG_BUF_INIT (이미지 버퍼 초기화) 패킷을 생성합니다.
 * @param out_packet_len 생성된 전체 패킷의 길이가 저장될 포인터.
 * @return 성공 시 생성된 패킷 데이터 (동적 할당됨, 사용 후 free 필요), 실패 시 NULL.
 */
uint8_t* create_image_buffer_init_packet(uint32_t* out_packet_len);


#endif // VMS_PROTOCOL_H
