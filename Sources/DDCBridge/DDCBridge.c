#include "DDCBridge.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *buffer, size_t length, const char *format, ...) {
    if (buffer == NULL || length == 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, length, format, args);
    va_end(args);
}

static kern_return_t create_framebuffer_iterator(io_iterator_t *iterator) {
    CFMutableDictionaryRef matching = IOServiceMatching("IOFramebuffer");
    if (matching == NULL) {
        return kIOReturnNoMemory;
    }

    return IOServiceGetMatchingServices(kIOMainPortDefault, matching, iterator);
}

static int raw_framebuffer_count(void) {
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (create_framebuffer_iterator(&iterator) != kIOReturnSuccess) {
        return 0;
    }

    int count = 0;
    io_service_t framebuffer = IO_OBJECT_NULL;
    while ((framebuffer = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        count++;
        IOObjectRelease(framebuffer);
    }

    IOObjectRelease(iterator);
    return count;
}

static io_service_t copy_raw_framebuffer_at_index(uint32_t index) {
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (create_framebuffer_iterator(&iterator) != kIOReturnSuccess) {
        return IO_OBJECT_NULL;
    }

    io_service_t framebuffer = IO_OBJECT_NULL;
    for (uint32_t current = 0; current <= index; current++) {
        if (framebuffer != IO_OBJECT_NULL) {
            IOObjectRelease(framebuffer);
        }
        framebuffer = IOIteratorNext(iterator);
        if (framebuffer == IO_OBJECT_NULL) {
            break;
        }
    }

    IOObjectRelease(iterator);
    return framebuffer;
}

static int cg_display_count(void) {
    uint32_t count = 0;
    if (CGGetOnlineDisplayList(0, NULL, &count) != kCGErrorSuccess) {
        return 0;
    }
    return (int)count;
}

static io_service_t copy_framebuffer_at_index(uint32_t index) {
    uint32_t count = 0;
    CGError error = CGGetOnlineDisplayList(0, NULL, &count);
    if (error != kCGErrorSuccess || index >= count) {
        return copy_raw_framebuffer_at_index(index);
    }

    if (count == 0) {
        return IO_OBJECT_NULL;
    }

    CGDirectDisplayID displays[count];
    error = CGGetOnlineDisplayList(count, displays, &count);
    if (error != kCGErrorSuccess || index >= count) {
        return IO_OBJECT_NULL;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    io_service_t framebuffer = CGDisplayIOServicePort(displays[index]);
#pragma clang diagnostic pop
    if (framebuffer != IO_OBJECT_NULL) {
        IOObjectRetain(framebuffer);
        return framebuffer;
    }

    return copy_raw_framebuffer_at_index(index);
}

int DDCDisplayCount(void) {
    int cgCount = cg_display_count();
    int rawCount = raw_framebuffer_count();
    return cgCount > rawCount ? cgCount : rawCount;
}

int DDCGetDisplayInfo(uint32_t index, DDCDisplayInfo *info) {
    if (info == NULL) {
        return 0;
    }

    int displayCount = DDCDisplayCount();
    if (index >= (uint32_t)displayCount) {
        return 0;
    }

    memset(info, 0, sizeof(*info));
    info->index = index;

    uint32_t count = 0;
    CGError error = CGGetOnlineDisplayList(0, NULL, &count);
    if (error == kCGErrorSuccess && count > 0 && index < count) {
        CGDirectDisplayID displays[count];
        error = CGGetOnlineDisplayList(count, displays, &count);
        if (error == kCGErrorSuccess && index < count) {
            CGDirectDisplayID display = displays[index];
            info->displayID = display;
            info->vendorID = CGDisplayVendorNumber(display);
            info->modelID = CGDisplayModelNumber(display);
            info->serialNumber = CGDisplaySerialNumber(display);
        }
    }

    io_service_t framebuffer = copy_framebuffer_at_index(index);
    if (framebuffer == IO_OBJECT_NULL) {
        return 1;
    }

    IOItemCount busCount = 0;
    kern_return_t result = IOFBGetI2CInterfaceCount(framebuffer, &busCount);
    IOObjectRelease(framebuffer);

    if (result != kIOReturnSuccess) {
        return 1;
    }

    info->busCount = (uint32_t)busCount;
    return 1;
}

static uint8_t ddc_checksum(const uint8_t *bytes, size_t count) {
    uint8_t checksum = 0x6e;
    for (size_t i = 0; i < count; i++) {
        checksum ^= bytes[i];
    }
    return checksum;
}

static kern_return_t send_vcp_to_bus(io_service_t framebuffer, IOItemCount bus, uint8_t control, uint16_t value) {
    io_service_t interface = IO_OBJECT_NULL;
    kern_return_t result = IOFBCopyI2CInterfaceForBus(framebuffer, bus, &interface);
    if (result != kIOReturnSuccess || interface == IO_OBJECT_NULL) {
        return result;
    }

    IOI2CConnectRef connect = NULL;
    result = IOI2CInterfaceOpen(interface, kNilOptions, &connect);
    if (result != kIOReturnSuccess) {
        IOObjectRelease(interface);
        return result;
    }

    uint8_t payload[7] = {
        0x51,
        0x84,
        0x03,
        control,
        (uint8_t)((value >> 8) & 0xff),
        (uint8_t)(value & 0xff),
        0x00
    };
    payload[6] = ddc_checksum(payload, 6);

    IOI2CRequest request;
    memset(&request, 0, sizeof(request));
    request.commFlags = 0;
    request.sendAddress = 0x6e;
    request.sendTransactionType = kIOI2CSimpleTransactionType;
    request.sendBuffer = (vm_address_t)payload;
    request.sendBytes = sizeof(payload);
    request.replyAddress = 0x6f;
    request.replyTransactionType = kIOI2CNoTransactionType;
    request.replyBytes = 0;
    request.minReplyDelay = 50;

    result = IOI2CSendRequest(connect, kNilOptions, &request);
    if (result == kIOReturnSuccess) {
        result = request.result;
    }

    IOI2CInterfaceClose(connect, kNilOptions);
    IOObjectRelease(interface);
    return result;
}

static int set_vcp_on_framebuffer(io_service_t framebuffer, uint8_t control, uint16_t value, kern_return_t *lastError) {
    IOItemCount busCount = 0;
    kern_return_t result = IOFBGetI2CInterfaceCount(framebuffer, &busCount);
    if (result != kIOReturnSuccess) {
        if (lastError != NULL) {
            *lastError = result;
        }
        return 0;
    }

    int successes = 0;
    for (IOItemCount bus = 0; bus < busCount; bus++) {
        result = send_vcp_to_bus(framebuffer, bus, control, value);
        if (result == kIOReturnSuccess) {
            successes++;
        } else if (lastError != NULL) {
            *lastError = result;
        }
    }

    return successes;
}

int DDCSetVCP(uint32_t displayIndex, uint8_t control, uint16_t value, char *errorBuffer, size_t errorBufferLength) {
    io_service_t framebuffer = copy_framebuffer_at_index(displayIndex);
    if (framebuffer == IO_OBJECT_NULL) {
        set_error(errorBuffer, errorBufferLength, "display %u was not found", displayIndex);
        return 0;
    }

    kern_return_t lastError = kIOReturnSuccess;
    int successes = set_vcp_on_framebuffer(framebuffer, control, value, &lastError);
    IOObjectRelease(framebuffer);

    if (successes == 0) {
        set_error(errorBuffer, errorBufferLength, "DDC write failed for display %u: 0x%08x", displayIndex, lastError);
        return 0;
    }

    return successes;
}

int DDCSetVCPAll(uint8_t control, uint16_t value, char *errorBuffer, size_t errorBufferLength) {
    int displayCount = DDCDisplayCount();
    if (displayCount == 0) {
        set_error(errorBuffer, errorBufferLength, "no online displays found");
        return 0;
    }

    int successes = 0;
    kern_return_t lastError = kIOReturnSuccess;
    for (int index = 0; index < displayCount; index++) {
        io_service_t framebuffer = copy_framebuffer_at_index((uint32_t)index);
        if (framebuffer == IO_OBJECT_NULL) {
            lastError = kIOReturnNotFound;
            continue;
        }
        successes += set_vcp_on_framebuffer(framebuffer, control, value, &lastError);
        IOObjectRelease(framebuffer);
    }

    if (successes == 0) {
        set_error(errorBuffer, errorBufferLength, "DDC write failed on all displays: 0x%08x", lastError);
    }

    return successes;
}
