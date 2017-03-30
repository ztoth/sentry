/*
 *------------------------------------------------------------------------------
 *
 * rcmgr.h
 *
 * Remote control manager class declaration
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
#ifndef RCMGR_H_
#define RCMGR_H_

#include <wiiusecpp.h>

#include "worker.h"
#include "message.h"
#include "message_queue.h"
#include "framework.h"

namespace sentry {

/**
 * RemoteControlManager class
 */
class RemoteControlManager : public Worker {
  public:
    /** remote control manager constructor */
    RemoteControlManager (MessageQueue* const engine_queue);

    /** remote control manager destructor */
    virtual ~RemoteControlManager (void);

  private:
    framework::Config *config;        /** RC manager configuration */
    MessageQueue* const engine_queue; /** main message queue */
    CWii *wii;                        /** wii controller object */

    /** main thread loop */
    void loop (void);

    /** connect remote controllers */
    void connect (void);

    /** send generic command to main */
    void send_command (message_type_en type);

    /** send chassis move event to main */
    void send_move_event (const move_direction_en direction);

    /** handle button event from a remote controller */
    void handle_buttons (CWiimote &wiimote);

    /** handle events from the remote controllers */
    void handle_events (void);
};

} /* namespace sentry */

#endif /* RCMGR_H_ */
