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
	void (*clear)(struct device *dev);
	void (*set_segment)(struct device *dev, u8_t segment, u8_t value);
	void (*set_symbol)(struct device *dev, u8_t symbol, u8_t state);
	void (*flush)(struct device *dev);
#if CONFIG_BU9795_TEST_PATTERN
	void (*set_test_pattern)(struct device *dev, int stage);
#endif
};

static inline void bu9795_clear(struct device *dev)
{
	const struct bu9795_driver_api *api = dev->driver_api;
	api->clear(dev);
}

static inline void bu9795_set_segment(struct device *dev, u8_t segment, u8_t value)
{
	const struct bu9795_driver_api *api = dev->driver_api;
	api->set_segment(dev, segment, value);
}

static inline void bu9795_set_symbol(struct device *dev, u8_t symbol, u8_t state)
{
	const struct bu9795_driver_api *api = dev->driver_api;
	api->set_symbol(dev, symbol, state);
}

static inline void bu9795_flush(struct device *dev)
{
	const struct bu9795_driver_api *api = dev->driver_api;
	api->flush(dev);
}
#if CONFIG_BU9795_TEST_PATTERN
static inline void bu9795_set_test_pattern(struct device *dev, int stage)
{
	const struct bu9795_driver_api *api = dev->driver_api;
	api->set_test_pattern(dev, stage);
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BU9795_DRIVER_H__ */
