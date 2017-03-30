/*
 *------------------------------------------------------------------------------
 *
 * netcom.cc
 *
 * Network communication server and netcom uplink implementation
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
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include "netcom.h"

namespace sentry {

/**
 * Netcom server constructor
 */
Netcom::Netcom (MessageQueue* const engine_queue)
        : Worker("netcom server", false), engine_queue(engine_queue)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
         "initializing " << get_name());

    /* read configuration */
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
         "parsing file " << framework::config_file << " for " << get_name() <<
         " config");
    config = new framework::Config("netcom");

    /* initialize SSL context */
    init_ssl();

    /* open the server sockets */
    init_server_socket(NETCOM_SOCKET_STREAM);
    init_server_socket(NETCOM_SOCKET_DGRAM);

    /* reset client map */
    clients.clear();

    /* ready to start the worker thread */
    run();
}

/**
 * Netcom server destructor
 */
Netcom::~Netcom (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
         "destroying " << get_name());

    /* terminate the worker thread */
    terminate();

    /* close the server sockets and SSL context */
    if (server_socket[NETCOM_SOCKET_STREAM] != NETCOM_SOCKET_INVALID) {
        close(server_socket[NETCOM_SOCKET_STREAM]);
    }
    if (server_socket[NETCOM_SOCKET_DGRAM] != NETCOM_SOCKET_INVALID) {
        close(server_socket[NETCOM_SOCKET_DGRAM]);
    }
    SSL_CTX_free(ssl_ctx);

    delete config;
}

/**
 * Initialize SSL context
 */
void
Netcom::init_ssl (void)
{
    /* create the SSL context */
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ssl_ctx = SSL_CTX_new(SSLv23_server_method());
    if (NULL == ssl_ctx) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
             "failed to initialize SSL library");
        throw RC_NETCOM_SSL_ERROR;
    }

    /* load the server certificate file */
    if (SSL_CTX_use_certificate_file(ssl_ctx, config->get_string("certfile").c_str(),
                                     SSL_FILETYPE_PEM) <= 0) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
             "invalid or non-existing certificate file " <<
             config->get_string("certfile"));
        throw RC_NETCOM_INVALID_CERTIFICATE;
    }

    /* load the server private keyfile */
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, config->get_string("keyfile").c_str(),
                                    SSL_FILETYPE_PEM) <= 0) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
             "invalid or non-existing key file " << config->get_string("keyfile"));
        throw RC_NETCOM_INVALID_KEY;
    }

    /* validate server credentials */
    if (!SSL_CTX_check_private_key(ssl_ctx)) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
             "private key does not match the public certificate");
        throw RC_NETCOM_KEY_CERT_MISMATCH;
    }

    /* load signed client certificates, if needed */
    if (config->get_bool("force_auth")) {
        if (SSL_CTX_load_verify_locations(
                ssl_ctx, config->get_string("clients").c_str(), NULL) <= 0) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
                 "unable to load file with signed client certificates " <<
                 config->get_string("clients"));
            throw RC_NETCOM_CLIENT_CA_ERR;
        }

        STACK_OF(X509_NAME) *client_list;
        client_list = SSL_load_client_CA_file(config->get_string("clients").c_str());
        if (NULL == client_list) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
                 "cannot find any client certificates in file " <<
                 config->get_string("clients"));
            throw RC_NETCOM_CLIENT_CA_ERR;
        }
        SSL_CTX_set_client_CA_list(ssl_ctx, client_list);
        SSL_CTX_set_verify(ssl_ctx, (SSL_VERIFY_PEER |
                                     SSL_VERIFY_FAIL_IF_NO_PEER_CERT |
                                     SSL_VERIFY_CLIENT_ONCE), NULL);
    }
}

/**
 * Initialize server socket
 */
void
Netcom::init_server_socket (const netcom_sockets_en type)
{
    /* create socket and bind to it */
    struct addrinfo hints, *results, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    if (NETCOM_SOCKET_STREAM == type) {
        hints.ai_socktype = SOCK_STREAM;
    } else {
        hints.ai_socktype = SOCK_DGRAM;
    }
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(NULL, config->get_string("port").c_str(), &hints, &results);
    if (rc != 0) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
             "getaddrinfo() failed with " << gai_strerror(rc) <<
             " for socket type " << type);
        throw RC_NETCOM_SOCKET_ERROR;
    }

    server_socket[type] = NETCOM_SOCKET_INVALID;
    for (rp = results; rp != NULL; rp = rp->ai_next) {
        server_socket[type] = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (NETCOM_SOCKET_INVALID == server_socket[type]) {
            continue;
        }

        if (bind(server_socket[type], rp->ai_addr, rp->ai_addrlen) != -1) {
            /* success */
            break;
        } else {
            server_socket[type] = NETCOM_SOCKET_INVALID;
        }
    }
    freeaddrinfo(results);

    /* make sure socket creation was successful */
    if (NETCOM_SOCKET_INVALID == server_socket[type]) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
             "unable to open server socket type " << type);
        throw RC_NETCOM_SOCKET_ERROR;
    }

    /* set up listening for the stream socket */
    if (NETCOM_SOCKET_STREAM == type) {
        if (listen(server_socket[type], 3) != 0) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
                 "listen() failed with " << strerror(errno));
            throw RC_NETCOM_SOCKET_ERROR;
        }
    }

    /* set read and write timeout for the socket */
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(server_socket[type], SOL_SOCKET, SO_RCVTIMEO,
                   (char*)&timeout, sizeof(timeout)) < 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "setsockopt() for receive timeout failed with " << strerror(errno) <<
             " for socket type " << type);
    }
    if (setsockopt(server_socket[type], SOL_SOCKET, SO_SNDTIMEO,
                   (char*)&timeout, sizeof(timeout)) < 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "setsockopt() for send timeout failed with " << strerror(errno) <<
             " for socket type " << type);
    }
}

/**
 * Create new client
 *
 * There are a few tasks to be done when a new client arrives:
 *   - establish connection with the client using SSL
 *   - create netcom client and uplink objects for storing client information
 *   - generate random byte stream used as one time password during the datagram
 *     connection establishment
 *   - send the client ID (socket file descriptor ID) along with the one time
 *     password to the client via the SSL connection
 */
netcom_client_st*
Netcom::create_client (void) const
{
    /* accept the client connection */
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int client_sd;
    if ((client_sd = accept(server_socket[NETCOM_SOCKET_STREAM],
                            (struct sockaddr*)&client_addr, &addr_size)) < 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "accept() returned error " << strerror(errno));
        return NULL;
    }

    char client_info[256];
    char client_port[8];
    getnameinfo((struct sockaddr*)&client_addr, addr_size, client_info,
                sizeof(client_info), client_port, sizeof(client_port), 0);
    std::string client_name = std::string(client_info) + ":" + std::string(client_port);
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
         "new connection from " << client_name << " on fd " << client_sd);

    /* set read and write timeout on the client socket */
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (setsockopt(client_sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
                   sizeof(timeout)) < 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "setsockopt() failed with " << strerror(errno));
    }
    if (setsockopt(client_sd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout,
                   sizeof(timeout)) < 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "setsockopt() failed with " << strerror(errno));
    }

    /* initiate SSL connection with the new client */
    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, client_sd);
    if ((SSL_accept(ssl) != -1) &&
        (!config->get_bool("force_auth") ||
         ((SSL_get_peer_certificate(ssl) != NULL) &&
          (SSL_get_verify_result(ssl) == X509_V_OK)))) {
        /* client is trusted, display client certificates */
        X509 *cert = SSL_get_peer_certificate(ssl);
        if (NULL != cert) {
            dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
                 "client certificate issuer: " <<
                 X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0));
        } else {
            dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
                 "no client certificates available");
        }

        /* intialize client data */
        netcom_client_st *client = new netcom_client_st;
        client->name = client_name;
        client->sd = client_sd;
        client->ssl = ssl;
        RAND_bytes(client->otp, sizeof(client->otp));

        /* let the client know its credentials via the SSL socket */
        message_connect_st *msg = new message_connect_st;
        msg->type = htonl(MESSAGE_NETCOM_CONNECT);
        msg->id = htonl(client_sd);
        memcpy(msg->otp, client->otp, sizeof(client->otp));
        if (SSL_write(client->ssl, msg, sizeof(*msg)) <= 0) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_NETCOM,
                 "unable to send client credentials to client");
            SSL_free(client->ssl);
            close(client->sd);
            delete client;
            delete msg;
            return NULL;
        }
        delete msg;

        /* create the uplink counterpart */
        netcom_uplink_st *uplink = new netcom_uplink_st;
        uplink->name = client_name;
        uplink->sd = NETCOM_SOCKET_INVALID;
        client->uplink = uplink;
        return client;
    } else {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "untrusted client SSL, rejecting connection");
        SSL_free(ssl);
        close(client_sd);
    }

    return NULL;
}

/**
 * Connect uplink socket
 *
 * Camera frames and sensor data are sent to the client via datagram socket.
 * This connection must be initiated by the client, in order to bypass NAT
 * routers: client sends its credentials via datagram socket, then, after
 * verifying the credentials, server creates an uplink socket for this client
 * using its IP address and port (from where server received the credentials).
 * When ready, server hands over the client to sentry, which creates a new
 * thread for it to handle uplink traffic.
 */
void
Netcom::connect_uplink (struct sockaddr_storage &client_addr, char *buf, int length)
{
    char client_info[256];
    char client_port[8];
    getnameinfo((struct sockaddr*)&client_addr, sizeof(client_addr), client_info,
                sizeof(client_info), client_port, sizeof(client_port), 0);
    std::string client_name = std::string(client_info) + ":" + std::string(client_port);
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
         "message from " << client_name << ", length " << length);

    message_st *socket_msg = reinterpret_cast<message_st*>(buf);
    message_type_en type = static_cast<message_type_en>(ntohl(socket_msg->type));
    if (type != MESSAGE_NETCOM_CONNECT) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "unsupported socket message, type " << message_type_str(type));
        return;
    }

    /* find the client */
    message_connect_st *connect_msg = reinterpret_cast<message_connect_st*>(buf);
    int client_sd = ntohl(connect_msg->id);
    std::map<int, netcom_client_st*>::iterator it = clients.find(client_sd);
    if (it == clients.end()) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "unknown client " << client_sd);
        return;
    }

    netcom_client_st *client = it->second;
    netcom_uplink_st *uplink = client->uplink;

    /* check if this client is already connected */
    if (uplink->sd != NETCOM_SOCKET_INVALID) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "client is already connected, ignoring message");
        return;
    }

    /* make sure password is correct */
    if (memcmp(connect_msg->otp, client->otp, sizeof(client->otp)) != 0) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "password mismatch, client " << client_name << " id " << client->sd);
        return;
    }

    /* generate encryption key for this client */
    RAND_bytes(uplink->key, sizeof(uplink->key));

    /* send the client key securely */
    message_key_st *msg = new message_key_st;
    msg->type = htonl(MESSAGE_NETCOM_KEY);
    memcpy(msg->key, uplink->key, sizeof(uplink->key));
    if (SSL_write(client->ssl, msg, sizeof(*msg)) <= 0) {
        delete msg;
        return;
    }

    /* update uplink info */
    uplink->id = client->sd;
    uplink->sd = server_socket[NETCOM_SOCKET_DGRAM];
    uplink->addr = client_addr;

    /* send a message to main to notify about new client */
    message_netcom_st *netcom_msg = new message_netcom_st;
    netcom_msg->type = MESSAGE_NETCOM_CLIENT_ALIVE;
    netcom_msg->id = client->sd;
    netcom_msg->client = uplink;
    engine_queue->push_msg(netcom_msg);
}

/**
 * Process control message from client
 */
void
Netcom::proc_control_message (const netcom_client_st *client, char *buf, int length)
{
    message_st *socket_msg = reinterpret_cast<message_st*>(buf);
    message_type_en type = static_cast<message_type_en>(ntohl(socket_msg->type));

    dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_NETCOM,
         "message from " << client->name << ", type " << message_type_str(type) <<
         ", forwarding to engine");

    switch (type) {
    case MESSAGE_TERMINATE:
    case MESSAGE_SEARCH_REMOTE:
    case MESSAGE_SENSOR_REQUEST:
    case MESSAGE_HEARTBEAT: {
        engine_queue->push_msg(type);
        break;
    }

    case MESSAGE_CAMERA_REQUEST: {
        message_netcom_st *msg = new message_netcom_st;
        msg->type = MESSAGE_CAMERA_REQUEST;
        msg->id = client->sd;
        engine_queue->push_msg(msg);
        break;
    }

    case MESSAGE_MOVE: {
        message_move_st *socket_msg = reinterpret_cast<message_move_st*>(buf);
        message_move_st *msg = new message_move_st;
        msg->type = MESSAGE_MOVE;
        msg->direction = ntohl(socket_msg->direction);
        engine_queue->push_msg(msg);
        break;
    }

    default:
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
             "invalid socket message, type " << message_type_str(type));
        return;
    }
}

/**
 * Netcom server thread loop
 *
 * This thread listens to messages on all sockets, which includes the SSL
 * protected stream socket, the server datagram socket, and the already
 * established client sockets. Most of these messages are simply forwarded
 * to sentry for processing, except for the connection establishment (and
 * related) messages, which are processed by the netcom server.
 */
void
Netcom::loop (void)
{
    fd_set read_fds, tmp_fds;
    int max_fd;
    char buf[2048];

    /* clear the file descriptor sets */
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    /* add the server sockets to the read file descriptor set */
    FD_SET(server_socket[NETCOM_SOCKET_STREAM], &read_fds);
    FD_SET(server_socket[NETCOM_SOCKET_DGRAM], &read_fds);

    /* keep track of the biggest file descriptor */
    max_fd = server_socket[NETCOM_SOCKET_DGRAM];
    if (server_socket[NETCOM_SOCKET_STREAM] > max_fd) {
        max_fd = server_socket[NETCOM_SOCKET_STREAM];
    }

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
         "starting " << get_name() << " loop");

    while (true) {
        tmp_fds = read_fds;
        if (select(max_fd + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
            dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
                 "select() returned error " << strerror(errno));
        }

        /* look for incoming messages */
        for (int i = 0; i <= max_fd; i++) {
            if (!FD_ISSET(i, &tmp_fds)) {
                continue;
            }

            if (i == server_socket[NETCOM_SOCKET_STREAM]) {
                /* new connection */
                netcom_client_st *client = create_client();
                if (NULL == client) {
                    continue;
                }

                /* store the new file descriptor */
                FD_SET(client->sd, &read_fds);
                if (client->sd > max_fd) {
                    max_fd = client->sd;
                }

                /* store the new client data */
                clients.insert(std::pair<int, netcom_client_st*>(client->sd, client));
            } else if (i == server_socket[NETCOM_SOCKET_DGRAM]) {
                /* message on the datagram socket, must be uplink connection request */
                struct sockaddr_storage client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int length = recvfrom(server_socket[NETCOM_SOCKET_DGRAM], buf,
                                      sizeof(buf), 0, (struct sockaddr*)&client_addr,
                                      &addr_size);
                if (length > 0) {
                    connect_uplink(client_addr, buf, length);
                }
            } else {
                /* message from an existing client */
                std::map<int, netcom_client_st*>::iterator it = clients.find(i);
                if (it == clients.end()) {
                    continue;
                }

                netcom_client_st *client = it->second;
                int length = SSL_read(client->ssl, buf, sizeof(buf));
                if (length > 0) {
                    proc_control_message(client, buf, length);
                } else {
                    if (0 == length) {
                        /* connection closed by client */
                        dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
                             "client " << client->name << " hung up");
                    } else {
                        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_NETCOM,
                             "garbage received from client " <<
                             client->name << ", closing socket");
                    }

                    /* inform main thread about the dead client */
                    message_netcom_st *msg = new message_netcom_st;
                    msg->type = MESSAGE_NETCOM_CLIENT_DEAD;
                    msg->id = client->sd;
                    engine_queue->push_msg(msg);

                    /* close the SSL connection and socket */
                    FD_CLR(client->sd, &read_fds);
                    SSL_free(client->ssl);
                    close(client->sd);
                    clients.erase(it);
                    delete client;
                }
            }
        }
    }

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM,
         get_name() << " exiting");
}

/**
 * Netcom uplink constructor
 */
NetcomUplink::NetcomUplink (MessageQueue* const engine_queue,
                            netcom_uplink_st *client, Camera *camera)
        : Worker(client->name, true), engine_queue(engine_queue), client(client),
          camera(camera)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM_UPLINK,
         "initializing netcom client " << get_name());

    /* ready to start the worker thread */
    run();
}

/**
 * Netcom uplink destructor
 */
NetcomUplink::~NetcomUplink (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM_UPLINK,
         "destroying netcom client " << get_name());

    /* terminate the worker thread */
    terminate();

    /* make sure to release camera if it was used */
    camera->release(client->id);

    /* delete the uplink data */
    delete client;
}

/**
 * Grab a camera frame and stream it to the client
 *
 * The frame is encrypted with the client-specific key, and sent in small
 * chunks to avoid IP level fragmentation, as well as to minimize lost
 * information when there is a packet loss.
 */
void
NetcomUplink::upload_frame (void) const
{
    static std::vector<unsigned char> buf;

    /* capture an image from the camera */
    camera->get_image(buf);

    /* encrypt frame with client specific key */
    int k = 0;
    for (unsigned int i = 0; i < buf.size(); i++) {
        buf[i] ^= client->key[k++];
        if (k >= max_buf_size) {
            k = 0;
        }
    }

    dbug(DEBUG_LEVEL_VERY_VERBOSE, DEBUG_TYPE_NETCOM_UPLINK,
         "sending frame (" << buf.size() << " bytes) to client " << get_name());

    /* send the message, fragment if necessary */
    message_frame_st *msg = new message_frame_st;
    msg->type = htonl(MESSAGE_CAMERA_FRAME);
    msg->frame_size = htonl(buf.size());
    msg->cols = htons(camera->get_cols());
    msg->rows = htons(camera->get_rows());

    int rem_size = buf.size();
    int sent_bytes = 0;
    int frag_seq = 1;
    do {
        /* prepare current fragment */
        int frag_size = rem_size;
        if (frag_size > max_buf_size) {
            frag_size = max_buf_size;
        }
        msg->frag_size = htons(frag_size);
        msg->frag_seq = htons(frag_seq);
        memcpy(msg->frame, &buf[0] + sent_bytes, frag_size);

        /* ship it */
        sendto(client->sd, msg, sizeof(*msg) - max_buf_size + frag_size,
               0, (struct sockaddr*)&client->addr, sizeof(client->addr));

        /* readjust counters */
        rem_size -= frag_size;
        sent_bytes += frag_size;
        frag_seq++;
    } while (rem_size > 0);

    /* cleanup */
    delete msg;
}

/**
 * Upload sensor data to the client
 */
void
NetcomUplink::upload_sensor (message_st *msg) const
{
    message_sensor_st *sensor_msg = reinterpret_cast<message_sensor_st*>(msg);

    dbug(DEBUG_LEVEL_VERY_VERBOSE, DEBUG_TYPE_NETCOM_UPLINK,
         "sending message " << message_print(msg) << " to client " << get_name());

    sensor_msg->type = htonl(sensor_msg->type);
    sensor_msg->sensor = htons(sensor_msg->sensor);
    sensor_msg->data = htons(sensor_msg->data);

    sendto(client->sd, sensor_msg, sizeof(*sensor_msg), 0,
           (struct sockaddr*)&client->addr, sizeof(client->addr));
}

/**
 * Netcom client uplink thread loop
 *
 * This thread's job is to send messages to the corresponding netcom client
 * via it's datagram socket, such as camera frames and sensor data.
 */
void
NetcomUplink::loop (void)
{
    message_st *msg;
    bool loop = true;
    bool stream = false;

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM_UPLINK,
         "starting netcom client " << get_name() << " loop");

    while (loop) {
        /* go to sleep if there's nothing to do */
        if (!stream) {
            get_queue()->wait_msg();
        } else {
            upload_frame();
        }

        /* process message */
        if (NULL != (msg = get_queue()->pop_msg())) {
            dbug(DEBUG_LEVEL_VERBOSE, DEBUG_TYPE_NETCOM_UPLINK,
                 "netcom client " << get_name() << " message " <<
                 message_print(msg));

            switch (msg->type) {
            case MESSAGE_CAMERA_REQUEST: {
                if (!stream) {
                    camera->reserve(client->id);
                } else {
                    camera->release(client->id);
                }
                stream = !stream;
                break;
            }

            case MESSAGE_SENSOR_DATA: {
                upload_sensor(msg);
                break;
            }

            case MESSAGE_TERMINATE: {
                if (stream) {
                    stream = false;
                    camera->release(client->id);
                }
                loop = false;
                break;
            }

            default:
                break;
            }

            /* free the message */
            delete msg;
        }
    }

    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_NETCOM_UPLINK,
         "netcom client " << get_name() << " terminating");
}

} /* namespace sentry */
