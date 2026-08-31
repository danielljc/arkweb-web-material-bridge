#include "test_fixtures.h"
#include "test_framework.h"

#include "web_material/codec.h"

#include <cstdint>
#include <vector>

using namespace arkweb::material;

namespace {

std::uint32_t TestCrc32(const std::uint8_t* data, std::size_t size)
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

void RewriteChecksum(std::vector<std::uint8_t>* bytes)
{
    const std::size_t payload_size = bytes->size() - sizeof(std::uint32_t);
    const std::uint32_t checksum = TestCrc32(bytes->data(), payload_size);
    for (std::size_t byte = 0U; byte < sizeof(checksum); ++byte) {
        (*bytes)[payload_size + byte] =
            static_cast<std::uint8_t>((checksum >> (byte * 8U)) & 0xFFU);
    }
}

}  // namespace

TEST(CodecTest, RoundTripsCompleteDescriptor)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.clip_chain = {
        arkweb::material::test::MakeClip(1U),
        arkweb::material::test::MakeClip(2U),
    };
    descriptor.flags = DescriptorFlag::kAnimated | DescriptorFlag::kScrollable;
    MaterialBatch batch = arkweb::material::test::MakeAddBatch(descriptor);

    MaterialCodec codec;
    const EncodeResult encoded = codec.Encode(batch);
    ASSERT_TRUE(encoded);
    const DecodeResult decoded = codec.Decode(encoded.bytes);
    ASSERT_TRUE(decoded);
    ASSERT_EQ(decoded.batch.updates.size(), 1U);
    ASSERT_TRUE(decoded.batch.updates.front().descriptor.has_value());
    EXPECT_TRUE(decoded.batch.updates.front().descriptor->NearlyEquals(descriptor));
}

TEST(CodecTest, RoundTripsAddModifyAndRemove)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    MaterialBatch batch;
    batch.document_id = descriptor.document_id;
    batch.navigation_epoch = 4U;
    batch.batch_sequence = 9U;
    MaterialUpdate add = MaterialUpdate::Add(descriptor);
    add.sequence = 10U;
    descriptor.opacity = 0.75F;
    MaterialUpdate modify = MaterialUpdate::Modify(descriptor, ChangedField::kOpacity);
    modify.sequence = 11U;
    MaterialUpdate remove = MaterialUpdate::Remove(descriptor.document_id, descriptor.element_id);
    remove.sequence = 12U;
    batch.updates = {add, modify, remove};

    MaterialCodec codec;
    const EncodeResult encoded = codec.Encode(batch);
    ASSERT_TRUE(encoded);
    const DecodeResult decoded = codec.Decode(encoded.bytes);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded.batch.updates.size(), 3U);
    EXPECT_EQ(decoded.batch.updates[0].type, UpdateType::kAdd);
    EXPECT_EQ(decoded.batch.updates[1].type, UpdateType::kModify);
    EXPECT_EQ(decoded.batch.updates[2].type, UpdateType::kRemove);
}

TEST(CodecTest, RejectsEmptyInput)
{
    const MaterialCodec codec;
    const DecodeResult result = codec.Decode(std::vector<std::uint8_t>{});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, CodecError::kTruncated);
}

TEST(CodecTest, RejectsBadMagic)
{
    MaterialCodec codec;
    EncodeResult encoded = codec.Encode(
        arkweb::material::test::MakeAddBatch(arkweb::material::test::MakeDescriptor()));
    ASSERT_TRUE(encoded);
    encoded.bytes[0] ^= 0xFFU;
    RewriteChecksum(&encoded.bytes);
    const DecodeResult decoded = codec.Decode(encoded.bytes);
    EXPECT_FALSE(decoded);
    EXPECT_EQ(decoded.error, CodecError::kBadMagic);
}

TEST(CodecTest, RejectsEveryTruncatedPrefix)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.clip_chain.push_back(arkweb::material::test::MakeClip());
    MaterialCodec codec;
    const EncodeResult encoded = codec.Encode(arkweb::material::test::MakeAddBatch(descriptor));
    ASSERT_TRUE(encoded);
    for (std::size_t size = 0U; size < encoded.bytes.size(); ++size) {
        const DecodeResult decoded = codec.Decode(encoded.bytes.data(), size);
        EXPECT_FALSE(decoded);
    }
}

TEST(CodecTest, RejectsTrailingBytes)
{
    MaterialCodec codec;
    EncodeResult encoded = codec.Encode(
        arkweb::material::test::MakeAddBatch(arkweb::material::test::MakeDescriptor()));
    ASSERT_TRUE(encoded);
    encoded.bytes.insert(encoded.bytes.end() - static_cast<std::ptrdiff_t>(sizeof(std::uint32_t)),
        0x42U);
    RewriteChecksum(&encoded.bytes);
    const DecodeResult decoded = codec.Decode(encoded.bytes);
    EXPECT_FALSE(decoded);
    EXPECT_EQ(decoded.error, CodecError::kTrailingData);
}

TEST(CodecTest, EnforcesPacketByteLimit)
{
    CodecLimits limits;
    limits.max_packet_bytes = 32U;
    MaterialCodec codec(limits);
    const EncodeResult encoded = codec.Encode(
        arkweb::material::test::MakeAddBatch(arkweb::material::test::MakeDescriptor()));
    EXPECT_FALSE(encoded);
    EXPECT_EQ(encoded.error, CodecError::kPacketTooLarge);
}

TEST(CodecTest, EnforcesUpdateCountLimit)
{
    CodecLimits limits;
    limits.max_updates = 1U;
    MaterialCodec codec(limits);
    MaterialDescriptor first = arkweb::material::test::MakeDescriptor(1U);
    MaterialDescriptor second = arkweb::material::test::MakeDescriptor(2U);
    MaterialBatch batch;
    batch.document_id = first.document_id;
    batch.navigation_epoch = 1U;
    batch.batch_sequence = 1U;
    MaterialUpdate first_update = MaterialUpdate::Add(first);
    first_update.sequence = 1U;
    MaterialUpdate second_update = MaterialUpdate::Add(second);
    second_update.sequence = 2U;
    batch.updates = {first_update, second_update};
    const EncodeResult encoded = codec.Encode(batch);
    EXPECT_FALSE(encoded);
    EXPECT_EQ(encoded.error, CodecError::kInvalidCount);
}

TEST(CodecTest, EnforcesDebugNameLimit)
{
    CodecLimits limits;
    limits.max_string_bytes = 8U;
    MaterialCodec codec(limits);
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.debug_name = "this-name-is-too-long";
    const EncodeResult encoded = codec.Encode(arkweb::material::test::MakeAddBatch(descriptor));
    EXPECT_FALSE(encoded);
    EXPECT_EQ(encoded.error, CodecError::kInvalidString);
}

TEST(CodecTest, EncodingIsDeterministic)
{
    const MaterialBatch batch = arkweb::material::test::MakeAddBatch(
        arkweb::material::test::MakeDescriptor());
    MaterialCodec codec;
    const EncodeResult first = codec.Encode(batch);
    const EncodeResult second = codec.Encode(batch);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first.bytes, second.bytes);
}
