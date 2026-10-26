#ifndef NATIVE_UI_PROTOCOL_H
#define NATIVE_UI_PROTOCOL_H

#include <cstdint>

// Identical copies are kept in the server and client source repositories.
namespace NativeUIProtocol
{
constexpr uint16_t LegacyVersion = 1;
constexpr uint16_t WideVersion = 2;
constexpr uint16_t MaxFields = 128;
constexpr uint8_t MaxKeyBytes = 63;
constexpr uint16_t MaxValueBytes = 1023;
constexpr uint16_t MaxIntegerBytes = 79;
constexpr uint8_t LastUnitKind = 3;
constexpr uint8_t LastSnapshotKind = 5;
constexpr uint16_t TooltipProtocolVersion = 1;
constexpr uint16_t MaxTooltipPayloadBytes = 65520;
}

#endif
