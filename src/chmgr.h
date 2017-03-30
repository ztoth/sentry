/*
 *------------------------------------------------------------------------------
 *
 * chmgr.h
 *
 * Chassis manager class declaration
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
#ifndef CHMGR_H_
#define CHMGR_H_

#include <pthread.h>
#include <termios.h>
#include <string>

#include "worker.h"
#include "message.h"
#include "message_queue.h"
#include "framework.h"

namespace sentry {

/**
 * ChassisManager class
 */
class ChassisManager : public Worker {
  public:
    /** chassis manager constructor */
    ChassisManager (MessageQueue* const engine_queue);

    /** chassis manager destructor */
    virtual ~ChassisManager (void);

  private:
    framework::Config *config;        /** chassis manager configuration */
    MessageQueue* const engine_queue; /** main message queue */
    pthread_t recv_thrd;              /** serial data receive thread */
    bool recv_running;                /** flag to indicate receive thread is running */
    int num_users;                    /** number of active sentry users */
    int port;                         /** file descriptor used for serial port access */
    struct termios port_attr;         /** serial port settings */

    /** flag to indicate serial port is not open */
    static const int port_invalid = -1;

    /** main thread loop */
    void loop (void);

    /** open serial port */
    void open_serial_port (void);

    /** close serial port */
    void close_serial_port (void);

    /** send message via the serial port */
    void send_data (const char *buf, const int len);

    /** read binary data from serial port */
    int read_data (char *buf, const int maxlen);

    /** send a stop command to the robot */
    void send_stop_chassis (void);

    /** serial data receive thread */
    static void* receive_thread (void *args);
};

} /* namespace sentry */

#endif /* CHMGR_H_ */
