/*
 *------------------------------------------------------------------------------
 *
 * camera.cc
 *
 * Camera handler implementation
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
#include "camera.h"

namespace sentry {

/**
 * Camera constructor
 */
Camera::Camera (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CAMERA,
         "initializing camera");

    /* read configuration */
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CAMERA,
         "parsing file " << framework::config_file << " for camera config");
    config = new framework::Config("camera");

    /* initialize members */
    pthread_mutex_init(&mutex, NULL);
    clients.clear();
    frame_params.clear();
    frame_params.push_back(CV_IMWRITE_JPEG_QUALITY);
    frame_params.push_back(config->get_int("quality"));

    /* configure the camera */
    device = new raspicam::RaspiCam_Cv;
    device->set(CV_CAP_PROP_FRAME_WIDTH, config->get_int("cols"));
    device->set(CV_CAP_PROP_FRAME_HEIGHT, config->get_int("rows"));
    device->set(CV_CAP_PROP_FORMAT, CV_8UC3);
    device->set(CV_CAP_PROP_BRIGHTNESS, 50);
    device->set(CV_CAP_PROP_CONTRAST, 50);
    device->set(CV_CAP_PROP_SATURATION, 50);
    device->set(CV_CAP_PROP_GAIN, 50);
}

/**
 * Camera destructor
 */
Camera::~Camera (void)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CAMERA,
         "destroying camera");

    /* let go of the camera */
    pthread_mutex_lock(&mutex);
    clients.clear();
    if (device->isOpened()) {
        device->release();
    }
    pthread_mutex_unlock(&mutex);

    /* cleanup members */
    pthread_mutex_destroy(&mutex);
    delete config;
    delete device;
}

/**
 * Reserve camera
 *
 * Open the camera if it's not yet done, and save the client's ID, so that
 * camera remembers who has access to it. We assume the same client does not
 * call open() multiple times, without calling release() first, but it's not
 * enforced in the camera object.
 */
void
Camera::reserve (const int client_id)
{
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CAMERA,
         "client (" << client_id << ") requesting camera stream");

    pthread_mutex_lock(&mutex);
    if (!device->isOpened()) {
        device->open();
        if (!device->isOpened()) {
            dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_CAMERA,
                 "unable to open camera");
            pthread_mutex_unlock(&mutex);
            return;
        }
    }
    clients.push_back(client_id);
    pthread_mutex_unlock(&mutex);
}

/**
 * Release camera
 *
 * Client no longer wants to stream camera frames, thus we remove the client ID.
 * The camera is stopped if there are no more clients.
 */
void
Camera::release (const int client_id)
{
    pthread_mutex_lock(&mutex);
    for (std::vector<int>::iterator i = clients.begin(); i != clients.end(); i++) {
        if (client_id == *i) {
            dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CAMERA,
                 "client (" << client_id << ") released camera");
            clients.erase(i);
            if (!clients.size()) {
                dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_CAMERA,
                     "no more clients");
                device->release();
            }
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * Capture a frame and encode it into a JPEG image
 */
void
Camera::get_image (std::vector<unsigned char> &buf)
{
    if (!device->isOpened()) {
        return;
    }

    /* capture a frame and encode it into JPEG image */
    cv::Mat frame = cv::Mat::zeros(config->get_int("rows"),
                                   config->get_int("cols"), CV_8UC3);
    pthread_mutex_lock(&mutex);
    device->grab();
    device->retrieve(frame);
    if (frame.rows > 0 && frame.cols > 0) {
        cv::imencode(".jpg", frame, buf, frame_params);
    }
    pthread_mutex_unlock(&mutex);

    dbug(DEBUG_LEVEL_VERY_VERBOSE, DEBUG_TYPE_CAMERA,
         "captured frame, size " << buf.size() << " bytes");
}

/**
 * Number of cols (i.e. width)
 */
int
Camera::get_cols (void) const
{
    return config->get_int("cols");
}

/**
 * Number of rows (i.e. height)
 */
int
Camera::get_rows (void) const
{
    return config->get_int("rows");
}

} /* namespace sentry */
