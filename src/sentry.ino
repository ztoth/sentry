/*
 *------------------------------------------------------------------------------
 *
 * sentry.ino
 *
 * Arduino code for the home sentry robot
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
#include <Wire.h>
#include <Servo.h>
#include <Adafruit_MotorShield.h>
#include "message.h"

/**
 * Motor shield and wheel handlers
 * (the default I2C address of the motor shield is 0x60)
 */
Adafruit_MotorShield motor_shield = Adafruit_MotorShield();
Adafruit_DCMotor *rear_right = motor_shield.getMotor(1);
Adafruit_DCMotor *front_right = motor_shield.getMotor(2);
Adafruit_DCMotor *front_left = motor_shield.getMotor(3);
Adafruit_DCMotor *rear_left = motor_shield.getMotor(4);

/** motor related variables */
const int max_speed = 140;
int left_wheels = RELEASE;
int right_wheels = RELEASE;
int speed = 0;

/** current moving direction */
move_direction move_state = STOP;

/** camera servo handler and related variables */
Servo camera_servo;
const int camera_servo_pin = 10;
const int camera_max_angle = 105;
const int camera_min_angle = 30;
const int camera_rotation_step = 15;
int camera_angle = camera_max_angle;

/** distance sensor pins and data (period [ms], threshold [cm]) */
const int distance_trig_pin = 2;
const int distance_echo_pin = 3;
const int distance_period = 250;
const int distance_threshold = 25;
long distance = 9999;
long last_distance_data = 0;

/** temperature sensor info (temperature is measured in Fahrenheit) */
const int temperature_sensor_address = 0x48;
const int temperature_period = 300000;
const int temperature_threshold = 5;
float temperature = 9999;
long last_temperature_data = 0;

/** heartbeat monitor and threshold (in milliseconds) */
const int heartbeat_threshold = 1200;
long last_heartbeat = 0;

/** serial command buffer */
char cmd[16] = {0};
boolean cmd_ready = false;

/**
 * Arduino init function
 */
void
setup (void)
{
    /* initialize motor shield */
    motor_shield.begin();

    /* initialize camera servo */
    camera_servo.attach(camera_servo_pin);
    camera_servo.write(camera_angle);

    /* initialize distance sensor */
    pinMode(distance_trig_pin, OUTPUT);
    pinMode(distance_echo_pin, INPUT);
    digitalWrite(distance_trig_pin, HIGH);

    /* initialize wire object */
    Wire.begin();

    /* start serial port and send delimiter character */
    Serial.begin(9600);
    Serial.write(":");
}

/**
 * Serial read callback
 */
void
serialEvent (void)
{
    /* read data from serial one byte at a time, until the delimiter is recvd */
    while (Serial.available()) {
        char c;
        int read_bytes = 0;
        do {
            Serial.readBytes(&c, 1);
            cmd[read_bytes++] = c;
        } while (c != ':');
        Serial.flush();
        cmd_ready = true;
    }
}

/**
 * Set the speed and rotation of the wheels
 */
static void
update_wheels (int direction)
{
    /*
     * Special case if robot is already moving/rotating, but in a different
     * direction than what is requested. We need to stop first and rest a bit,
     * otherwise the high power consumption can kill other components, such as
     * the wifi in the raspberry pi.
     */
    if ((move_state != STOP) && (direction != STOP) &&
        (move_state != direction)) {
        front_right->setSpeed(0);
        front_left->setSpeed(0);
        rear_left->setSpeed(0);
        rear_right->setSpeed(0);
        front_right->run(RELEASE);
        front_left->run(RELEASE);
        rear_left->run(RELEASE);
        rear_right->run(RELEASE);
        delay(500);
    }

    switch (direction) {
    case MOVE_FORWARD: {
        /* forward movement command */
        if (MOVE_FORWARD == move_state) {
            move_state = STOP;
            left_wheels = right_wheels = RELEASE;
            speed = 0;
        } else {
            move_state = MOVE_FORWARD;
            left_wheels = right_wheels = BACKWARD;
            speed = max_speed;
        }
        break;
    }

    case MOVE_BACKWARD: {
        /* backward movement command */
        if (MOVE_BACKWARD == move_state) {
            move_state = STOP;
            left_wheels = right_wheels = RELEASE;
            speed = 0;
        } else {
            move_state = MOVE_BACKWARD;
            left_wheels = right_wheels = FORWARD;
            speed = max_speed;
        }
        break;
    }

    case TURN_LEFT: {
        /* turn left */
        if (TURN_LEFT == move_state) {
            move_state = STOP;
            left_wheels = right_wheels = RELEASE;
            speed = 0;
        } else {
            move_state = TURN_LEFT;
            left_wheels = FORWARD;
            right_wheels = BACKWARD;
            speed = max_speed;
        }
        break;
    }

    case TURN_RIGHT: {
        /* turn right */
        if (TURN_RIGHT == move_state) {
            move_state = STOP;
            left_wheels = right_wheels = RELEASE;
            speed = 0;
        } else {
            move_state = TURN_RIGHT;
            left_wheels = BACKWARD;
            right_wheels = FORWARD;
            speed = max_speed;
        }
        break;
    }

    case STOP:
    default:
        /* for safety reasons, anything else will make us stop */
        move_state = STOP;
        left_wheels = right_wheels = RELEASE;
        speed = 0;
        break;
    }

    /* set the speed of the wheels */
    front_right->setSpeed(speed);
    front_left->setSpeed(speed);
    rear_left->setSpeed(speed);
    rear_right->setSpeed(speed);

    /* set the rotation direction of the wheels */
    front_right->run(right_wheels);
    front_left->run(left_wheels);
    rear_left->run(left_wheels);
    rear_right->run(right_wheels);
}

/**
 * Rotate the camera by turning the servo in the given direction
 */
static void
rotate_camera (int direction)
{
    if (ROTATE_DOWN == direction) {
        if (camera_angle < camera_max_angle) {
            camera_angle += camera_rotation_step;
        }
    } else if (ROTATE_UP == direction) {
        if (camera_angle > camera_min_angle) {
            camera_angle -= camera_rotation_step;
        }
    }
    camera_servo.write(camera_angle);
}

/**
 * Update distance and send data to sentry
 */
static void
update_distance (void)
{
    /* read the current distance value from the sensor */
    digitalWrite(distance_trig_pin, LOW);
    distance = pulseIn(distance_echo_pin, HIGH) / 29 / 2;
    digitalWrite(distance_trig_pin, HIGH);

    /* send it to the sentry */
    message_sensor_st *msg = new message_sensor_st;
    msg->type = MESSAGE_SENSOR_DATA;
    msg->sensor = SENSOR_DISTANCE;
    msg->data = distance;
    Serial.write((uint8_t*)msg, sizeof(*msg));
    Serial.write(":");
}

/**
 * Measure current temperature, and send data to sentry if either the difference
 * from the previously sent data hits the threshold, or the data was requested
 * by the sentry
 */
static void
update_temperature (bool send_data)
{
    Wire.requestFrom(temperature_sensor_address, 2);
    byte MSB = Wire.read();
    byte LSB = Wire.read();
    float curr_temp = (((MSB << 8) | LSB) >> 4) * 0.09378 + 32;

    if (send_data || (abs(temperature - curr_temp) >= temperature_threshold)) {
        temperature = curr_temp;

        /* send it to the sentry */
        message_sensor_st *msg = new message_sensor_st;
        msg->type = MESSAGE_SENSOR_DATA;
        msg->sensor = SENSOR_TEMPERATURE;
        msg->data = temperature;
        Serial.write((uint8_t*)msg, sizeof(*msg));
        Serial.write(":");
    }
}

/**
 * Arduino main loop
 */
void
loop (void)
{
    /* get current time */
    long now = millis();

    /* check if we have a command ready to process */
    if (cmd_ready) {
        cmd_ready = false;

        /*
         * We expect commands of the following types:
         *   - heartbeat, update the heartbeat monitor timer
         *   - sensor request, send distance and temperature data
         *   - movement command, rock'n roll baby
         */
        message_st *msg = (message_st*)cmd;
        switch (msg->type) {
          case MESSAGE_HEARTBEAT: {
              /* heartbeat received, update monitor */
              last_heartbeat = now;
              break;
          }

          case MESSAGE_SENSOR_REQUEST: {
              /* sentry is requesting sensor data */
              update_distance();
              update_temperature(true);
              last_distance_data = now;
              last_temperature_data = now;
              break;
          }

          case MESSAGE_MOVE: {
              /* movement or camera rotation command */
              int direction = ((message_move_st*)msg)->direction;
              if ((ROTATE_UP == direction) || (ROTATE_DOWN == direction)) {
                  /* camera rotation command */
                  rotate_camera(direction);
              } else {
                  /* movement command, set the speed and rotation values */
                  update_wheels(direction);

                  /*
                   * The initial power consumption of the motors is high, which
                   * may affect the accuracy of the distance sensor, thus we
                   * skip a measurement by updating the time. We must also reset
                   * the distance if it was below threshold, otherwise it would
                   * never be able to move forward.
                   */
                  if (distance < distance_threshold) {
                      distance = 9999;
                  }
                  last_distance_data = now;
              }
              break;
          }

          default:
            break;
        }
    }

    /* send distance data continuously to sentry while moving */
    if (speed != 0) {
        if (now > last_distance_data + distance_period) {
            last_distance_data = now;
            update_distance();
        }

        /*
         * We need to stop moving if it's too close for comfort, or if it has
         * been a while since we got the last heartbeat from sentry
         */
        if ((now > last_heartbeat + heartbeat_threshold) ||
            ((distance < distance_threshold) &&
             (BACKWARD == left_wheels) &&
             (BACKWARD == right_wheels))) {
            update_wheels(STOP);
        }
    }

    /* measure temperature once in every 5 minutes */
    if (now > last_temperature_data + temperature_period) {
        update_temperature(false);
        last_temperature_data = now;
    }
}
