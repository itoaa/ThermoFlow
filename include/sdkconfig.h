/**
 * @file sdkconfig.h
 * @brief Minimal sdkconfig for compilation
 */

#ifndef SDKCONFIG_H
#define SDKCONFIG_H

#define CONFIG_LOG_DEFAULT_LEVEL 3
#define CONFIG_LOG_MAXIMUM_LEVEL 3
#define CONFIG_FREERTOS_UNICORE 0
#define CONFIG_IDF_TARGET_ESP32S3 1
#define CONFIG_ESPTOOLPY_FLASHSIZE_8MB 1
#define CONFIG_PARTITION_TABLE_CUSTOM 1

#endif