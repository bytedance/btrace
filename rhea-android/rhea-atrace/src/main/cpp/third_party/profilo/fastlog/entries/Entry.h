// @generated SignedSource<<92e7f010a9d663922326eefa54f7b0e0>>

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <unistd.h>

#include "entries/EntryType.h"

#pragma once

namespace facebook {
namespace profilo {
namespace entries {

struct __attribute__((packed)) StandardEntry {

  static const uint8_t kSerializationType = 1;

  int32_t id;
  EntryType type;
  int64_t timestamp;
  int32_t tid;
  int32_t callid;
  int32_t matchid;
  int64_t extra;

  static void pack(const StandardEntry& entry, void* dst, size_t size);
  static void unpack(StandardEntry& entry, const void* src, size_t size);

  static size_t calculateSize(StandardEntry const& entry);
};

struct __attribute__((packed)) FramesEntry {

  static const uint8_t kSerializationType = 2;

  int32_t id;
  EntryType type;
  int64_t timestamp;
  int32_t tid;
  int32_t matchid;
  struct {
    const int64_t* values;
    uint16_t size;
  } frames;

  static void pack(const FramesEntry& entry, void* dst, size_t size);
  static void unpack(FramesEntry& entry, const void* src, size_t size);

  static size_t calculateSize(FramesEntry const& entry);
};

struct __attribute__((packed)) BytesEntry {

  static const uint8_t kSerializationType = 3;

  int32_t id;
  EntryType type;
  int32_t matchid;
  struct {
    const uint8_t* values;
    uint16_t size;
  } bytes;

  static void pack(const BytesEntry& entry, void* dst, size_t size);
  static void unpack(BytesEntry& entry, const void* src, size_t size);

  static size_t calculateSize(BytesEntry const& entry);
};


uint8_t peek_type(const void* src, size_t len);

} // namespace entries
} // namespace profilo
} // namespace facebook
