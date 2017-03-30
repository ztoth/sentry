/*
 *------------------------------------------------------------------------------
 *
 * message.cc
 *
 * Message related helper functions
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
#include <sstream>

#include "message.h"

/**
 * Message type enum to string conversion
 */
const char*
message_type_str (const message_type_en type)
{
    static const std::string message_type_strs[] = {
        MESSAGE_TYPE_DEF(MESSAGE_TYPE_STR)
    };

    if (type >= MESSAGE_TYPE_COUNT) {
        return "<unknown>";
    }
    return message_type_strs[type].c_str();
}

/**
 * Move direction enum to string conversion
 */
const char*
move_direction_str (const move_direction_en direction)
{
    static const std::string move_direction_strs[] = {
        MOVE_DIRECTION_DEF(MOVE_DIRECTION_STR)
    };

    if (direction >= MOVE_DIRECTION_COUNT) {
        return "<unknown>";
    }
    return move_direction_strs[direction].c_str();
}

/**
 * Sensor type enum to string conversion
 */
const char*
sensor_type_str (const sensor_type_en type)
{
    static const std::string sensor_type_strs[] = {
        SENSOR_TYPE_DEF(SENSOR_TYPE_STR)
    };

    if (type >= SENSOR_TYPE_COUNT) {
        return "<unknown>";
    }
    return sensor_type_strs[type].c_str();
}

/**
 * Return length of message based on its type
 */
size_t
message_length (const message_st *msg)
{
    switch (msg->type) {
    case MESSAGE_MOVE: {
        return sizeof(message_move_st);
    }

    case MESSAGE_SENSOR_DATA: {
        return sizeof(message_sensor_st);
    }

    case MESSAGE_CAMERA_FRAME: {
        return sizeof(message_frame_st);
    }

    case MESSAGE_NETCOM_CONNECT: {
        return sizeof(message_connect_st);
    }

    case MESSAGE_NETCOM_KEY: {
        return sizeof(message_key_st);
    }

    case MESSAGE_NETCOM_CLIENT_ALIVE:
    case MESSAGE_NETCOM_CLIENT_DEAD: {
        return sizeof(message_netcom_st);
    }

    default:
        return sizeof(message_st);
    }
}

/**
 * Print message
 */
const char*
message_print (message_st *msg)
{
    message_type_en type = static_cast<message_type_en>(msg->type);
    std::stringstream strstr;

    /* for most message we only print type and length */
    strstr << "type " << message_type_str(type)
           << " length " << message_length(msg);

    switch (type) {
    case MESSAGE_CAMERA_FRAME: {
        message_frame_st *fmsg = reinterpret_cast<message_frame_st*>(msg);
        strstr << " frame size " << fmsg->frame_size << " bytes";
        break;
    }

    case MESSAGE_SENSOR_DATA: {
        message_sensor_st *smsg = reinterpret_cast<message_sensor_st*>(msg);
        sensor_type_en stype = static_cast<sensor_type_en>(smsg->sensor);
        strstr << " sensor type " << sensor_type_str(stype)
               << " data " << smsg->data;
        break;
    }

    case MESSAGE_MOVE: {
        message_move_st *mmsg = reinterpret_cast<message_move_st*>(msg);
        move_direction_en direction = static_cast<move_direction_en>(mmsg->direction);
        strstr << " direction " << move_direction_str(direction);
        break;
    }

    case MESSAGE_NETCOM_CONNECT: {
        message_connect_st *cmsg = reinterpret_cast<message_connect_st*>(msg);
        strstr << " id " << cmsg->id;
        break;
    }

    case MESSAGE_NETCOM_CLIENT_ALIVE:
    case MESSAGE_NETCOM_CLIENT_DEAD: {
        message_netcom_st *nmsg = reinterpret_cast<message_netcom_st*>(msg);
        strstr << " id " << nmsg->id;
        break;
    }

    default:
        break;
    }

    return strstr.str().c_str();
}
