/*
 *------------------------------------------------------------------------------
 *
 * chmgr.cc
 *
 * Chassis manager implementation
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>

#include "chmgr.h"

namespace sentry {

/**
 * Chassis manager constructor
 */
ChassisManager::ChassisManager (MessageQueue* const engine_queue)
        : Worker("chassis manager", true), engine_queue(engine_queue)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
         "initializing " << get_name());

    /* read configuration */
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
         "parsing file " << framework::config_file << " for " << get_name() <<
         " config");
    config = new framework::Config("chmgr");

    /* reset local variables */
    num_users = 0;
    port = port_invalid;
    recv_running = false;

    /* try to open the serial port */
    try {
        open_serial_port();
    } catch (const return_code_en &rc) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
             "unable to open the serial port, error code " << rc);
    }

    /* ready to start the worker thread */
    run();
}

/**
 * Chassis manager destructor
 */
ChassisManager::~ChassisManager (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
         "destroying " << get_name());

    /* terminate the worker thread */
    terminate();

    /* reset old attributes and close serial port */
    if (port_invalid != port) {
        tcsetattr(port, TCSANOW, &port_attr);
        close_serial_port();
    }

    delete config;
}

/**
 * Open the serial port
 *
 * The following attributes are configured:
 *   - ignore framing errors and parity errors (input mode)
 *   - reset all output mode flags
 *   - set baud rate to 9600 (control mode)
 *   - set character size to 8 bits, no parity, no stop bits (control mode)
 *   - enable receiver (control mode)
 *   - switch to non-canonical mode to get data immediately without waiting
 *     for EOL (local mode)
 *   - set minimum number of characters for non-canonical read to 1, so
 *     that read gets blocked until data is available
 */
void
ChassisManager::open_serial_port (void)
{
    if (port_invalid != port) {
        /* already open */
        return;
    }

    /* open serial port */
    port = open(config->get_string("serial").c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (port_invalid == port) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
             "open() failed with " << strerror(errno));
        throw RC_CHMGR_PORT_ERROR;
    }

    /* set port to blocking mode (ie. block in read if there's no data) */
    int port_flags = fcntl(port, F_GETFL, 0);
    if (fcntl(port, F_SETFL, port_flags & ~O_NONBLOCK) == -1) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
             "fcntl() failed with " << strerror(errno));
        close_serial_port();
        throw RC_CHMGR_PORT_ERROR;
    }

    /* configure the port (see function comment for details) */
    if (tcgetattr(port, &port_attr) != 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
             "tcgetattr() failed with " << strerror(errno));
        close_serial_port();
        throw RC_CHMGR_PORT_ERROR;
    }

    struct termios new_attr = port_attr;
    new_attr.c_iflag = IGNPAR;
    new_attr.c_oflag = 0;
    new_attr.c_cflag = B9600 | CS8 | CREAD;
    new_attr.c_lflag &= ~ICANON;
    new_attr.c_cc[VMIN] = 1;
    if (tcsetattr(port, TCSANOW, &new_attr) != 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
             "tcsetattr() failed with " << strerror(errno));
        close_serial_port();
        throw RC_CHMGR_PORT_ERROR;
    }

    /* port is ready, spawn a thread for processing incoming data */
    if (pthread_create(&recv_thrd, 0, receive_thread, this) != 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
             "unable to start receive thread");
        throw RC_CHMGR_THREAD_ERROR;
    }
    pthread_setname_np(recv_thrd, "serial receiver");
    recv_running = true;

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
         "serial port opened successfully");
}

/**
 * Close serial port
 */
void
ChassisManager::close_serial_port (void)
{
    if (port_invalid == port) {
        /* port is not open */
        return;
    }

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
         "closing serial port");

    /* stop the receive thread */
    if (recv_running) {
        pthread_cancel(recv_thrd);
        pthread_join(recv_thrd, NULL);
        recv_running = false;
    }

    /* close the serial port */
    close(port);
    port = port_invalid;
}

/**
 * Send message via the serial port
 */
void
ChassisManager::send_data (const char *buf, const int len)
{
    /* ensure port is open */
    if (port_invalid == port) {
        try {
            open_serial_port();
        } catch (const return_code_en &rc) {
            dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
                 "serial port is down, cannot send command");
            throw RC_CHMGR_WRITE_ERROR;
        }
    }

    /* send the message */
    int sent_bytes = 0;
    while (sent_bytes < len) {
        char c = buf[sent_bytes++];
        if (write(port, &c, 1) == -1) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_CHMGR,
                 "write() returned error " << strerror(errno));
            throw RC_CHMGR_WRITE_ERROR;
        }
    }

    /* send message delimiter */
    char c = ':';
    if (write(port, &c, 1) == -1) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_CHMGR,
             "write() returned error " << strerror(errno));
        throw RC_CHMGR_WRITE_ERROR;
    }
}

/**
 * Read data from serial port
 */
int
ChassisManager::read_data (char *buf, const int maxlen)
{
    /* ensure port is open */
    if (port_invalid == port) {
        try {
            open_serial_port();
        } catch (const return_code_en &rc) {
            dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CHMGR,
                 "serial port is down, cannot read data");
            throw RC_CHMGR_READ_ERROR;
        }
    }

    /* read from serial until we hit the message delimiter */
    int read_bytes = 0;
    char c;
    do {
        if (read(port, &c, 1) == -1) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_CHMGR,
                 "read() returned error " << strerror(errno));
            throw RC_CHMGR_READ_ERROR;
        }
        if (read_bytes < maxlen) {
            buf[read_bytes++] = c;
        }
    } while (c != ':');

    return read_bytes;
}

/**
 * Send a stop command to the chassis
 */
void
ChassisManager::send_stop_chassis (void)
{
    message_move_st *msg = new message_move_st;
    msg->type = MESSAGE_MOVE;
    msg->direction = STOP;
    try {
        dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_CHMGR,
             "sending STOP command to chassis");
        send_data((char*)msg, sizeof(*msg));
    } catch (const return_code_en &rc) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_CHMGR,
             "unable to send command, error code " << rc);
    }
}

/**
 * Serial data receive thread
 *
 * This thread listens to messages from the chassis via the serial port, and
 * forwards them to sentry.
 */
void*
ChassisManager::receive_thread (void *args)
{
    ChassisManager *chmgr = reinterpret_cast<ChassisManager*>(args);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    /* keep reading data from chassis forever */
    while (true) {
        message_sensor_st *msg = new message_sensor_st;
        try {
            chmgr->read_data(reinterpret_cast<char*>(msg), sizeof(*msg));
            chmgr->engine_queue->push_msg(msg);
        } catch (const return_code_en &rc) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_CHMGR,
                 "problem reading serial data, error code " << rc);
            delete msg;
        }
    }

    pthread_exit(NULL);
}

/**
 * Chassis manager thread loop
 *
 * This thread listens to messages from sentry, such as heartbeat, movement
 * command, sensor data request, etc, and forwards them to the chassis via
 * the serial port.
 */
void
ChassisManager::loop (void)
{
    message_st *msg;
    bool loop = true;

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
         "starting " << get_name() << " loop");

    while (loop) {
        /* go to sleep if there's nothing to do */
        get_queue()->wait_msg();

        /* process messages */
        if (NULL != (msg = get_queue()->pop_msg())) {
            dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_CHMGR,
                 "message " << message_print(msg));

            switch (msg->type) {
            case MESSAGE_MOVE:
            case MESSAGE_HEARTBEAT:
            case MESSAGE_SENSOR_REQUEST: {
                try {
                    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_CHMGR,
                         "forwarding message to chassis");
                    send_data((char*)msg, message_length(msg));
                } catch (const return_code_en &rc) {
                    dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_CHMGR,
                         "unable to send message, error code " << rc);
                }
                break;
            }

            case MESSAGE_USER_UP: {
                num_users++;
                dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
                     "registering new user, total active " << num_users);
                break;
            }

            case MESSAGE_USER_DOWN: {
                if (num_users > 0) {
                    num_users--;
                    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
                         "unregistering a user, remaining " << num_users);
                    if (0 == num_users) {
                        /* last user, send a stop command to the robot */
                        send_stop_chassis();
                    }
                } else {
                    dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_CHMGR,
                         "number of users going negative!");
                }
                break;
            }

            case MESSAGE_TERMINATE: {
                send_stop_chassis();
                loop = false;
                break;
            }

            default:
                break;
            }

            /* free the message */
            delete msg;
        }
    }

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CHMGR,
         get_name() << " terminating");
}

} /* namespace sentry */
