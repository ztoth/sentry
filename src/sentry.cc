/*
 *------------------------------------------------------------------------------
 *
 * sentry.cc
 *
 * Home sentry robot main file
 *
 * Currently the following (optional) command line args are supported:
 *   -v                verbose debug level
 *   -vv               very verbose debug level
 *   -c <configfile>   use the given config file (default is cfg/default.cfg)
 *   -l <logfile>      redirect std::cout to the given file
 *   -s                log messages to syslog
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
#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <pthread.h>

#include "engine.h"
#include "message.h"
#include "framework.h"

/**
 * Signal handler thread
 */
void*
signal_thread (void *arg)
{
    sentry::MessageQueue* const engine_queue =
        reinterpret_cast<sentry::MessageQueue*>(arg);
    sigset_t sigset;
    int signal;

    pthread_setname_np(pthread_self(), "signal handler");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    sigfillset(&sigset);

    /* wait for asynchronous OS signals */
    while (true) {
        if (sigwait(&sigset, &signal) != 0) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
                 "sigwait() returned error");
            break;
        }

        /* catch CTRL-C */
        if (SIGINT == signal) {
            dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
                 "SIGINT received, exiting");
            engine_queue->push_msg(MESSAGE_TERMINATE);
            break;
        }
    }

    pthread_exit(NULL);
}

/**
 * Main function
 */
int
main (int argc, char *argv[])
{
    /* GPL notice */
    std::cout << "Sentry home monitoring robot - server program" << std::endl;
    std::cout << "Copyright (c) 2017 Zoltan Toth <ztoth AT thetothfamily DOT net>" << std::endl;
    std::cout << "This program comes with ABSOLUTELY NO WARRANTY; This is free software," << std::endl;
    std::cout << "and you are welcome to redistribute it under certain conditions;" << std::endl;
    std::cout << "Please refer to COPYING for details." << std::endl << std::endl;

    /* init local and global variables */
    std::streambuf *cout = std::cout.rdbuf();
    std::ofstream logfile;
    std::string logfile_name;
    bool log_to_file = false;

    /* process command line arguments */
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        /* debug level */
        if ("-v" == arg) {
            framework::debug_level = DEBUG_LEVEL_VERBOSE;
        } else if ("-vv" == arg) {
            framework::debug_level = DEBUG_LEVEL_VERY_VERBOSE;
        }

        /* configuration file */
        if ("-c" == arg) {
            framework::config_file = argv[++i];
        }

        /* check if we need to log to file */
        if ("-l" == arg) {
            logfile_name = std::string(argv[++i]);
            log_to_file = true;
        }

        /* check if we need to log to syslog */
        if ("-s" == arg) {
            framework::log_to_syslog = true;
        }
    }

    if (framework::log_to_syslog) {
        /* don't log to file if syslog is enabled */
        log_to_file = false;
        openlog(framework::project_name.c_str(), 0, 0);
    }

    if (log_to_file) {
        /* redirect cout to file */
        logfile.open(logfile_name);
        if (logfile.std::ios::fail()) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
                 "could not open logfile" << logfile_name);
            return RC_MAIN_LOGFILE_ERROR;
        }
        std::cout.rdbuf(logfile.rdbuf());
    }

    if (DEBUG_LEVEL_VERBOSE == framework::debug_level) {
        dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
             "verbose mode enabled");
    } else if (DEBUG_LEVEL_VERY_VERBOSE == framework::debug_level) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_FRAMEWORK,
             "very verbose mode enabled!");
    }
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
         "using config file " << framework::config_file);

    /* set the name of the main thread */
    pthread_setname_np(pthread_self(), framework::project_name.c_str());

    /* block every signal in the main and its child threads */
    sigset_t sigset;
    sigfillset(&sigset);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
             "unable to set sigmask");
        return RC_MAIN_SIGNAL_ERROR;
    }

    /* create the engine object */
    sentry::Engine *engine = new sentry::Engine();

    /* spawn a signal handler thread to catch asynchronous signals from the OS */
    pthread_t signal_thrd;
    if (pthread_create(&signal_thrd, 0, signal_thread,
                       engine->get_engine_queue()) != 0) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
             "unable to start signal handler thread");
        return RC_MAIN_SIGNAL_ERROR;
    }

    /* loop in the sentry message processing function */
    return_code_en rc = engine->start();

    /* cleanup */
    pthread_cancel(signal_thrd);
    pthread_join(signal_thrd, NULL);
    delete engine;

    /* close logfile if we used one */
    if (logfile.is_open()) {
        std::cout.rdbuf(cout);
        logfile.close();
    }

    return rc;
}
