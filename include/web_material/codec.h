#ifndef ARKWEB_WEB_MATERIAL_CODEC_H_
#define ARKWEB_WEB_MATERIAL_CODEC_H_

#include "web_material/descriptor.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace arkweb::material {

struct CodecLimits {
    std::size_t max_packet_bytes = 1024U * 1024U;
    std::size_t max_updates = 1024U;
    std::size_t max_clips_per_descriptor = 32U;
    std::size_t max_string_bytes = 1024U;
};

enum class CodecError {
    kNone = 0,
    kPacketTooLarge,
    kTruncated,
    kBadMagic,
    kUnsupportedVersion,
    kInvalidEnum,
    kInvalidCount,
    kInvalidString,
    kInvalidDescriptor,
    kChecksumMismatch,
    kTrailingData,
};

std::string_view ToString(CodecError error);

struct EncodeResult {
    CodecError error = CodecError::kNone;
    std::vector<std::uint8_t> bytes;
    std::string detail;

    explicit operator bool() const;
};

struct DecodeResult {
    CodecError error = CodecError::kNone;
    MaterialBatch batch;
    std::string detail;

    explicit operator bool() const;
};

class MaterialCodec {
public:
    explicit MaterialCodec(CodecLimits limits = {});

    EncodeResult Encode(const MaterialBatch& batch) const;
    DecodeResult Decode(const std::vector<std::uint8_t>& bytes) const;
    DecodeResult Decode(const std::uint8_t* data, std::size_t size) const;

    const CodecLimits& Limits() const;

private:
    CodecLimits limits_;
};

}  // namespace arkweb::material

#endif  // ARKWEB_WEB_MATERIAL_CODEC_H_
