/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * UVC control registration and dispatch
 */

#include <errno.h>
#include <linux/usb/g_uvc.h>
#include <linux/usb/video.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configfs.h"
#include "control.h"
#include "list.h"
#include "stream.h"
#include "uvc.h"

struct uvc_registered_control {
	struct list_entry list;
	const struct uvc_control *control;
};

static void *to_heap(const void *src, size_t size)
{
	void *dst = malloc(size);

	if (dst)
		memcpy(dst, src, size);

	return dst;
}

static int uvc_control_init_sized(struct uvc_control *control,
				  enum uvc_control_section section,
				  uint8_t entity_id, uint8_t selector,
				  uint16_t size, uint8_t info,
				  const void *cur, const void *min,
				  const void *max, const void *res,
				  const void *def,
				  const struct uvc_control_ops *ops,
				  void *userdata)
{
	if (!control)
		return -EINVAL;

	memset(control, 0, sizeof(*control));

	control->section = section;
	control->entity_id = entity_id;
	control->selector = selector;
	control->size = size;
	control->info = info;
	control->cur = to_heap(cur, size);
	if (!control->cur)
		goto nomem;
	control->alloc_flags |= UVC_CONTROL_ALLOC_CUR;

	control->min = to_heap(min, size);
	if (!control->min)
		goto nomem;
	control->alloc_flags |= UVC_CONTROL_ALLOC_MIN;

	control->max = to_heap(max, size);
	if (!control->max)
		goto nomem;
	control->alloc_flags |= UVC_CONTROL_ALLOC_MAX;

	control->res = to_heap(res, size);
	if (!control->res)
		goto nomem;
	control->alloc_flags |= UVC_CONTROL_ALLOC_RES;

	control->def = to_heap(def, size);
	if (!control->def)
		goto nomem;
	control->alloc_flags |= UVC_CONTROL_ALLOC_DEF;

	control->ops = ops;
	control->userdata = userdata;

	return 0;

nomem:
	uvc_control_deinit(control);
	return -ENOMEM;
}

static int uvc_stream_register_control_sized(
	struct uvc_stream *stream, struct uvc_control *control,
	enum uvc_control_section section, uint8_t entity_id, uint8_t selector,
	uint16_t size, uint8_t info, const void *cur, const void *min,
	const void *max, const void *res, const void *def,
	const struct uvc_control_ops *ops, void *userdata)
{
	int ret;

	ret = uvc_control_init_sized(control, section, entity_id, selector,
				     size, info, cur, min, max, res, def, ops,
				     userdata);
	if (ret < 0)
		return ret;

	ret = uvc_stream_register_control(stream, control);
	if (ret < 0)
		uvc_control_deinit(control);

	return ret;
}

void uvc_control_deinit(struct uvc_control *control)
{
	if (!control)
		return;

	if (control->alloc_flags & UVC_CONTROL_ALLOC_CUR)
		free(control->cur);
	if (control->alloc_flags & UVC_CONTROL_ALLOC_MIN)
		free((void *)control->min);
	if (control->alloc_flags & UVC_CONTROL_ALLOC_MAX)
		free((void *)control->max);
	if (control->alloc_flags & UVC_CONTROL_ALLOC_RES)
		free((void *)control->res);
	if (control->alloc_flags & UVC_CONTROL_ALLOC_DEF)
		free((void *)control->def);

	memset(control, 0, sizeof(*control));
}

int uvc_control_init_uint8(struct uvc_control *control,
			   enum uvc_control_section section,
			   uint8_t entity_id, uint8_t selector,
			   uint8_t cur, uint8_t min, uint8_t max,
			   uint8_t res, uint8_t def,
			   const struct uvc_control_ops *ops,
			   void *userdata)
{
	return uvc_control_init_sized(control, section, entity_id, selector,
				      sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min, &max, &res,
				      &def, ops, userdata);
}

int uvc_control_init_int8(struct uvc_control *control,
			  enum uvc_control_section section,
			  uint8_t entity_id, uint8_t selector,
			  int8_t cur, int8_t min, int8_t max,
			  int8_t res, int8_t def,
			  const struct uvc_control_ops *ops,
			  void *userdata)
{
	return uvc_control_init_sized(control, section, entity_id, selector,
				      sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min, &max, &res,
				      &def, ops, userdata);
}

int uvc_control_init_int16(struct uvc_control *control,
			   enum uvc_control_section section,
			   uint8_t entity_id, uint8_t selector,
			   int16_t cur, int16_t min, int16_t max,
			   int16_t res, int16_t def,
			   const struct uvc_control_ops *ops,
			   void *userdata)
{
	return uvc_control_init_sized(control, section, entity_id, selector,
				      sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min, &max, &res,
				      &def, ops, userdata);
}

int uvc_control_init_uint32(struct uvc_control *control,
			    enum uvc_control_section section,
			    uint8_t entity_id, uint8_t selector,
			    uint32_t cur, uint32_t min, uint32_t max,
			    uint32_t res, uint32_t def,
			    const struct uvc_control_ops *ops,
			    void *userdata)
{
	return uvc_control_init_sized(control, section, entity_id, selector,
				      sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min, &max, &res,
				      &def, ops, userdata);
}

static const char *uvc_control_section_name(enum uvc_control_section section)
{
	switch (section) {
	case UVC_CONTROL_SECTION_CAMERA_TERMINAL:
		return "camera-terminal";
	case UVC_CONTROL_SECTION_PROCESSING_UNIT:
		return "processing";
	case UVC_CONTROL_SECTION_EXTENSION_UNIT:
		return "extension";
	default:
		return "unknown";
	}
}

static int uvc_control_section_valid(enum uvc_control_section section)
{
	switch (section) {
	case UVC_CONTROL_SECTION_CAMERA_TERMINAL:
	case UVC_CONTROL_SECTION_PROCESSING_UNIT:
	case UVC_CONTROL_SECTION_EXTENSION_UNIT:
		return 1;
	default:
		return 0;
	}
}

static struct uvc_registered_control *
uvc_find_control(struct uvc_device *dev, uint8_t entity_id, uint8_t selector)
{
	struct list_entry *entry;

	list_for_each(entry, &dev->controls) {
		struct uvc_registered_control *control =
			list_entry(entry, struct uvc_registered_control, list);

		if (control->control->entity_id == entity_id &&
		    control->control->selector == selector)
			return control;
	}

	return NULL;
}

static const void *uvc_control_value_ptr(const struct uvc_control *control,
					 uint8_t request)
{
	switch (request) {
	case UVC_GET_CUR:
		return control->cur;
	case UVC_GET_MIN:
		return control->min;
	case UVC_GET_MAX:
		return control->max;
	case UVC_GET_RES:
		return control->res;
	case UVC_GET_DEF:
		return control->def;
	default:
		return NULL;
	}
}

static int uvc_control_fill_data(struct uvc_device *dev,
				 const struct uvc_control *control,
				 uint8_t request, struct uvc_request_data *resp)
{
	uint16_t size = control->size;
	const void *src;
	int ret;

	if (control->ops && control->ops->get) {
		ret = control->ops->get(dev->stream, control, request, resp->data,
					&size, control->userdata);
		if (ret == 0) {
			if (size != control->size)
				return -EINVAL;

			resp->length = size;
			return 0;
		}

		if (ret != -EOPNOTSUPP)
			return ret;
	}

	src = uvc_control_value_ptr(control, request);
	if (!src)
		return -EOPNOTSUPP;

	memcpy(resp->data, src, control->size);
	resp->length = control->size;

	return 0;
}

void uvc_controls_cleanup(struct uvc_device *dev)
{
	struct list_entry *entry;
	struct list_entry *next;

	list_for_each_safe(entry, next, &dev->controls) {
		struct uvc_registered_control *control =
			list_entry(entry, struct uvc_registered_control, list);

		list_remove(&control->list);
		free(control);
	}

	dev->pending_control.control = NULL;
	dev->pending_control.request = 0;
	dev->pending_control.length = 0;
}

int uvc_register_control(struct uvc_device *dev,
			 const struct uvc_control *control)
{
	struct uvc_registered_control *entry;

	if (!control || !uvc_control_section_valid(control->section) ||
	    control->entity_id == 0 || control->selector == 0 ||
	    control->size == 0 || control->size > sizeof(((struct uvc_request_data *)0)->data))
		return -EINVAL;

	if (uvc_find_control(dev, control->entity_id, control->selector))
		return -EEXIST;

	entry = calloc(1, sizeof(*entry));
	if (!entry)
		return -ENOMEM;

	entry->control = control;

	list_append(&entry->list, &dev->controls);

	return 0;
}

int uvc_unregister_control(struct uvc_device *dev,
			   enum uvc_control_section section,
			   uint8_t entity_id, uint8_t selector)
{
	struct uvc_registered_control *control;

	if (!uvc_control_section_valid(section) || entity_id == 0 || selector == 0)
		return -EINVAL;

	control = uvc_find_control(dev, entity_id, selector);
	if (!control || control->control->section != section)
		return -ENOENT;

	if (dev->pending_control.control == control) {
		dev->pending_control.control = NULL;
		dev->pending_control.request = 0;
		dev->pending_control.length = 0;
	}

	list_remove(&control->list);
	free(control);

	return 0;
}

int uvc_control_process_setup(struct uvc_device *dev, uint8_t request,
			      uint8_t entity_id, uint8_t selector,
			      uint16_t length, struct uvc_request_data *resp)
{
	struct uvc_registered_control *control;

	control = uvc_find_control(dev, entity_id, selector);
	if (!control)
		return -ENOENT;

	switch (request) {
	case UVC_SET_CUR:
		if (!(control->control->info & UVC_CONTROL_CAP_SET))
			return -EACCES;
		if (length != control->control->size)
			return -EINVAL;

		dev->pending_control.control = control;
		dev->pending_control.request = request;
		dev->pending_control.length = length;
		resp->length = length;
		return 0;

	case UVC_GET_CUR:
	case UVC_GET_MIN:
	case UVC_GET_MAX:
	case UVC_GET_RES:
	case UVC_GET_DEF:
		if (!(control->control->info & UVC_CONTROL_CAP_GET))
			return -EACCES;
		return uvc_control_fill_data(dev, control->control, request, resp);

	case UVC_GET_LEN:
		resp->data[0] = control->control->size & 0xff;
		resp->data[1] = control->control->size >> 8;
		resp->length = 2;
		return 0;

	case UVC_GET_INFO:
		resp->data[0] = control->control->info;
		resp->length = 1;
		return 0;

	default:
		return -EOPNOTSUPP;
	}
}

void uvc_control_process_data(struct uvc_device *dev,
			      const struct uvc_request_data *data)
{
	struct uvc_registered_control *control = dev->pending_control.control;
	const struct uvc_control *desc;
	int ret = 0;

	if (!control)
		return;

	desc = control->control;

	if (dev->pending_control.request != UVC_SET_CUR) {
		ret = -EOPNOTSUPP;
		goto done;
	}

	if (data->length != dev->pending_control.length) {
		ret = -EINVAL;
		goto done;
	}

	if (desc->ops && desc->ops->set) {
		ret = desc->ops->set(dev->stream, desc, data->data, data->length,
				     desc->userdata);
		if (ret < 0)
			goto done;
	}

	if (desc->cur)
		memcpy(desc->cur, data->data, desc->size);
	else if (!desc->ops || !desc->ops->set)
		ret = -EOPNOTSUPP;

done:
	if (ret < 0) {
		printf("failed to apply %s control entity %u selector 0x%02x: %s (%d)\n",
		       uvc_control_section_name(desc->section), desc->entity_id,
		       desc->selector, strerror(-ret), -ret);
	}

	dev->pending_control.control = NULL;
	dev->pending_control.request = 0;
	dev->pending_control.length = 0;
}

int uvc_stream_register_control(struct uvc_stream *stream,
				const struct uvc_control *control)
{
	if (!stream || !stream->uvc)
		return -EINVAL;

	return uvc_register_control(stream->uvc, control);
}

static int uvc_stream_resolve_entity_id(struct uvc_stream *stream,
					enum uvc_control_section section,
					uint8_t *entity_id)
{
	unsigned int resolved;

	if (!stream || !stream->uvc || !stream->uvc->fc || !entity_id)
		return -EINVAL;

	switch (section) {
	case UVC_CONTROL_SECTION_CAMERA_TERMINAL:
		resolved = stream->uvc->fc->control.camera_terminal_id;
		break;
	case UVC_CONTROL_SECTION_PROCESSING_UNIT:
		resolved = stream->uvc->fc->control.processing_unit_id;
		break;
	case UVC_CONTROL_SECTION_EXTENSION_UNIT:
		resolved = stream->uvc->fc->control.extension_unit_id;
		break;
	default:
		return -EINVAL;
	}

	if (resolved == 0 || resolved > UINT8_MAX)
		return -ENOENT;

	*entity_id = (uint8_t)resolved;
	return 0;
}

int uvc_stream_register_control_uint8(
	struct uvc_stream *stream, struct uvc_control *control,
	enum uvc_control_section section, uint8_t selector,
	uint8_t cur, uint8_t min, uint8_t max,
	uint8_t res, uint8_t def, const struct uvc_control_ops *ops)
{
	uint8_t entity_id;
	int ret;

	ret = uvc_stream_resolve_entity_id(stream, section, &entity_id);
	if (ret < 0)
		return ret;

	return uvc_stream_register_control_sized(stream, control, section,
						 entity_id, selector,
						 sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min,
						 &max, &res, &def, ops,
						 NULL);
}

int uvc_stream_register_control_int8(
	struct uvc_stream *stream, struct uvc_control *control,
	enum uvc_control_section section, uint8_t selector,
	int8_t cur, int8_t min, int8_t max,
	int8_t res, int8_t def, const struct uvc_control_ops *ops)
{
	uint8_t entity_id;
	int ret;

	ret = uvc_stream_resolve_entity_id(stream, section, &entity_id);
	if (ret < 0)
		return ret;

	return uvc_stream_register_control_sized(stream, control, section,
						 entity_id, selector,
						 sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min,
						 &max, &res, &def, ops,
						 NULL);
}

int uvc_stream_register_control_int16(
	struct uvc_stream *stream, struct uvc_control *control,
	enum uvc_control_section section, uint8_t selector,
	int16_t cur, int16_t min, int16_t max,
	int16_t res, int16_t def, const struct uvc_control_ops *ops)
{
	uint8_t entity_id;
	int ret;

	ret = uvc_stream_resolve_entity_id(stream, section, &entity_id);
	if (ret < 0)
		return ret;

	return uvc_stream_register_control_sized(stream, control, section,
						 entity_id, selector,
						 sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min,
						 &max, &res, &def, ops,
						 NULL);
}

int uvc_stream_register_control_uint32(
	struct uvc_stream *stream, struct uvc_control *control,
	enum uvc_control_section section, uint8_t selector,
	uint32_t cur, uint32_t min, uint32_t max,
	uint32_t res, uint32_t def, const struct uvc_control_ops *ops)
{
	uint8_t entity_id;
	int ret;

	ret = uvc_stream_resolve_entity_id(stream, section, &entity_id);
	if (ret < 0)
		return ret;

	return uvc_stream_register_control_sized(stream, control, section,
						 entity_id, selector,
						 sizeof(cur), UVC_CONTROL_CAP_RW, &cur, &min,
						 &max, &res, &def, ops,
						 NULL);
}

int uvc_stream_unregister_control(struct uvc_stream *stream,
				  enum uvc_control_section section,
				  uint8_t entity_id, uint8_t selector)
{
	if (!stream || !stream->uvc)
		return -EINVAL;

	return uvc_unregister_control(stream->uvc, section, entity_id, selector);
}
