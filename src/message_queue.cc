/*
 *------------------------------------------------------------------------------
 *
 * message_queue.cc
 *
 * Message infrastructure implementation for the worker threads
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
#include "message_queue.h"
#include "framework.h"

namespace sentry {

/**
 * Message queue infrastructure constructor
 */
MessageQueue::MessageQueue (const std::string name)
{
    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_MESSAGEQUEUE,
         "initializing message queue infrastructure for " << name);

    /* initialize members */
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cv, NULL);
    more = false;
}

/**
 * Message queue infrastructure destructor
 */
MessageQueue::~MessageQueue (void)
{
    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_MESSAGEQUEUE,
         "destroying message queue infrastructure");

    /* cleanup the remaining messages in the queue */
    pthread_mutex_lock(&mutex);
    while (!msgs.empty()) {
        message_st *msg = reinterpret_cast<message_st*>(msgs.front());
        msgs.pop();
        if (NULL != msg) {
            delete msg;
        }
    }
    pthread_mutex_unlock(&mutex);

    /* cleanup members */
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cv);
}

/**
 * Enqueue a message into the queue
 */
void
MessageQueue::push_msg (void *msg)
{
    pthread_mutex_lock(&mutex);
    msgs.push(reinterpret_cast<message_st*>(msg));
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mutex);
}

/**
 * Create a new message and put it in the message queue
 */
void
MessageQueue::push_msg (const message_type_en type)
{
    message_st *msg = new message_st;
    msg->type = type;
    push_msg(msg);
}

/**
 * Dequeue the next message from the queue
 */
message_st*
MessageQueue::pop_msg (void)
{
    message_st *msg = NULL;

    pthread_mutex_lock(&mutex);
    if (!msgs.empty()) {
        msg = msgs.front();
        msgs.pop();
        if (!msgs.empty()) {
            more = true;
        } else {
            more = false;
        }
    }
    pthread_mutex_unlock(&mutex);

    return msg;
}

/**
 * Go to sleep if there are no messages waiting to be processed
 */
void
MessageQueue::wait_msg (void)
{
    if (more) {
        return;
    }

    pthread_mutex_lock(&mutex);
    while (msgs.empty()) {
        pthread_cond_wait(&cv, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}

} /* namespace sentry */
