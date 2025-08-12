#pragma once

#define NOTIFICATIONS_QUEUE_SIZE 5
#define MESSAGE_QUEUE_LEN 10
#define MESSAGE_SIZE 64
#define MAX_MESSAGE_INDEX 63
#define PROTOCOL_SIZE 16
#define PROTOCOL_MESSAGE_MAX_NUM 11
#define CHECKSUM_MODULO 256
// TODO assign final value
#define RECEIVE_BYTE_QUEUE_LEN 128
#define BLANK_CHARACTER ' '
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