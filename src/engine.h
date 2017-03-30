/*
 *------------------------------------------------------------------------------
 *
 * engine.h
 *
 * Engine class declaration
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
#ifndef ENGINE_H_
#define ENGINE_H_

#include <map>

#include "worker.h"
#include "camera.h"
#include "message_queue.h"
#include "framework.h"

namespace sentry {

/**
 * Engine class
 */
class Engine : public Worker {
  public:
    /** engine constructor */
    Engine (void);

    /** engine destructor */
    virtual ~Engine (void);

    /** start the engine */
    return_code_en start (void);

    /** get the main message queue */
    MessageQueue* get_engine_queue (void) const;

  private:
    Camera *camera;                 /** camera object */
    Worker *rcmgr;                  /** remote control manager worker */
    Worker *chmgr;                  /** chassis manager worker */
    Worker *netcom;                 /** netcom server */
    std::map<int, Worker*> clients; /** netcom uplink workers */
};

} /* namespace sentry */

#endif /* ENGINE_H_ */
