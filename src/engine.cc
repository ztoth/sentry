/*
 *------------------------------------------------------------------------------
 *
 * engine.cc
 *
 * Engine implementation
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
#include "engine.h"
#include "rcmgr.h"
#include "chmgr.h"
#include "netcom.h"
#include "message.h"

namespace sentry {

/**
 * Engine constructor
 */
Engine::Engine (void)
        : Worker("engine", true)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_ENGINE,
         "initializing " << get_name());

    /* reset members */
    camera = NULL;
    rcmgr = NULL;
    chmgr = NULL;
    netcom = NULL;
}

/**
 * Engine destructor
 */
Engine::~Engine (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_ENGINE,
         "destroying " << get_name());

    /* destroy remote control manager */
    if (NULL != rcmgr) {
        delete rcmgr;
    }

    /* destroy chassis manager */
    if (NULL != chmgr) {
        delete chmgr;
    }

    /* destroy netcom uplink threads */
    std::map<int, Worker*>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        delete it->second;
    }
    clients.clear();

    /* delete netcom server */
    if (NULL != netcom) {
        delete netcom;
    }

    /* delete camera object */
    if (NULL != camera) {
        delete camera;
    }
}

/**
 * Get the main message queue
 */
MessageQueue*
Engine::get_engine_queue (void) const
{
    return get_queue();
}

/**
 * Start engine
 *
 * First we initialize the objects and worker threads, then start the infinite
 * loop where the engine is waiting for messages from the worker threads. Most
 * of these messages are forwarded to the appropriate worker thread; the main
 * job of engine is to act as a message distributor. An exception is the netcom
 * uplink client, for which engine must dynamically create and destroy worker
 * threads as clients come and go.
 */
return_code_en
Engine::start (void)
{
    /* initialize the objects and worker threads */
    try {
        camera = new Camera();
        rcmgr = new RemoteControlManager(get_queue());
        chmgr = new ChassisManager(get_queue());
        netcom = new Netcom(get_queue());
    } catch (const return_code_en &rc) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_ENGINE,
             "failed to initialize objects, return code " << rc);
        return rc;
    }

    /* start the main loop and process messages from threads */
    message_st *msg = NULL;
    bool loop = true;

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_ENGINE,
         "starting " << get_name() << " loop");

    while (loop) {
        /* go to sleep if there's nothing to do */
        get_queue()->wait_msg();

        /* process messages from the queue */
        if (NULL != (msg = get_queue()->pop_msg())) {
            dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_ENGINE,
                 "message " << message_print(msg));

            /* by default delete the message unless it was forwarded */
            bool msg_forwarded = false;

            switch (msg->type) {
            case MESSAGE_SEARCH_REMOTE: {
                rcmgr->get_queue()->push_msg(msg);
                msg_forwarded = true;
                break;
            }

            case MESSAGE_HEARTBEAT:
            case MESSAGE_SENSOR_REQUEST:
            case MESSAGE_MOVE:
            case MESSAGE_USER_UP:
            case MESSAGE_USER_DOWN: {
                chmgr->get_queue()->push_msg(msg);
                msg_forwarded = true;
                break;
            }

            case MESSAGE_NETCOM_CLIENT_ALIVE: {
                message_netcom_st *netcom_msg =
                    reinterpret_cast<message_netcom_st*>(msg);
                netcom_uplink_st *uplink =
                    reinterpret_cast<netcom_uplink_st*>(netcom_msg->client);
                try {
                    Worker *client_worker = new NetcomUplink(get_queue(),
                                                             uplink, camera);
                    clients.insert(std::pair<int, Worker*>
                                   (netcom_msg->id, client_worker));
                    chmgr->get_queue()->push_msg(MESSAGE_USER_UP);
                } catch (const return_code_en &rc) {
                    dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_ENGINE,
                         "failed to create netcom uplink for client " <<
                         uplink->name);
                }
                break;
            }

            case MESSAGE_NETCOM_CLIENT_DEAD: {
                message_netcom_st *netcom_msg =
                    reinterpret_cast<message_netcom_st*>(msg);
                std::map<int, Worker*>::iterator it =
                    clients.find(netcom_msg->id);
                if (it != clients.end()) {
                    delete it->second;
                    clients.erase(it);
                    chmgr->get_queue()->push_msg(MESSAGE_USER_DOWN);
                }
                break;
            }

            case MESSAGE_CAMERA_REQUEST: {
                message_netcom_st *netcom_msg =
                    reinterpret_cast<message_netcom_st*>(msg);
                std::map<int, Worker*>::iterator it =
                    clients.find(netcom_msg->id);
                if (it != clients.end()) {
                    it->second->get_queue()->push_msg(msg);
                    msg_forwarded = true;
                }
                break;
            }

            case MESSAGE_SENSOR_DATA: {
                message_sensor_st *sensor_msg =
                    reinterpret_cast<message_sensor_st*>(msg);
                std::map<int, Worker*>::iterator it;
                for (it = clients.begin(); it != clients.end(); ++it) {
                    message_sensor_st *tmp_msg = new message_sensor_st;
                    *tmp_msg = *sensor_msg;
                    it->second->get_queue()->push_msg(tmp_msg);
                }
                break;
            }

            case MESSAGE_TERMINATE: {
                loop = false;
                break;
            }

            default:
                break;
            }

            /* delete the message unless it was forwarded */
            if (!msg_forwarded) {
                delete msg;
            }
        }
    }

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_ENGINE,
         get_name() << " terminating");

    return RC_OK;
}

} /* namespace sentry */
