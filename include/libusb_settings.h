/*
 * File: libusb_settings.h
 *
 * Settings for libusb library
 */

#ifndef LIBUSB_SETTINGS_H
#define LIBUSB_SETTINGS_H

#include <libusb.h>

/*
 *  libusb ver | LIBUSB_API_VERSION
 *  -----------+--------------------
 *  v1.0.24    | 0x01000108
 *  v1.0.25    | 0x01000109
 *  v1.0.26    | 0x01000110
 *  v1.0.27    | 0x01000111
 *  v1.0.28    | 0x01000112
 *  v1.0.29    | 0x01000113
 *  v1.0.30    | 0x01000114
 *  v1.0.31    | 0x01000115
 */

#define MINIMAL_API_VERSION 0x01000108
#if(LIBUSB_API_VERSION < MINIMAL_API_VERSION)
    #error unsupported libusb version
#endif

#endif // LIBUSB_SETTINGS_H
