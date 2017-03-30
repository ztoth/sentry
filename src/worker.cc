/*
 *------------------------------------------------------------------------------
 *
 * worker.cc
 *
 * Sentry worker implementation
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
#include "worker.h"
#include "message.h"
#include "framework.h"

namespace sentry {

/**
 * Worker constructor
 */
Worker::Worker (const std::string name, const bool need_queue)
        : name(name)
{
    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_WORKER,
         "creating worker " << get_name());

    /* create message queue if needed */
    if (need_queue) {
        queue = new MessageQueue(name);
    } else {
        queue = NULL;
    }

    running = false;
}

/**
 * Worker destructor
 */
Worker::~Worker (void)
{
    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_WORKER,
         "destroying worker " << get_name());

    /* cleanup the message queue if there is one */
    if (NULL != queue) {
        delete queue;
    }
}

/**
 * Return the name of the worker
 */
std::string
Worker::get_name (void) const
{
    return name;
}

/**
 * Return a pointer to this worker's message queue
 */
MessageQueue*
Worker::get_queue (void) const
{
    return queue;
}

/**
 * Start the worker thread
 */
void
Worker::run (void)
{
    if (!running) {
        if (pthread_create(&thrd, 0, thrd_func, this) != 0) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_WORKER,
                 "unable to start worker thread for " << get_name());
            throw RC_WORKER_THREAD_ERROR;
        }

        pthread_setname_np(thrd, name.c_str());
        running = true;
    }
}

/**
 * Stop the worker thread
 */
void
Worker::terminate (void)
{
    if (running) {
        if (NULL != queue) {
            queue->push_msg(MESSAGE_TERMINATE);
        } else {
            pthread_cancel(thrd);
        }
        pthread_join(thrd, NULL);
        running = false;
    }
}

/**
 * Thread init function, needed by pthread library
 */
void*
Worker::thrd_func (void *obj)
{
    /*
     * Make the thread cancellable, so that it can be killed even if it does not
     * have a message queue
     */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    reinterpret_cast<Worker*>(obj)->loop();
    pthread_exit(NULL);
}

/**
 * Dummy main thread loop
 */
void
Worker::loop (void)
{
}

} /* namespace sentry */
