#pragma once

#define NOTIFICATIONS_QUEUE_SIZE 5
#define MESSAGE_QUEUE_LEN 10
#define MESSAGE_SIZE 64
#define MAX_MESSAGE_INDEX 63

// protocol buffer
#define PROTOCOL_SIZE 16
#define PROTOCOL_MESSAGE_MAX_NUM 11
#define PROTOCOL_IP_INDEX 6
#define MESSAGES_QUANTITY_INDEX 7
#define PROTOCOL_MESSAGE_START_INDEX 8
#define PROTOCOL_CHECKSUM_INDEX 14
#define PROTOCOL_MESSAGE_LENGTH 6

#define CHECKSUM_MODULO 256
// TODO assign final value
#define RECEIVE_BYTE_QUEUE_LEN 128
#define BLANK_CHARACTER 0
// TODO assign final value
#define SUSPEND_TASK_TIME_SHORT 200 // 0.2s
// TODO assign final value
#define SUSPEND_TASK_TIME_LONG 2000 // 2s
// TODO assign final value
#define RECEIVE_BYTE_TIMEOUT 100 // 0.1s
// TODO assign final value
#define RECEIVE_MESSAGE_TIMEOUT 1000 // 1s
// TODO assign final value
#define REPEAT_LAST_MESSAGE_MAX_ATTEMPTS 3
// TODO assign final value
#define PING_MAX_ATTEMPTS 3
// TODO assign final value
#define DELAY_BETWEEN_MESSAGES 40

#define ACK_NUMBER_INDEX 0
#define START_ACK_NUMBER 1
// TODO assign final value
#define CONNECTION_REQUEST_TIMEOUT 30000 // 30s

// special messages
#define SPECIAL_MESSAGE_LEN 6
#define REPEAT_MESSAGE "\0repea"
#define PING_MESSAGE "\0ping\0"
#define RE_PING_MESSAGE "\0repin"
