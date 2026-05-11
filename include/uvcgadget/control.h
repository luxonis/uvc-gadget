/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * UVC control registration API
 *
 * Copyright (C) 2026 Luxonis
 */

#ifndef __UVC_CONTROL_H__
#define __UVC_CONTROL_H__

#include <stdint.h>

struct uvc_stream;
struct uvc_device;

enum uvc_control_section {
	UVC_CONTROL_SECTION_CAMERA_TERMINAL,
	UVC_CONTROL_SECTION_PROCESSING_UNIT,
	UVC_CONTROL_SECTION_EXTENSION_UNIT,
};

#define UVC_CONTROL_CAP_GET	0x01
#define UVC_CONTROL_CAP_SET	0x02
#define UVC_CONTROL_CAP_RW	(UVC_CONTROL_CAP_GET | UVC_CONTROL_CAP_SET)

struct uvc_control;

typedef int (*uvc_control_get_cb_t)(struct uvc_stream *stream,
				       const struct uvc_control *control,
				       uint8_t request, uint8_t *data,
				       uint16_t *size, void *userdata);

typedef int (*uvc_control_set_cb_t)(struct uvc_stream *stream,
				       const struct uvc_control *control,
				       const uint8_t *data, uint16_t size,
				       void *userdata);

struct uvc_control_ops {
	/*
	 * Optional callback for GET_CUR/GET_MIN/GET_MAX/GET_RES/GET_DEF.
	 * Return -EOPNOTSUPP to fall back to the backing buffers in
	 * struct uvc_control.
	 */
	uvc_control_get_cb_t get;

	/*
	 * Optional callback for SET_CUR. The callback receives the payload as
	 * sent by the host. On success, the library copies the payload into
	 * the current-value backing buffer when one is provided.
	 */
	uvc_control_set_cb_t set;
};

enum uvc_control_alloc_flags {
	UVC_CONTROL_ALLOC_CUR = 1 << 0,
	UVC_CONTROL_ALLOC_MIN = 1 << 1,
	UVC_CONTROL_ALLOC_MAX = 1 << 2,
	UVC_CONTROL_ALLOC_RES = 1 << 3,
	UVC_CONTROL_ALLOC_DEF = 1 << 4,
};

struct uvc_control {
	enum uvc_control_section section;
	uint8_t entity_id;
	uint8_t selector;
	uint16_t size;

	/*
	 * GET_INFO response byte as defined by the UVC specification.
	 * Use 0x03 for the common read/write case.
	 */
	uint8_t info;

	/*
	 * Backing buffers used by the default GET_* and SET_CUR handlers.
	 *
	 * Unless documented otherwise by a helper, the caller owns the storage
	 * and must keep it valid until the control is unregistered or the
	 * stream is deleted.
	 */
	void *cur;
	const void *min;
	const void *max;
	const void *res;
	const void *def;

	const struct uvc_control_ops *ops;
	void *userdata;

	/* Internal ownership bits used by helper-backed controls. */
	uint8_t alloc_flags;
};

/*
 * Initialize a control descriptor for an unsigned 8-bit payload.
 *
 * The helper allocates backing storage for cur/min/max/res/def, sets the
 * control capabilities to UVC_CONTROL_CAP_RW, and stores the ownership
 * information inside struct uvc_control. The caller must later call
 * uvc_control_deinit() after the control has been unregistered or after the
 * owning stream has been deleted.
 */
int uvc_control_init_uint8(struct uvc_control *control,
			   enum uvc_control_section section,
			   uint8_t entity_id, uint8_t selector,
			   uint8_t cur, uint8_t min, uint8_t max,
			   uint8_t res, uint8_t def,
			   const struct uvc_control_ops *ops,
			   void *userdata);

/*
 * Initialize a control descriptor for a signed 8-bit payload.
 *
 */
int uvc_control_init_int8(struct uvc_control *control,
			  enum uvc_control_section section,
			  uint8_t entity_id, uint8_t selector,
			  int8_t cur, int8_t min, int8_t max,
			  int8_t res, int8_t def,
			  const struct uvc_control_ops *ops,
			  void *userdata);

/*
 * Initialize a control descriptor for a signed 16-bit payload.
 *
 */
int uvc_control_init_int16(struct uvc_control *control,
			   enum uvc_control_section section,
			   uint8_t entity_id, uint8_t selector,
			   int16_t cur, int16_t min, int16_t max,
			   int16_t res, int16_t def,
			   const struct uvc_control_ops *ops,
			   void *userdata);

/*
 * Initialize a control descriptor for an unsigned 32-bit payload.
 *
 */
int uvc_control_init_uint32(struct uvc_control *control,
			    enum uvc_control_section section,
			    uint8_t entity_id, uint8_t selector,
			    uint32_t cur, uint32_t min, uint32_t max,
			    uint32_t res, uint32_t def,
			    const struct uvc_control_ops *ops,
			    void *userdata);

/*
 * Release helper-owned backing storage inside a control descriptor.
 * Safe to call on a zero-initialized or already deinitialized control.
 */
void uvc_control_deinit(struct uvc_control *control);

/*
 * Register a caller-owned control descriptor. The control object must remain
 * valid until it is unregistered or until the stream is deleted.
 */
int uvc_stream_register_control(struct uvc_stream *stream,
				const struct uvc_control *control);

/*
 * Convenience helper that initializes a caller-owned unsigned 8-bit control
 * and registers it in one step.
 *
 * The helper resolves the entity ID automatically from the stream's ConfigFS
 * metadata based on the control section.
 */
int uvc_stream_register_control_uint8(struct uvc_stream *stream,
				      struct uvc_control *control,
				      enum uvc_control_section section,
				      uint8_t selector,
				      uint8_t cur, uint8_t min, uint8_t max,
				      uint8_t res, uint8_t def,
				      const struct uvc_control_ops *ops);

/*
 * Convenience helper that initializes a caller-owned signed 8-bit control and
 * registers it in one step.
 *
 * The helper resolves the entity ID automatically from the stream's ConfigFS
 * metadata based on the control section.
 */
int uvc_stream_register_control_int8(struct uvc_stream *stream,
				     struct uvc_control *control,
				     enum uvc_control_section section,
				     uint8_t selector,
				     int8_t cur, int8_t min, int8_t max,
				     int8_t res, int8_t def,
				     const struct uvc_control_ops *ops);

/*
 * Convenience helper that initializes a caller-owned signed 16-bit control
 * and registers it in one step.
 *
 * The helper resolves the entity ID automatically from the stream's ConfigFS
 * metadata based on the control section.
 */
int uvc_stream_register_control_int16(struct uvc_stream *stream,
				      struct uvc_control *control,
				      enum uvc_control_section section,
				      uint8_t selector,
				      int16_t cur, int16_t min, int16_t max,
				      int16_t res, int16_t def,
				      const struct uvc_control_ops *ops);

/*
 * Convenience helper that initializes a caller-owned unsigned 32-bit control
 * and registers it in one step.
 *
 * The helper resolves the entity ID automatically from the stream's ConfigFS
 * metadata based on the control section.
 */
int uvc_stream_register_control_uint32(struct uvc_stream *stream,
				       struct uvc_control *control,
				       enum uvc_control_section section,
				       uint8_t selector,
				       uint32_t cur, uint32_t min, uint32_t max,
				       uint32_t res, uint32_t def,
				       const struct uvc_control_ops *ops);

int uvc_stream_unregister_control(struct uvc_stream *stream,
				  enum uvc_control_section section,
				  uint8_t entity_id, uint8_t selector);


/* Internal helpers */
void uvc_controls_cleanup(struct uvc_device *dev);

int uvc_register_control(struct uvc_device *dev,
			 const struct uvc_control *control);
int uvc_unregister_control(struct uvc_device *dev,
			   enum uvc_control_section section,
			   uint8_t entity_id, uint8_t selector);

int uvc_control_process_setup(struct uvc_device *dev, uint8_t request,
			      uint8_t entity_id, uint8_t selector,
			      uint16_t length, struct uvc_request_data *resp);
void uvc_control_process_data(struct uvc_device *dev,
			      const struct uvc_request_data *data);

#endif /* __UVC_CONTROL_H__ */
