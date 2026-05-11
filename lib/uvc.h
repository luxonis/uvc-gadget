/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * UVC protocol handling
 *
 * Copyright (C) 2010-2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

#ifndef __UVC_H__
#define __UVC_H__

#include <stdint.h>
#include <linux/usb/video.h>

#include "control.h"
#include "list.h"

struct events;
struct v4l2_device;
struct uvc_device;
struct uvc_function_config;
struct uvc_stream;

struct uvc_registered_control;

struct uvc_still_streaming_control {
	uint8_t  bFormatIndex;
	uint8_t  bFrameIndex;
	uint8_t  bCompressionIndex;
	uint32_t dwMaxVideoFrameSize;
	uint32_t dwMaxPayloadTransferSize;
} __attribute__((packed));

struct uvc_pending_control_request {
	struct uvc_registered_control *control;
	uint8_t request;
	uint16_t length;
};

struct uvc_device
{
	struct v4l2_device *vdev;

	struct uvc_stream *stream;
	struct uvc_function_config *fc;

	struct uvc_streaming_control probe;
	struct uvc_streaming_control commit;

	struct uvc_still_streaming_control still_probe;
	struct uvc_still_streaming_control still_commit;

	int control;

	unsigned int fcc;
	unsigned int width;
	unsigned int height;

	struct list_entry controls;
	struct uvc_pending_control_request pending_control;

	/* Custom optional callback for events handling*/
	void (*uvc_events_cb)(uint32_t arg);
};

struct uvc_device *uvc_open(const char *devname, struct uvc_stream *stream);
void uvc_close(struct uvc_device *dev);
void uvc_events_init(struct uvc_device *dev, struct events *events);
void uvc_set_config(struct uvc_device *dev, struct uvc_function_config *fc);
int uvc_set_format(struct uvc_device *dev, struct v4l2_pix_format *format);
int uvc_set_still_image_next(struct uvc_device *dev, int enable);
struct v4l2_device *uvc_v4l2_device(struct uvc_device *dev);
void uvc_events_register_cb(struct uvc_stream *stream, void (*cb)(uint32_t arg));

#endif /* __UVC_H__ */
