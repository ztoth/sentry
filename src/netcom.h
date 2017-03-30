/*
 *------------------------------------------------------------------------------
 *
 * netcom.h
 *
 * Network communication server and netcom uplink class declaration
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
#ifndef NETCOM_H_
#define NETCOM_H_

#include <string>
#include <map>
#include <openssl/ssl.h>

#include "camera.h"
#include "worker.h"
#include "message.h"
#include "message_queue.h"
#include "framework.h"

namespace sentry {

/** netcom uplink data */
typedef struct netcom_uplink {
    int id;                            /** client ID */
    int sd;                            /** uplink stream socket */
    struct sockaddr_storage addr;      /** client's address info */
    unsigned char key[max_buf_size];   /** client specific key */
    std::string name;                  /** uplink client name */
} netcom_uplink_st;

/** netcom client specific data */
typedef struct netcom_client {
    int sd;                            /** control socket descriptor */
    SSL *ssl;                          /** SSL context of the client */
    unsigned char otp[max_buf_size];   /** password used during connection init */
    netcom_uplink_st *uplink;          /** pointer to the uplink object */
    std::string name;                  /** client name */
} netcom_client_st;

/**
 * Netcom server class
 */
class Netcom : public Worker {
  public:
    /** netcom server constructor */
    Netcom (MessageQueue* const engine_queue);

    /** netcom server destructor */
    virtual ~Netcom (void);

  private:
    /** netcom socket types */
    typedef enum netcom_sockets {
        NETCOM_SOCKET_INVALID = -1,
        NETCOM_SOCKET_STREAM  = 0,
        NETCOM_SOCKET_DGRAM,
        NETCOM_SOCKET_MAX
    } netcom_sockets_en;

    framework::Config *config;                /** netcom server config */
    MessageQueue* const engine_queue;         /** main message queue */
    SSL_CTX *ssl_ctx;                         /** SSL context */
    int server_socket[NETCOM_SOCKET_MAX];     /** server sockets */
    std::map<int, netcom_client_st*> clients; /** sd => client map */

    /** main thread loop */
    void loop (void);

    /** initialize SSL context */
    void init_ssl (void);

    /** initialize server socket */
    void init_server_socket (const netcom_sockets_en type);

    /** create new client */
    netcom_client_st* create_client (void) const;

    /** connect uplink socket */
    void connect_uplink (struct sockaddr_storage &client_addr, char *buf, int length);

    /** process control message from client */
    void proc_control_message (const netcom_client_st *client, char *buf, int length);
};

/**
 * NetcomUplink class
 */
class NetcomUplink : public Worker {
  public:
    /** netcom uplink constructor */
    NetcomUplink (MessageQueue* const engine_queue, netcom_uplink_st *client,
                  Camera *camera);

    /** netcom uplink destructor */
    virtual ~NetcomUplink (void);

  private:
    MessageQueue* const engine_queue;   /** main message queue */
    netcom_uplink_st *client;           /** client object from the netcom server */
    Camera *camera;                     /** pointer to the camera object */

    /** main thread loop */
    void loop (void);

    /** grab a camera frame and stream it to the client */
    void upload_frame (void) const;

    /** upload sensor data to the client */
    void upload_sensor (message_st *msg) const;
};

} /* namespace sentry */

#endif /* NETCOM_H_ */
