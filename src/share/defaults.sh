#!/bin/bash
# -*- c-basic-offset: 4; indent-tabs-mode: nil -*-

## Copyright 2018-19, Lancaster University
## All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
## 
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the
##    distribution.
## 
##  * Neither the name of the copyright holder nor the names of
##    its contributors may be used to endorse or promote products derived
##    from this software without specific prior written permission.
## 
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
##
## Author: Steven Simpson <https://github.com/simpsonst>


####### Capture

## /... => Video4Linux device to capture from; otherwise, URL of
## multipart/x-mixed-replace of image/jpeg to fetch from
DEVICE=/dev/video0

## Capture rate for Video4Linux devices; no longer ignored for
## HTTP/RTSP; must be an integer
RATE=10

## The capture dimensions for Video4Linux devices
CAPDIMS=640x480

## Ephemeral storage for captured frames - Each camera configuration
## will watch this with inotify, so nothing else may write into it,
## i.e., you'll need a different one for each simultaneously running
## configuration.
CAPDIR=/var/run/stecam/capture



####### Correction

## Ephemeral storage for corrected frames - Rotated frames are moved
## in here and renamed by timestamp.  The CGI program that serves live
## video monitors this with inotify, so nothing else may write into
## it, i.e., you'll need a different one for each simultaneously
## running configuration.
WORKDIR=/var/run/stecam/work


## Set this if you need to scale down the live video (because of
## limited upstream bandwidth, for example).
#LIVEDIMS=320x240


## Rotate the captured image by 90, 180 or 270 degrees before any
## other processing.
#ROTATION



####### Detection

## Frame rate for motion detection; may be a ratio, e.g., 10/3 for 3⅓
## frames per second
#DETRATE="$RATE"

## The frame dimensions for detection - Higher resolutions will detect
## smaller movements.  Unset to turn off detection.
DETDIMS=16x12

## The motion score that triggers recording; either a single integer,
## or a pair (100-200), the larger of which triggers the start of
## recording when exceeded, and the smaller triggers the end when it
## exceeds the score
THRESHOLD=400

## Number of historical detection frames to merge together to form an
## average (to be compared to the average of the next $GATHER frames) -
## Increase this to desensitize motion detection to waving branches.
MERGE=50

## Number of recent detection frames to average together for
## comparison with the previous $MERGE frames
GATHER=1

## Number of above-upper-threshold detection frames to wait before
## starting recording - A counter increases by one while the threshold
## is exceeded, and decreases by one otherwise.  If the counter
## reaches this value, recording starts.
HESITATE=4

## Log of scale factor for score - This exposes the fractional part.
## Increase by one to multiply score by 10, if the score is not
## showing any change in response to motion.
POWER=9

## Additional frames before motion to record
LEADER=5

## Ephemeral storage for condensed frames for detection and modified
## frames for recording - The same file-naming scheme is used for all
## camera configurations, i.e., you'll need a different one for each
## simultaneously running configuration.
DETDIR=/var/run/stecam/detect

## Directory for depositing video recordings of detected motion
MOVDIR=/var/spool/stecam

## Number of below-lower-threshold detection frames to wait before
## stopping recording - At the start of recording, the counter is set
## to this value.  If the score drops below the threshold, it
## decrements; otherwise, it is reset.  If it reaches zero, recording
## stops.  Note that this value is superceded by (MERGE+HESITATE) if
## that is larger; in that case, LINGER is merely the number of excess
## frames to record.
LINGER=10

## The exponent to convert the variance of the old frames' cells into
## their standard deviation - By using a value greater than 0.5, the
## 'SD' comes out higher, and reduces the significance of cells where
## a lot of variation has historically taken place.
VAREXP=0.5

####### Recordings

## Format of the file name of recordings; For example:
## motion-2020-05-11T12-00-00.123+0000.mp4 - Set PREFIX to deposit
## recording from several cameras in the same directory.  Set DATEFMT
## to change the detail in the timestamp.  You probably don't need
## fractions of a second.  SUFFIX (unset by default) is inserted
## before ".mp4".
PREFIX=motion
DATEFMT='%Y-%m-%dT%H-%M-%S.%3N%z'

## FFmpeg arguments for recordings - These options are applied just
## before the output file is specified.  You might want to use H.264:
## (-vcodec libx264); adjust the colour profile: -vf "format=yuv420p";
## preserve JPEG quality: (-vcodec mjpeg -q:v 3 -huffman optimal).
FFMPEG_OUT=(-vcodec copy)

## Unset to prevent any recordings from actually being made.
RECORD=yes

## Configuration for overlaying the time and score onto a recorded
## video - Choose a monospace font to prevent the panel from jiggling
## as different-sized digits are displayed.  Jiggling is a technical
## term.
FONT=Courier
GRAVITY=NorthWest
POINTSIZE=24

## Destination for notification emails - This must be set to enable
## email notifications.
# EMAIL_TO

## Source for notification emails - This must be set to enable email
## notifications.
# EMAIL_FROM

## URI prefix of motion-detection recordings - This must be set to
## enable email notifications.
# PUBPREFIX
