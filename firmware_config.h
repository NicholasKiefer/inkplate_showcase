#pragma once

// The optional local test profile reads production without reporting health there.
// OTA remains on dev in both profiles.
#ifdef INKPLATE_PRODUCTION_CONTENT_TEST
#define FIRMWARE_ENVIRONMENT "development-production-content-test"
#define FIRMWARE_DEVICE_ID "inkplate-development-production-test"
#define FIRMWARE_CONTENT_URL "https://board.linaku94.workers.dev/api/display/"
#else
#define FIRMWARE_ENVIRONMENT "development"
#define FIRMWARE_DEVICE_ID "inkplate-development"
#define FIRMWARE_CONTENT_URL "https://board-preview.linaku94.workers.dev/api/display/"
#endif
#define FIRMWARE_MANIFEST_URL "https://raw.githubusercontent.com/NicholasKiefer/inkplate_showcase/refs/heads/dev/manifest-dev.txt"
