/*
 *------------------------------------------------------------------------------
 *
 * camera.h
 *
 * Camera handler class declaration
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
#ifndef CAMERA_H_
#define CAMERA_H_

#include <pthread.h>
#include <vector>
#include <raspicam/raspicam_cv.h>

#include "framework.h"

namespace sentry {

/**
 * Camera class
 */
class Camera {
  public:
    /** camera constructor */
    Camera (void);

    /** camera destructor */
    virtual ~Camera (void);

    /** reserve camera */
    void reserve (const int client_id);

    /** release camera */
    void release (const int client_id);

    /** capture a frame and encode it into a JPEG image */
    void get_image (std::vector<unsigned char> &buf);

    /** number of cols (i.e. width) */
    int get_cols (void) const;

    /** number of rows (i.e. height) */
    int get_rows (void) const;

  private:
    framework::Config *config;     /** camera configuration */
    raspicam::RaspiCam_Cv *device; /** camera device */
    pthread_mutex_t mutex;         /** mutex to protect access to camera */
    std::vector<int> frame_params; /** frame parameters */
    std::vector<int> clients;      /** list of clients using the camera */

    /** camera thread */
    static void* camera_thread (void *args);
};

} /* namespace sentry */

#endif /* CAMERA_H_ */
