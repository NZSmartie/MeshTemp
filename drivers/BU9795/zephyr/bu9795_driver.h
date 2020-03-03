/*
 * Copyright (c) 2019 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BU9795_DRIVER_H__
#define __BU9795_DRIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <device.h>

struct bu9795_driver_api {
	void (*print)(struct device *dev);
};

__syscall     void        bu9795_print(struct device *dev);
static inline void z_impl_bu9795_print(struct device *dev)
{
	const struct bu9795_driver_api *api = dev->driver_api;

	__ASSERT(api->print, "Callback pointer should not be NULL");

	api->print(dev);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/bu9795_driver.h>

#endif /* __BU9795_DRIVER_H__ */
