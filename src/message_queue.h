/*
 *------------------------------------------------------------------------------
 *
 * message_queue.h
 *
 * Message infrastructure class declaration for the worker threads
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
#ifndef MESSAGE_QUEUE_H_
#define MESSAGE_QUEUE_H_

#include <pthread.h>
#include <queue>
#include <string>

#include "message.h"

namespace sentry {

/**
 * MessageQueue class
 */
class MessageQueue {
  public:
    /** message queue infrastructure constructor */
    MessageQueue (const std::string name);

    /** message queue infrastructure destructor */
    virtual ~MessageQueue (void);

    /** enqueue a message into the queue */
    void push_msg (void *msg);
    void push_msg (const message_type_en type);

    /** dequeue the next message from the queue */
    message_st* pop_msg (void);

    /** go to sleep if there are no messages waiting to be processed */
    void wait_msg (void);

  private:
    pthread_mutex_t         mutex;   /** mutex to protect the queue */
    pthread_cond_t          cv;      /** queue condition variable */
    std::queue<message_st*> msgs;    /** message queue */
    bool                    more;    /** true means messages are waiting in queue */
};

} /* namespace sentry */

#endif /* MESSAGE_QUEUE_H_ */
