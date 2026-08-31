#include "web_material/codec.h"

#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace arkweb::material {
namespace {

constexpr std::uint32_t kPacketMagic = 0x31424D57U;  // "WMB1" in little endian.
constexpr std::size_t kChecksumBytes = sizeof(std::uint32_t);

template <typename Type, bool IsEnum = std::is_enum_v<Type>>
struct IntegerStorageType {
    using type = Type;
};

template <typename Type>
struct IntegerStorageType<Type, true> {
    using type = std::underlying_type_t<Type>;
};

std::uint32_t ComputeCrc32(const std::uint8_t* data, std::size_t size)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

void AppendUint32(std::vector<std::uint8_t>* bytes, std::uint32_t value)
{
    for (std::size_t byte = 0U; byte < sizeof(value); ++byte) {
        bytes->push_back(static_cast<std::uint8_t>((value >> (byte * 8U)) & 0xFFU));
    }
}

std::uint32_t ReadUint32At(const std::uint8_t* data)
{
    std::uint32_t value = 0U;
    for (std::size_t byte = 0U; byte < sizeof(value); ++byte) {
        value |= static_cast<std::uint32_t>(data[byte]) << (byte * 8U);
    }
    return value;
}

class BinaryWriter {
public:
    explicit BinaryWriter(std::size_t maximum_size) : maximum_size_(maximum_size) {}

    template <typename Integer,
        typename = std::enable_if_t<std::is_integral_v<Integer> || std::is_enum_v<Integer>>>
    void WriteInteger(Integer value)
    {
        using RawType = typename IntegerStorageType<Integer>::type;
        using UnsignedType = std::make_unsigned_t<RawType>;
        const UnsignedType raw = static_cast<UnsignedType>(value);
        for (std::size_t byte = 0U; byte < sizeof(RawType); ++byte) {
            WriteByte(static_cast<std::uint8_t>((raw >> (byte * 8U)) & 0xFFU));
        }
    }

    void WriteFloat(float value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        std::uint32_t raw = 0U;
        std::memcpy(&raw, &value, sizeof(raw));
        WriteInteger(raw);
    }

    void WriteString(const std::string& value)
    {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
            overflow_ = true;
            return;
        }
        WriteInteger(static_cast<std::uint32_t>(value.size()));
        for (char character : value) {
            WriteByte(static_cast<std::uint8_t>(character));
        }
    }

    bool Overflowed() const
    {
        return overflow_;
    }

    std::vector<std::uint8_t> TakeBytes()
    {
        return std::move(bytes_);
    }

private:
    void WriteByte(std::uint8_t value)
    {
        if (bytes_.size() >= maximum_size_) {
            overflow_ = true;
            return;
        }
        bytes_.push_back(value);
    }

    std::size_t maximum_size_;
    bool overflow_ = false;
    std::vector<std::uint8_t> bytes_;
};

class BinaryReader {
public:
    BinaryReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    template <typename Integer,
        typename = std::enable_if_t<std::is_integral_v<Integer>>>
    bool ReadInteger(Integer* output)
    {
        using UnsignedType = std::make_unsigned_t<Integer>;
        if (Remaining() < sizeof(Integer)) {
            failed_ = true;
            return false;
        }
        UnsignedType raw = 0U;
        for (std::size_t byte = 0U; byte < sizeof(Integer); ++byte) {
            raw |= static_cast<UnsignedType>(data_[position_++]) << (byte * 8U);
        }
        *output = static_cast<Integer>(raw);
        return true;
    }

    template <typename Enum, typename = std::enable_if_t<std::is_enum_v<Enum>>>
    bool ReadEnum(Enum* output)
    {
        using RawType = std::underlying_type_t<Enum>;
        RawType raw = 0;
        if (!ReadInteger(&raw)) {
            return false;
        }
        *output = static_cast<Enum>(raw);
        return true;
    }

    bool ReadFloat(float* output)
    {
        std::uint32_t raw = 0U;
        if (!ReadInteger(&raw)) {
            return false;
        }
        std::memcpy(output, &raw, sizeof(raw));
        return true;
    }

    bool ReadString(std::string* output, std::size_t maximum_length)
    {
        std::uint32_t length = 0U;
        if (!ReadInteger(&length)) {
            return false;
        }
        if (length > maximum_length || Remaining() < length) {
            failed_ = true;
            return false;
        }
        output->assign(reinterpret_cast<const char*>(data_ + position_), length);
        position_ += length;
        return true;
    }

    std::size_t Remaining() const
    {
        return size_ - position_;
    }

    bool Failed() const
    {
        return failed_;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t position_ = 0U;
    bool failed_ = false;
};

void WriteRect(BinaryWriter* writer, const RectF& rect)
{
    writer->WriteFloat(rect.x);
    writer->WriteFloat(rect.y);
    writer->WriteFloat(rect.width);
    writer->WriteFloat(rect.height);
}

void WriteRadii(BinaryWriter* writer, const CornerRadii& radii)
{
    writer->WriteFloat(radii.top_left);
    writer->WriteFloat(radii.top_right);
    writer->WriteFloat(radii.bottom_right);
    writer->WriteFloat(radii.bottom_left);
}

void WriteMatrix(BinaryWriter* writer, const Matrix3& matrix)
{
    for (float value : matrix.values) {
        writer->WriteFloat(value);
    }
}

void WriteClip(BinaryWriter* writer, const ClipNode& clip)
{
    writer->WriteInteger(clip.node_id);
    WriteRect(writer, clip.rect);
    WriteRadii(writer, clip.radii);
    WriteMatrix(writer, clip.transform_to_viewport);
    writer->WriteInteger(static_cast<std::uint8_t>(clip.anti_alias ? 1U : 0U));
}

void WriteDescriptor(BinaryWriter* writer, const MaterialDescriptor& descriptor)
{
    writer->WriteInteger(descriptor.element_id);
    writer->WriteInteger(descriptor.document_id);
    writer->WriteInteger(descriptor.role);
    WriteRect(writer, descriptor.viewport_rect);
    WriteRadii(writer, descriptor.corner_radii);
    WriteMatrix(writer, descriptor.local_to_viewport);
    writer->WriteInteger(static_cast<std::uint32_t>(descriptor.clip_chain.size()));
    for (const ClipNode& clip : descriptor.clip_chain) {
        WriteClip(writer, clip);
    }
    writer->WriteFloat(descriptor.opacity);
    writer->WriteInteger(descriptor.z_order);
    writer->WriteInteger(descriptor.color_scheme);
    writer->WriteInteger(descriptor.contrast);
    writer->WriteInteger(descriptor.power_preference);
    writer->WriteInteger(descriptor.trust_level);
    writer->WriteInteger(descriptor.flags);
    writer->WriteInteger(descriptor.source_revision);
    writer->WriteString(descriptor.debug_name);
}

void WriteUpdate(BinaryWriter* writer, const MaterialUpdate& update)
{
    writer->WriteInteger(update.type);
    writer->WriteInteger(update.element_id);
    writer->WriteInteger(update.document_id);
    writer->WriteInteger(update.changed_fields);
    writer->WriteInteger(update.sequence);
    writer->WriteInteger(static_cast<std::uint8_t>(update.descriptor.has_value() ? 1U : 0U));
    if (update.descriptor.has_value()) {
        WriteDescriptor(writer, *update.descriptor);
    }
}

bool ReadRect(BinaryReader* reader, RectF* rect)
{
    return reader->ReadFloat(&rect->x) && reader->ReadFloat(&rect->y) &&
        reader->ReadFloat(&rect->width) && reader->ReadFloat(&rect->height);
}

bool ReadRadii(BinaryReader* reader, CornerRadii* radii)
{
    return reader->ReadFloat(&radii->top_left) && reader->ReadFloat(&radii->top_right) &&
        reader->ReadFloat(&radii->bottom_right) && reader->ReadFloat(&radii->bottom_left);
}

bool ReadMatrix(BinaryReader* reader, Matrix3* matrix)
{
    for (float& value : matrix->values) {
        if (!reader->ReadFloat(&value)) {
            return false;
        }
    }
    return true;
}

bool IsKnownRole(MaterialRole role)
{
    return role >= MaterialRole::kNone && role <= MaterialRole::kStatus;
}

bool IsKnownColorScheme(ColorScheme scheme)
{
    return scheme >= ColorScheme::kSystem && scheme <= ColorScheme::kDark;
}

bool IsKnownContrast(ContrastPreference preference)
{
    return preference >= ContrastPreference::kSystem &&
        preference <= ContrastPreference::kHigh;
}

bool IsKnownPowerPreference(PowerPreference preference)
{
    return preference >= PowerPreference::kDefault &&
        preference <= PowerPreference::kHighQuality;
}

bool IsKnownTrustLevel(TrustLevel level)
{
    return level >= TrustLevel::kUntrusted && level <= TrustLevel::kSystem;
}

bool IsKnownUpdateType(UpdateType type)
{
    return type >= UpdateType::kAdd && type <= UpdateType::kClearDocument;
}

bool ReadClip(BinaryReader* reader, ClipNode* clip)
{
    std::uint8_t anti_alias = 0U;
    if (!reader->ReadInteger(&clip->node_id) || !ReadRect(reader, &clip->rect) ||
        !ReadRadii(reader, &clip->radii) || !ReadMatrix(reader, &clip->transform_to_viewport) ||
        !reader->ReadInteger(&anti_alias)) {
        return false;
    }
    if (anti_alias > 1U) {
        return false;
    }
    clip->anti_alias = anti_alias != 0U;
    return true;
}

CodecError ReadDescriptor(BinaryReader* reader,
    const CodecLimits& limits,
    MaterialDescriptor* descriptor)
{
    if (!reader->ReadInteger(&descriptor->element_id) ||
        !reader->ReadInteger(&descriptor->document_id) ||
        !reader->ReadEnum(&descriptor->role) || !ReadRect(reader, &descriptor->viewport_rect) ||
        !ReadRadii(reader, &descriptor->corner_radii) ||
        !ReadMatrix(reader, &descriptor->local_to_viewport)) {
        return CodecError::kTruncated;
    }
    if (!IsKnownRole(descriptor->role)) {
        return CodecError::kInvalidEnum;
    }

    std::uint32_t clip_count = 0U;
    if (!reader->ReadInteger(&clip_count)) {
        return CodecError::kTruncated;
    }
    if (clip_count > limits.max_clips_per_descriptor) {
        return CodecError::kInvalidCount;
    }
    descriptor->clip_chain.reserve(clip_count);
    for (std::uint32_t index = 0U; index < clip_count; ++index) {
        ClipNode clip;
        if (!ReadClip(reader, &clip)) {
            return reader->Failed() ? CodecError::kTruncated : CodecError::kInvalidDescriptor;
        }
        descriptor->clip_chain.push_back(std::move(clip));
    }

    if (!reader->ReadFloat(&descriptor->opacity) ||
        !reader->ReadInteger(&descriptor->z_order) ||
        !reader->ReadEnum(&descriptor->color_scheme) ||
        !reader->ReadEnum(&descriptor->contrast) ||
        !reader->ReadEnum(&descriptor->power_preference) ||
        !reader->ReadEnum(&descriptor->trust_level) ||
        !reader->ReadEnum(&descriptor->flags) ||
        !reader->ReadInteger(&descriptor->source_revision)) {
        return CodecError::kTruncated;
    }
    if (!IsKnownColorScheme(descriptor->color_scheme) ||
        !IsKnownContrast(descriptor->contrast) ||
        !IsKnownPowerPreference(descriptor->power_preference) ||
        !IsKnownTrustLevel(descriptor->trust_level)) {
        return CodecError::kInvalidEnum;
    }
    if (!reader->ReadString(&descriptor->debug_name, limits.max_string_bytes)) {
        return reader->Failed() ? CodecError::kInvalidString : CodecError::kTruncated;
    }
    return descriptor->IsStructurallyValid() ? CodecError::kNone : CodecError::kInvalidDescriptor;
}

CodecError ReadUpdate(BinaryReader* reader,
    const CodecLimits& limits,
    MaterialUpdate* update)
{
    std::uint8_t has_descriptor = 0U;
    if (!reader->ReadEnum(&update->type) || !reader->ReadInteger(&update->element_id) ||
        !reader->ReadInteger(&update->document_id) ||
        !reader->ReadEnum(&update->changed_fields) ||
        !reader->ReadInteger(&update->sequence) ||
        !reader->ReadInteger(&has_descriptor)) {
        return CodecError::kTruncated;
    }
    if (!IsKnownUpdateType(update->type) || has_descriptor > 1U) {
        return CodecError::kInvalidEnum;
    }
    if (has_descriptor != 0U) {
        MaterialDescriptor descriptor;
        const CodecError error = ReadDescriptor(reader, limits, &descriptor);
        if (error != CodecError::kNone) {
            return error;
        }
        update->descriptor = std::move(descriptor);
    }
    return update->IsValid() ? CodecError::kNone : CodecError::kInvalidDescriptor;
}

EncodeResult EncodeFailure(CodecError error, std::string detail)
{
    EncodeResult result;
    result.error = error;
    result.detail = std::move(detail);
    return result;
}

DecodeResult DecodeFailure(CodecError error, std::string detail)
{
    DecodeResult result;
    result.error = error;
    result.detail = std::move(detail);
    return result;
}

}  // namespace

std::string_view ToString(CodecError error)
{
    switch (error) {
        case CodecError::kNone:
            return "none";
        case CodecError::kPacketTooLarge:
            return "packet-too-large";
        case CodecError::kTruncated:
            return "truncated";
        case CodecError::kBadMagic:
            return "bad-magic";
        case CodecError::kUnsupportedVersion:
            return "unsupported-version";
        case CodecError::kInvalidEnum:
            return "invalid-enum";
        case CodecError::kInvalidCount:
            return "invalid-count";
        case CodecError::kInvalidString:
            return "invalid-string";
        case CodecError::kInvalidDescriptor:
            return "invalid-descriptor";
        case CodecError::kChecksumMismatch:
            return "checksum-mismatch";
        case CodecError::kTrailingData:
            return "trailing-data";
    }
    return "unknown";
}

EncodeResult::operator bool() const
{
    return error == CodecError::kNone;
}

DecodeResult::operator bool() const
{
    return error == CodecError::kNone;
}

MaterialCodec::MaterialCodec(CodecLimits limits) : limits_(limits) {}

EncodeResult MaterialCodec::Encode(const MaterialBatch& batch) const
{
    if (!batch.IsValid()) {
        return EncodeFailure(CodecError::kInvalidDescriptor, "batch validation failed");
    }
    if (batch.updates.size() > limits_.max_updates) {
        return EncodeFailure(CodecError::kInvalidCount, "update count exceeds codec limit");
    }
    for (const MaterialUpdate& update : batch.updates) {
        if (update.descriptor.has_value()) {
            if (update.descriptor->clip_chain.size() > limits_.max_clips_per_descriptor) {
                return EncodeFailure(CodecError::kInvalidCount, "clip count exceeds codec limit");
            }
            if (update.descriptor->debug_name.size() > limits_.max_string_bytes) {
                return EncodeFailure(CodecError::kInvalidString, "debug name exceeds codec limit");
            }
        }
    }

    if (limits_.max_packet_bytes <= kChecksumBytes) {
        return EncodeFailure(CodecError::kPacketTooLarge,
            "packet byte limit cannot hold protocol checksum");
    }
    BinaryWriter writer(limits_.max_packet_bytes - kChecksumBytes);
    writer.WriteInteger(kPacketMagic);
    writer.WriteInteger(batch.major_version);
    writer.WriteInteger(batch.minor_version);
    writer.WriteInteger(batch.document_id);
    writer.WriteInteger(batch.navigation_epoch);
    writer.WriteInteger(batch.batch_sequence);
    writer.WriteInteger(static_cast<std::uint32_t>(batch.updates.size()));
    for (const MaterialUpdate& update : batch.updates) {
        WriteUpdate(&writer, update);
    }
    if (writer.Overflowed()) {
        return EncodeFailure(CodecError::kPacketTooLarge, "encoded packet exceeds byte limit");
    }

    EncodeResult result;
    result.bytes = writer.TakeBytes();
    const std::uint32_t checksum = ComputeCrc32(result.bytes.data(), result.bytes.size());
    AppendUint32(&result.bytes, checksum);
    return result;
}

DecodeResult MaterialCodec::Decode(const std::vector<std::uint8_t>& bytes) const
{
    return Decode(bytes.data(), bytes.size());
}

DecodeResult MaterialCodec::Decode(const std::uint8_t* data, std::size_t size) const
{
    if (size > limits_.max_packet_bytes) {
        return DecodeFailure(CodecError::kPacketTooLarge, "input packet exceeds byte limit");
    }
    if (data == nullptr || size <= kChecksumBytes) {
        return DecodeFailure(CodecError::kTruncated, "empty packet");
    }

    const std::size_t payload_size = size - kChecksumBytes;
    const std::uint32_t expected_checksum = ReadUint32At(data + payload_size);
    const std::uint32_t actual_checksum = ComputeCrc32(data, payload_size);
    if (actual_checksum != expected_checksum) {
        return DecodeFailure(CodecError::kChecksumMismatch,
            "packet checksum does not match payload");
    }

    BinaryReader reader(data, payload_size);
    std::uint32_t magic = 0U;
    MaterialBatch batch;
    std::uint32_t update_count = 0U;
    if (!reader.ReadInteger(&magic)) {
        return DecodeFailure(CodecError::kTruncated, "missing packet magic");
    }
    if (magic != kPacketMagic) {
        return DecodeFailure(CodecError::kBadMagic, "packet magic does not match WMB1");
    }
    if (!reader.ReadInteger(&batch.major_version) ||
        !reader.ReadInteger(&batch.minor_version) ||
        !reader.ReadInteger(&batch.document_id) ||
        !reader.ReadInteger(&batch.navigation_epoch) ||
        !reader.ReadInteger(&batch.batch_sequence) ||
        !reader.ReadInteger(&update_count)) {
        return DecodeFailure(CodecError::kTruncated, "incomplete packet header");
    }
    if (batch.major_version != kMaterialProtocolMajorVersion) {
        return DecodeFailure(CodecError::kUnsupportedVersion, "major version mismatch");
    }
    if (update_count > limits_.max_updates) {
        return DecodeFailure(CodecError::kInvalidCount, "update count exceeds codec limit");
    }

    batch.updates.reserve(update_count);
    for (std::uint32_t index = 0U; index < update_count; ++index) {
        MaterialUpdate update;
        const CodecError error = ReadUpdate(&reader, limits_, &update);
        if (error != CodecError::kNone) {
            return DecodeFailure(error, "failed while decoding update " + std::to_string(index));
        }
        batch.updates.push_back(std::move(update));
    }
    if (reader.Remaining() != 0U) {
        return DecodeFailure(CodecError::kTrailingData, "packet contains trailing bytes");
    }
    if (!batch.IsValid()) {
        return DecodeFailure(CodecError::kInvalidDescriptor, "decoded batch validation failed");
    }

    DecodeResult result;
    result.batch = std::move(batch);
    return result;
}

const CodecLimits& MaterialCodec::Limits() const
{
    return limits_;
}

}  // namespace arkweb::material
