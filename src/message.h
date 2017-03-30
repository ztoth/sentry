/*
 *------------------------------------------------------------------------------
 *
 * message.h
 *
 * Message types and structures
 *
 * Copyright (c) 2017 Zoltan Toth <ztoth AT thetothfamily DOT net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 *------------------------------------------------------------------------------
 */
#ifndef MESSAGE_H_
#define MESSAGE_H_

/* this file is used by the Arduino code, make sure it builds for both targets */

#include <stdint.h>
#include <string.h>

/** message types x-macro */
#define MESSAGE_TYPE_DEF(list_macro)                                \
    list_macro(MESSAGE_INVALID,             "INVALID"),             \
    list_macro(MESSAGE_TERMINATE,           "TERMINATE"),           \
    list_macro(MESSAGE_SEARCH_REMOTE,       "SEARCH_REMOTE"),       \
    list_macro(MESSAGE_CAMERA_REQUEST,      "CAMERA_REQUEST"),      \
    list_macro(MESSAGE_CAMERA_FRAME,        "CAMERA_FRAME"),        \
    list_macro(MESSAGE_SENSOR_REQUEST,      "SENSOR_REQUEST"),      \
    list_macro(MESSAGE_SENSOR_DATA,         "SENSOR_DATA"),         \
    list_macro(MESSAGE_MOVE,                "MOVE"),                \
    list_macro(MESSAGE_USER_UP,             "USER_UP"),             \
    list_macro(MESSAGE_USER_DOWN,           "USER_DOWN"),           \
    list_macro(MESSAGE_HEARTBEAT,           "HEARTBEAT"),           \
    list_macro(MESSAGE_NETCOM_CONNECT,      "NETCOM_CONNECT"),      \
    list_macro(MESSAGE_NETCOM_KEY,          "NETCOM_KEY"),          \
    list_macro(MESSAGE_NETCOM_CLIENT_ALIVE, "NETCOM_CLIENT_ALIVE"), \
    list_macro(MESSAGE_NETCOM_CLIENT_DEAD,  "NETCOM_CLIENT_DEAD"),  \

/** message types */
#define MESSAGE_TYPE_ENUM(__enum, __str) __enum
typedef enum message_type {
    MESSAGE_TYPE_DEF(MESSAGE_TYPE_ENUM)
    MESSAGE_TYPE_COUNT,
} message_type_en;

/** helper function to print message types in human readable format */
#define MESSAGE_TYPE_STR(__enum, __str) __str
extern const char* message_type_str(const message_type_en type);

/** movement direction flags x-macro */
#define MOVE_DIRECTION_DEF(list_macro)          \
    list_macro(STOP,          "STOP"),          \
    list_macro(MOVE_FORWARD,  "MOVE_FORWARD"),  \
    list_macro(MOVE_BACKWARD, "MOVE_BACKWARD"), \
    list_macro(TURN_LEFT,     "TURN_LEFT"),     \
    list_macro(TURN_RIGHT,    "TURN_RIGHT"),    \
    list_macro(ROTATE_UP,     "ROTATE_UP"),     \
    list_macro(ROTATE_DOWN,   "ROTATE_DOWN"),   \

/** movement direction flags */
#define MOVE_DIRECTION_ENUM(__enum, __str) __enum
typedef enum move_direction {
    MOVE_DIRECTION_DEF(MOVE_DIRECTION_ENUM)
    MOVE_DIRECTION_COUNT
} move_direction_en;

/** helper function to print movement directions in human readable format */
#define MOVE_DIRECTION_STR(__enum, __str) __str
extern const char* move_direction_str(const move_direction_en direction);

/** sensor types x-macro*/
#define SENSOR_TYPE_DEF(list_macro)                \
    list_macro(SENSOR_INVALID,     "INVALID"),     \
    list_macro(SENSOR_DISTANCE,    "DISTANCE"),    \
    list_macro(SENSOR_TEMPERATURE, "TEMPERATURE"), \

/** sensor types */
#define SENSOR_TYPE_ENUM(__enum, __str) __enum
typedef enum sensor_type {
    SENSOR_TYPE_DEF(SENSOR_TYPE_ENUM)
    SENSOR_TYPE_COUNT
} sensor_type_en;

/** helper function to print sensor type in human readable format */
#define SENSOR_TYPE_STR(__enum, __str) __str
extern const char* sensor_type_str(const sensor_type_en type);

/** maximum buffer size in bytes */
const int max_buf_size = 512;

/** simple message header structure */
typedef struct message {
    uint32_t type;   /** message type */
} message_st;

/** robot movement message from clients */
typedef struct message_move : message_st {
    uint32_t direction;   /** movement direction flag */
} message_move_st;

/** sensor data message */
typedef struct message_sensor : message_st {
    uint16_t sensor;   /** sensor type */
    uint16_t data;     /** sensor data */
} message_sensor_st;

/** camera frame message */
typedef struct message_frame : message_st {
    uint32_t frame_size;        /** total size of the frame */
    uint16_t cols;              /** cols, also known as width */
    uint16_t rows;              /** rows, also known as height */
    uint16_t frag_size;         /** current fragment size */
    uint16_t frag_seq;          /** fragment sequence number */
    char frame[max_buf_size];   /** frame data */
} message_frame_st;

/** netcom client connect message */
typedef struct message_connect : message_st {
    uint32_t id;              /** client ID */
    char otp[max_buf_size];   /** one-time password generated by server */
} message_connect_st;

/** random key generated by server for each client */
typedef struct message_key : message_st {
    char key[max_buf_size];   /** key generated by server */
} message_key_st;

/** netcom client message */
typedef struct message_netcom : message_st {
    int32_t id;     /** client ID */
    void *client;   /** pointer to uplink data */
} message_netcom_st;

/** message related helper functions */
extern size_t message_length(const message_st *msg);
extern const char* message_print(message_st *msg);

#endif /* MESSAGE_H_ */
