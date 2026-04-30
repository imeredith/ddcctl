#ifndef DDCBridge_h
#define DDCBridge_h

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t index;
    uint32_t displayID;
    uint32_t vendorID;
    uint32_t modelID;
    uint32_t serialNumber;
    uint32_t busCount;
} DDCDisplayInfo;

int DDCDisplayCount(void);
int DDCGetDisplayInfo(uint32_t index, DDCDisplayInfo *info);
int DDCSetVCP(uint32_t displayIndex, uint8_t control, uint16_t value, char *errorBuffer, size_t errorBufferLength);
int DDCSetVCPAll(uint8_t control, uint16_t value, char *errorBuffer, size_t errorBufferLength);

#endif
