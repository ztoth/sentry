/*
 *------------------------------------------------------------------------------
 *
 * rcmgr.cc
 *
 * Remote control manager implementation
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
#include <ctime>

#include "rcmgr.h"

namespace sentry {

/**
 * Remote control manager constructor
 */
RemoteControlManager::RemoteControlManager (MessageQueue* const engine_queue)
        : Worker("RC manager", true), engine_queue(engine_queue)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
         "initializing " << get_name());

    /* read configuration */
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
         "parsing file " << framework::config_file << " for " << get_name() <<
         " config");
    config = new framework::Config("rcmgr");

    /* initialize wii subsystem */
    wii = new CWii();

    /* ready to start the worker thread */
    run();
}

/**
 * Remote control manager destructor
 */
RemoteControlManager::~RemoteControlManager (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
         "destroying " << get_name());

    /* terminate the worker thread */
    terminate();

    /* clean up the wii subsystem */
    delete config;
    delete wii;
}

/**
 * Connect remote controllers
 */
void
RemoteControlManager::connect (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
         "connecting remote controllers");

    int i;
    int LED_MAP[4] = {CWiimote::LED_1, CWiimote::LED_2,
                      CWiimote::LED_3, CWiimote::LED_4};
    std::vector<CWiimote> &wiimotes = wii->Connect();
    std::vector<CWiimote>::iterator it;
    for(i = 0, it = wiimotes.begin(); it != wiimotes.end(); ++it, ++i) {
        CWiimote &wiimote = *it;
        wiimote.SetLEDs(LED_MAP[i]);
        wiimote.SetRumbleMode(CWiimote::ON);
        usleep(200000);
        wiimote.SetRumbleMode(CWiimote::OFF);
        engine_queue->push_msg(MESSAGE_USER_UP);
    }
}

/**
 * Send generic command to main
 */
void
RemoteControlManager::send_command (message_type_en type)
{
    message_st *msg = new message_st;
    msg->type = type;

    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_RCMGR,
         "sending message " << message_print(msg) << " to main");

    engine_queue->push_msg(msg);
}

/**
 * Send chassis move event to main
 */
void
RemoteControlManager::send_move_event (const move_direction_en direction)
{
    message_move_st *msg = new message_move_st;
    msg->type = MESSAGE_MOVE;
    msg->direction = direction;

    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_RCMGR,
         "sending message " << message_print(msg) << " to main");

    engine_queue->push_msg(msg);
}

/**
 * Handle button event from a remote controller
 */
void
RemoteControlManager::handle_buttons (CWiimote &wiimote)
{
    /* move forward */
    if (wiimote.Buttons.isJustPressed(CButtons::BUTTON_TWO)) {
        send_move_event(MOVE_FORWARD);
    }

    /* move backward */
    if (wiimote.Buttons.isJustPressed(CButtons::BUTTON_ONE)) {
        send_move_event(MOVE_BACKWARD);
    }

    /* turn left */
    if (wiimote.Buttons.isJustPressed(CButtons::BUTTON_UP)) {
        send_move_event(TURN_LEFT);
    }

    /* turn right */
    if (wiimote.Buttons.isJustPressed(CButtons::BUTTON_DOWN)) {
        send_move_event(TURN_RIGHT);
    }

    /* rotate camera up */
    if (wiimote.Buttons.isJustPressed(CButtons::BUTTON_PLUS)) {
        send_move_event(ROTATE_UP);
    }

    /* rotate camera down */
    if (wiimote.Buttons.isJustPressed(CButtons::BUTTON_MINUS)) {
        send_move_event(ROTATE_DOWN);
    }

    /* send sensor request */
    if (wiimote.Buttons.isJustPressed(CButtons::BUTTON_A)) {
        send_command(MESSAGE_SENSOR_REQUEST);
    }

    /* stop if a button is released and no other button is pressed */
    if ((wiimote.Buttons.isReleased(CButtons::BUTTON_TWO)) ||
        (wiimote.Buttons.isReleased(CButtons::BUTTON_ONE)) ||
        (wiimote.Buttons.isReleased(CButtons::BUTTON_UP)) ||
        (wiimote.Buttons.isReleased(CButtons::BUTTON_DOWN))) {
        if (wiimote.Buttons.isPressed(CButtons::BUTTON_TWO)) {
            send_move_event(MOVE_FORWARD);
        } else if (wiimote.Buttons.isPressed(CButtons::BUTTON_ONE)) {
            send_move_event(MOVE_BACKWARD);
        } else if (wiimote.Buttons.isPressed(CButtons::BUTTON_UP)) {
            send_move_event(TURN_LEFT);
        } else if (wiimote.Buttons.isPressed(CButtons::BUTTON_DOWN)) {
            send_move_event(TURN_RIGHT);
        } else {
            send_move_event(STOP);
        }
    }
}

/**
 * Handle events from the remote controllers
 */
void
RemoteControlManager::handle_events (void)
{
    bool refresh = false;
    std::vector<CWiimote> &wiimotes = wii->GetWiimotes(false);
    std::vector<CWiimote>::iterator it;

    for (it = wiimotes.begin(); it != wiimotes.end(); ++it) {
        CWiimote &wiimote = *it;

        switch (wiimote.GetEvent()) {
        case CWiimote::EVENT_EVENT: {
            handle_buttons(wiimote);
            break;
        }

        case CWiimote::EVENT_DISCONNECT:
        case CWiimote::EVENT_UNEXPECTED_DISCONNECT: {
            dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
                 "disconnecting remote controller");
            wiimote.Disconnect();
            engine_queue->push_msg(MESSAGE_USER_DOWN);
            refresh = true;
            break;
        }

        default:
            break;
        }
    }

    if (refresh) {
        wiimotes = wii->GetWiimotes(true);
    }
}

/**
 * Main thread loop
 *
 * The main job of this thread is to handle remote controller connections and
 * button events. If there are no remote controllers connected, the thread goes
 * to sleep until sentry wakes him up, for example, when a search remote
 * controller event is received from a netcom client.
 */
void
RemoteControlManager::loop (void)
{
    message_st *msg;
    int search = config->get_int("retries");
    bool loop = true;
    int last_heartbeat = 0;

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
         "starting " << get_name() << " loop");

    while (loop) {
        /* go to sleep if there's nothing to do */
        if ((0 == search) && (0 == wii->GetNumConnectedWiimotes())) {
            get_queue()->wait_msg();
        }

        /* process messages */
        if (NULL != (msg = get_queue()->pop_msg())) {
            dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_RCMGR,
                 "message " << message_print(msg));

            switch (msg->type) {
            case MESSAGE_SEARCH_REMOTE: {
                search = config->get_int("retries");
                break;
            }

            case MESSAGE_TERMINATE: {
                loop = false;
                break;
            }

            default:
                break;
            }

            /* free the message */
            delete msg;
        }

        /* handle remote controllers */
        if (0 == wii->GetNumConnectedWiimotes()) {
            if (0 != search) {
                search--;
                dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
                     "searching for remote controllers");
                if (0 != wii->Find(config->get_int("bt_timeout"))) {
                    connect();
                    search = 0;
                } else {
                    if (0 == search) {
                        dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
                             "nothing found, giving up");
                    } else {
                        dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
                             "nothing found, remaining tries " << search);
                    }
                }
            }
        } else {
            if (wii->Poll()) {
                handle_events();
            }

            /*
             * When remote controllers are connected, we must send heartbeat to
             * the chassis manager every second
             */
            int now = time(NULL);
            if (now - last_heartbeat) {
                dbug(DEBUG_LEVEL_VERY_VERBOSE, DEBUG_TYPE_RCMGR,
                     "sending heartbeat to main");
                last_heartbeat = now;
                engine_queue->push_msg(MESSAGE_HEARTBEAT);
            }
        }
    }

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_RCMGR,
         get_name() << " terminating");
}

} /* namespace sentry */
