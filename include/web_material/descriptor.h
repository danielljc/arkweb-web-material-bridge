#ifndef ARKWEB_WEB_MATERIAL_DESCRIPTOR_H_
#define ARKWEB_WEB_MATERIAL_DESCRIPTOR_H_

#include "web_material/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arkweb::material {

using ElementId = std::uint64_t;
using DocumentId = std::uint64_t;

constexpr std::uint16_t kMaterialProtocolMajorVersion = 1U;
constexpr std::uint16_t kMaterialProtocolMinorVersion = 0U;

enum class DescriptorFlag : std::uint32_t {
    kNone = 0U,
    kFixedPosition = 1U << 0U,
    kScrollable = 1U << 1U,
    kAnimated = 1U << 2U,
    kCrossOrigin = 1U << 3U,
    kHasComplexClip = 1U << 4U,
    kHasPerspective = 1U << 5U,
    kOccluded = 1U << 6U,
};

DescriptorFlag operator|(DescriptorFlag lhs, DescriptorFlag rhs);
DescriptorFlag operator&(DescriptorFlag lhs, DescriptorFlag rhs);
DescriptorFlag& operator|=(DescriptorFlag& lhs, DescriptorFlag rhs);
bool HasFlag(DescriptorFlag flags, DescriptorFlag flag);

struct DocumentContext {
    DocumentId document_id = 0U;
    std::string origin;
    TrustLevel trust_level = TrustLevel::kUntrusted;
    ColorScheme color_scheme = ColorScheme::kSystem;
    ContrastPreference contrast = ContrastPreference::kSystem;
    PowerPreference power_preference = PowerPreference::kDefault;
    float device_scale_factor = 1.0F;
    RectF viewport;
    std::uint64_t navigation_epoch = 0U;

    bool IsValid() const;
    bool IsSameNavigation(const DocumentContext& other) const;
};

struct ClipNode {
    std::uint64_t node_id = 0U;
    RectF rect;
    CornerRadii radii;
    Matrix3 transform_to_viewport;
    bool anti_alias = true;

    bool IsValid() const;
    bool NearlyEquals(const ClipNode& other, float epsilon = kGeometryEpsilon) const;
};

struct MaterialDescriptor {
    ElementId element_id = 0U;
    DocumentId document_id = 0U;
    MaterialRole role = MaterialRole::kNone;
    RectF viewport_rect;
    CornerRadii corner_radii;
    Matrix3 local_to_viewport;
    std::vector<ClipNode> clip_chain;
    float opacity = 1.0F;
    std::int32_t z_order = 0;
    ColorScheme color_scheme = ColorScheme::kSystem;
    ContrastPreference contrast = ContrastPreference::kSystem;
    PowerPreference power_preference = PowerPreference::kDefault;
    TrustLevel trust_level = TrustLevel::kUntrusted;
    DescriptorFlag flags = DescriptorFlag::kNone;
    std::uint64_t source_revision = 0U;
    std::string debug_name;

    bool IsStructurallyValid() const;
    RectF EffectiveClipRect() const;
    RectF VisibleRect(const RectF& viewport) const;
    float VisibleArea(const RectF& viewport) const;
    bool IsVisible(const RectF& viewport) const;
    bool NearlyEquals(const MaterialDescriptor& other,
        float epsilon = kGeometryEpsilon) const;
    std::string DebugString() const;
};

enum class UpdateType : std::uint8_t {
    kAdd = 0,
    kModify = 1,
    kRemove = 2,
    kClearDocument = 3,
};

enum class ChangedField : std::uint32_t {
    kNone = 0U,
    kRole = 1U << 0U,
    kGeometry = 1U << 1U,
    kClip = 1U << 2U,
    kOpacity = 1U << 3U,
    kStacking = 1U << 4U,
    kAppearance = 1U << 5U,
    kFlags = 1U << 6U,
    kMetadata = 1U << 7U,
    kAll = 0xFFFFFFFFU,
};

ChangedField operator|(ChangedField lhs, ChangedField rhs);
ChangedField& operator|=(ChangedField& lhs, ChangedField rhs);
bool HasField(ChangedField fields, ChangedField field);

ChangedField CompareDescriptors(const MaterialDescriptor& before,
    const MaterialDescriptor& after,
    float epsilon = kGeometryEpsilon);

struct MaterialUpdate {
    UpdateType type = UpdateType::kAdd;
    ElementId element_id = 0U;
    DocumentId document_id = 0U;
    ChangedField changed_fields = ChangedField::kNone;
    std::optional<MaterialDescriptor> descriptor;
    std::uint64_t sequence = 0U;

    static MaterialUpdate Add(MaterialDescriptor descriptor);
    static MaterialUpdate Modify(MaterialDescriptor descriptor, ChangedField fields);
    static MaterialUpdate Remove(DocumentId document_id, ElementId element_id);
    static MaterialUpdate Clear(DocumentId document_id);

    bool IsValid() const;
};

struct MaterialBatch {
    std::uint16_t major_version = kMaterialProtocolMajorVersion;
    std::uint16_t minor_version = kMaterialProtocolMinorVersion;
    DocumentId document_id = 0U;
    std::uint64_t navigation_epoch = 0U;
    std::uint64_t batch_sequence = 0U;
    std::vector<MaterialUpdate> updates;

    bool IsValid() const;
    std::size_t AddedCount() const;
    std::size_t ModifiedCount() const;
    std::size_t RemovedCount() const;
    bool IsEmpty() const;
};

}  // namespace arkweb::material

#endif  // ARKWEB_WEB_MATERIAL_DESCRIPTOR_H_
