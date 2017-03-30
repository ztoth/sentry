/*
 *------------------------------------------------------------------------------
 *
 * worker.h
 *
 * Sentry worker class declaration
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
#ifndef WORKER_H_
#define WORKER_H_

#include <string>
#include <pthread.h>

#include "message_queue.h"

namespace sentry {

/**
 * Worker class
 */
class Worker {
  public:
    /** worker constructor */
    Worker (const std::string name, const bool need_queue);

    /** worker destructor */
    virtual ~Worker (void);

    /** return the name of the worker */
    std::string get_name (void) const;

    /** return a pointer to the worker's message queue */
    MessageQueue* get_queue (void) const;

  protected:
    /** start the worker thread */
    void run (void);

    /** stop the worker thread */
    void terminate (void);

  private:
    std::string  name;      /** name of the worker */
    pthread_t    thrd;      /** worker thread's pthread handle */
    MessageQueue *queue;    /** message queue for incoming messages */
    bool         running;   /** execution state of the worker thread */

    /** main thread loop */
    virtual void loop (void);

    /** thread init function to be passed to the pthread library */
    static void* thrd_func (void *obj);
};

} /* namespace sentry */

#endif /* WORKER_H_ */
