/*
 * SPDX-FileCopyrightText: 2024 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "sensors.oplus_virtual"

#include <errno.h>
#include <fcntl.h>
#include <hardware/sensors.h>
#include <inttypes.h>
#include <log/log.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <utils/SystemClock.h>

#include <oplus_sensor_event.h>
#include <oplus_sensor_types.h>

#include "oplus_virtual_sensors.h"

static struct sensor_t virtual_sensors[] = {
        [SENSOR_PICKUP_DETECT] =
                {
                        .name = "Oplus Pickup Sensor",
                        .vendor = "Oplus",
                        .version = 1,
                        .handle = ID_PICKUP_DETECT,
                        .type = SENSOR_TYPE_PICK_UP_GESTURE,
                        .maxRange = 1.0f,
                        .resolution = 1.0f,
                        .power = 0,
                        .minDelay = -1,
                        .fifoReservedEventCount = 0,
                        .fifoMaxEventCount = 0,
                        .stringType = SENSOR_STRING_TYPE_PICK_UP_GESTURE,
                        .requiredPermission = "",
                        .maxDelay = 0,
                        .flags = SENSOR_FLAG_ONE_SHOT_MODE | SENSOR_FLAG_WAKE_UP,
                        .reserved = {},
                },
};

struct virtual_sensors_context_t {
    sensors_poll_device_1_t device;
    int dev_fd;
    int active_fd;
    int batch_fd;
    int flush_fd;
};

static int virtual_sensors_convert_event(oplus_sensor_event_t* oplus_event,
                                         sensors_event_t* event) {
    sensor_t* sensor = NULL;

    memset(event, 0, sizeof(sensors_event_t));
    switch (oplus_event->handle) {
        case ID_PICKUP_DETECT:
            sensor = &virtual_sensors[SENSOR_PICKUP_DETECT];
            event->data[0] = oplus_event->word[0];
            break;
        default:
            ALOGE("Unknown sensor ID %d.", oplus_event->handle);
            return -EINVAL;
    }

    event->version = sizeof(sensors_event_t);
    event->sensor = sensor->handle;
    event->type = sensor->type;
    event->timestamp = ::android::elapsedRealtimeNano();

    return 1;
}

static int virtual_sensors_read_event(int fd, oplus_sensor_event_t* event) {
    int rc;

    rc = read(fd, event, sizeof(oplus_sensor_event_t));
    if (rc < 0) {
        ALOGE("Failed to read: %d", -errno);
        return 1;
    }

    if (rc != sizeof(oplus_sensor_event_t)) {
        ALOGE("Read less data than expected! Dropping event.");
        return 1;
    }

    return 0;
}

static int virtual_sensors_wait_event(int fd, int timeout) {
    int rc;
    struct pollfd fds = {
            .fd = fd,
            .events = POLLIN,
            .revents = 0,
    };

    do {
        rc = poll(&fds, 1, timeout);
    } while (rc < 0 && errno == EINTR);

    return rc;
}

static int virtual_sensors_close(struct hw_device_t* dev) {
    virtual_sensors_context_t* ctx = reinterpret_cast<virtual_sensors_context_t*>(dev);

    if (ctx) {
        close(ctx->dev_fd);
        close(ctx->active_fd);
        close(ctx->batch_fd);
        close(ctx->flush_fd);
        delete ctx;
    }

    return 0;
}

static int virtual_sensors_activate(struct sensors_poll_device_t* dev, int handle, int enabled) {
    virtual_sensors_context_t* ctx = reinterpret_cast<virtual_sensors_context_t*>(dev);
    char buf[64];
    int err;

    if (!ctx) {
        return -EINVAL;
    }

    snprintf(buf, sizeof(buf), "%d,%d", handle, enabled);
    err = write(ctx->active_fd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("Failed to %s sensor %d: %d", enabled ? "enable" : "disable", handle, -errno);
        return -errno;
    }

    return 0;
}

static int virtual_sensors_poll(struct sensors_poll_device_t* dev, sensors_event_t* data,
                                int /* count */) {
    virtual_sensors_context_t* ctx = reinterpret_cast<virtual_sensors_context_t*>(dev);
    struct oplus_sensor_event event;
    int err = 0;

    if (!ctx) {
        return -EINVAL;
    }

    do {
        int rc = virtual_sensors_wait_event(ctx->dev_fd, -1);
        if (rc < 0) {
            ALOGE("Failed to poll for sensor events: %d", -errno);
            return -errno;
        } else if (rc > 0) {
            err = virtual_sensors_read_event(ctx->dev_fd, &event);
        }
    } while (err);

    return virtual_sensors_convert_event(&event, data);
}

static int virtual_sensors_batch(struct sensors_poll_device_1* dev, int handle, int flags,
                                 int64_t period_ns, int64_t max_ns) {
    virtual_sensors_context_t* ctx = reinterpret_cast<virtual_sensors_context_t*>(dev);
    char buf[128];
    int err;

    if (!ctx) {
        return -EINVAL;
    }

    snprintf(buf, sizeof(buf), "%d,%d,%" PRId64 ",%" PRId64, handle, flags, period_ns, max_ns);
    err = write(ctx->batch_fd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("Failed to batch sensor %d: %d", handle, -errno);
        return -errno;
    }

    return 0;
}

static int virtual_sensors_flush(struct sensors_poll_device_1* dev, int handle) {
    virtual_sensors_context_t* ctx = reinterpret_cast<virtual_sensors_context_t*>(dev);
    int err;
    char buf[32];

    if (!ctx) {
        return -EINVAL;
    }

    snprintf(buf, sizeof(buf), "%d", handle);
    err = write(ctx->flush_fd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("%s: Failed to flush sensor: %d", __func__, -errno);
        return -errno;
    }

    return 0;
}

static int open_sensors(const struct hw_module_t* module, const char* /* name */,
                        struct hw_device_t** device) {
    virtual_sensors_context_t* ctx = new virtual_sensors_context_t();

    memset(ctx, 0, sizeof(virtual_sensors_context_t));
    ctx->device.common.tag = HARDWARE_DEVICE_TAG;
    ctx->device.common.version = SENSORS_DEVICE_API_VERSION_1_3;
    ctx->device.common.module = const_cast<hw_module_t*>(module);
    ctx->device.common.close = virtual_sensors_close;
    ctx->device.activate = virtual_sensors_activate;
    ctx->device.poll = virtual_sensors_poll;
    ctx->device.batch = virtual_sensors_batch;
    ctx->device.flush = virtual_sensors_flush;

    ctx->dev_fd = open(VIRTUAL_SENSORS_DEVICE, O_RDONLY);
    if (ctx->dev_fd < 0) {
        ALOGE("Failed to open %s: %d", VIRTUAL_SENSORS_DEVICE, -errno);
        return -errno;
    }

    ctx->active_fd = open(VIRTUAL_SENSORS_ACTIVE, O_WRONLY);
    if (ctx->active_fd < 0) {
        ALOGE("Failed to open %s: %d", VIRTUAL_SENSORS_ACTIVE, -errno);
        return -errno;
    }

    ctx->batch_fd = open(VIRTUAL_SENSORS_BATCH, O_WRONLY);
    if (ctx->batch_fd < 0) {
        ALOGE("Failed to open %s: %d", VIRTUAL_SENSORS_BATCH, -errno);
        return -errno;
    }

    ctx->flush_fd = open(VIRTUAL_SENSORS_FLUSH, O_WRONLY);
    if (ctx->flush_fd < 0) {
        ALOGE("Failed to open %s: %d", VIRTUAL_SENSORS_FLUSH, -errno);
        return -errno;
    }

    *device = &ctx->device.common;

    return 0;
}

static struct hw_module_methods_t virtual_sensors_module_methods = {
        .open = open_sensors,
};

static int virtual_sensors_get_sensors_list(struct sensors_module_t*,
                                            struct sensor_t const** list) {
    *list = virtual_sensors;

    return sizeof(virtual_sensors) / sizeof(sensor_t);
}

static int virtual_sensors_set_operation_mode(unsigned int mode) {
    return !mode ? 0 : -EINVAL;
}

struct sensors_module_t HAL_MODULE_INFO_SYM = {
        .common = {.tag = HARDWARE_MODULE_TAG,
                   .version_major = 1,
                   .version_minor = 0,
                   .id = SENSORS_HARDWARE_MODULE_ID,
                   .name = "Oplus (MediaTek) Virtual Sensors module",
                   .author = "bengris32",
                   .methods = &virtual_sensors_module_methods,
                   .dso = NULL,
                   .reserved = {0}},
        .get_sensors_list = virtual_sensors_get_sensors_list,
        .set_operation_mode = virtual_sensors_set_operation_mode,
};
