#include "web_material/descriptor.h"

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <utility>

namespace arkweb::material {
namespace {

template <typename Enum>
constexpr auto EnumValue(Enum value)
{
    return static_cast<std::underlying_type_t<Enum>>(value);
}

bool ClipChainsEqual(const std::vector<ClipNode>& lhs,
    const std::vector<ClipNode>& rhs,
    float epsilon)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!lhs[index].NearlyEquals(rhs[index], epsilon)) {
            return false;
        }
    }
    return true;
}

}  // namespace

DescriptorFlag operator|(DescriptorFlag lhs, DescriptorFlag rhs)
{
    return static_cast<DescriptorFlag>(EnumValue(lhs) | EnumValue(rhs));
}

DescriptorFlag operator&(DescriptorFlag lhs, DescriptorFlag rhs)
{
    return static_cast<DescriptorFlag>(EnumValue(lhs) & EnumValue(rhs));
}

DescriptorFlag& operator|=(DescriptorFlag& lhs, DescriptorFlag rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

bool HasFlag(DescriptorFlag flags, DescriptorFlag flag)
{
    return EnumValue(flags & flag) != 0U;
}

bool DocumentContext::IsValid() const
{
    return document_id != 0U && !origin.empty() && viewport.IsFinite() &&
        !viewport.IsEmpty() && IsFinite(device_scale_factor) && device_scale_factor > 0.0F;
}

bool DocumentContext::IsSameNavigation(const DocumentContext& other) const
{
    return document_id == other.document_id && navigation_epoch == other.navigation_epoch;
}

bool ClipNode::IsValid() const
{
    return node_id != 0U && rect.IsFinite() && !rect.IsEmpty() && radii.IsFinite() &&
        radii.IsNonNegative() && transform_to_viewport.IsFinite() &&
        transform_to_viewport.IsInvertible();
}

bool ClipNode::NearlyEquals(const ClipNode& other, float epsilon) const
{
    return node_id == other.node_id && rect.NearlyEquals(other.rect, epsilon) &&
        radii.NearlyEquals(other.radii, epsilon) &&
        transform_to_viewport.NearlyEquals(other.transform_to_viewport, epsilon) &&
        anti_alias == other.anti_alias;
}

bool MaterialDescriptor::IsStructurallyValid() const
{
    if (element_id == 0U || document_id == 0U || role == MaterialRole::kNone) {
        return false;
    }
    if (!viewport_rect.IsFinite() || viewport_rect.IsEmpty() || !corner_radii.IsFinite() ||
        !corner_radii.IsNonNegative()) {
        return false;
    }
    if (!local_to_viewport.IsFinite() || !local_to_viewport.IsInvertible()) {
        return false;
    }
    if (!IsFinite(opacity) || opacity < 0.0F || opacity > 1.0F) {
        return false;
    }
    return std::all_of(clip_chain.begin(), clip_chain.end(), [](const ClipNode& clip) {
        return clip.IsValid();
    });
}

RectF MaterialDescriptor::EffectiveClipRect() const
{
    RectF effective = viewport_rect;
    for (const ClipNode& clip : clip_chain) {
        effective = effective.Intersection(clip.transform_to_viewport.MapRect(clip.rect));
        if (effective.IsEmpty()) {
            break;
        }
    }
    return effective;
}

RectF MaterialDescriptor::VisibleRect(const RectF& viewport) const
{
    if (HasFlag(flags, DescriptorFlag::kOccluded) || opacity <= 0.0F) {
        return {viewport_rect.x, viewport_rect.y, 0.0F, 0.0F};
    }
    return EffectiveClipRect().Intersection(viewport);
}

float MaterialDescriptor::VisibleArea(const RectF& viewport) const
{
    return VisibleRect(viewport).Area();
}

bool MaterialDescriptor::IsVisible(const RectF& viewport) const
{
    return VisibleArea(viewport) > 0.0F;
}

bool MaterialDescriptor::NearlyEquals(const MaterialDescriptor& other, float epsilon) const
{
    return element_id == other.element_id && document_id == other.document_id &&
        role == other.role && viewport_rect.NearlyEquals(other.viewport_rect, epsilon) &&
        corner_radii.NearlyEquals(other.corner_radii, epsilon) &&
        local_to_viewport.NearlyEquals(other.local_to_viewport, epsilon) &&
        ClipChainsEqual(clip_chain, other.clip_chain, epsilon) &&
        NearlyEqual(opacity, other.opacity, epsilon) && z_order == other.z_order &&
        color_scheme == other.color_scheme && contrast == other.contrast &&
        power_preference == other.power_preference && trust_level == other.trust_level &&
        flags == other.flags && source_revision == other.source_revision &&
        debug_name == other.debug_name;
}

std::string MaterialDescriptor::DebugString() const
{
    std::ostringstream stream;
    stream << "MaterialDescriptor{id=" << element_id
           << ", document=" << document_id
           << ", role=" << ToString(role)
           << ", rect=[" << viewport_rect.x << ',' << viewport_rect.y << ','
           << viewport_rect.width << ',' << viewport_rect.height << ']'
           << ", opacity=" << opacity
           << ", z=" << z_order
           << ", clips=" << clip_chain.size()
           << ", revision=" << source_revision
           << '}';
    return stream.str();
}

ChangedField operator|(ChangedField lhs, ChangedField rhs)
{
    return static_cast<ChangedField>(EnumValue(lhs) | EnumValue(rhs));
}

ChangedField& operator|=(ChangedField& lhs, ChangedField rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

bool HasField(ChangedField fields, ChangedField field)
{
    return (EnumValue(fields) & EnumValue(field)) != 0U;
}

ChangedField CompareDescriptors(const MaterialDescriptor& before,
    const MaterialDescriptor& after,
    float epsilon)
{
    if (before.element_id != after.element_id || before.document_id != after.document_id) {
        return ChangedField::kAll;
    }

    ChangedField changed = ChangedField::kNone;
    if (before.role != after.role) {
        changed |= ChangedField::kRole;
    }
    if (!before.viewport_rect.NearlyEquals(after.viewport_rect, epsilon) ||
        !before.corner_radii.NearlyEquals(after.corner_radii, epsilon) ||
        !before.local_to_viewport.NearlyEquals(after.local_to_viewport, epsilon)) {
        changed |= ChangedField::kGeometry;
    }
    if (!ClipChainsEqual(before.clip_chain, after.clip_chain, epsilon)) {
        changed |= ChangedField::kClip;
    }
    if (!NearlyEqual(before.opacity, after.opacity, epsilon)) {
        changed |= ChangedField::kOpacity;
    }
    if (before.z_order != after.z_order) {
        changed |= ChangedField::kStacking;
    }
    if (before.color_scheme != after.color_scheme || before.contrast != after.contrast ||
        before.power_preference != after.power_preference) {
        changed |= ChangedField::kAppearance;
    }
    if (before.flags != after.flags) {
        changed |= ChangedField::kFlags;
    }
    if (before.trust_level != after.trust_level ||
        before.source_revision != after.source_revision ||
        before.debug_name != after.debug_name) {
        changed |= ChangedField::kMetadata;
    }
    return changed;
}

MaterialUpdate MaterialUpdate::Add(MaterialDescriptor descriptor)
{
    MaterialUpdate update;
    update.type = UpdateType::kAdd;
    update.element_id = descriptor.element_id;
    update.document_id = descriptor.document_id;
    update.changed_fields = ChangedField::kAll;
    update.descriptor = std::move(descriptor);
    return update;
}

MaterialUpdate MaterialUpdate::Modify(MaterialDescriptor descriptor, ChangedField fields)
{
    MaterialUpdate update;
    update.type = UpdateType::kModify;
    update.element_id = descriptor.element_id;
    update.document_id = descriptor.document_id;
    update.changed_fields = fields;
    update.descriptor = std::move(descriptor);
    return update;
}

MaterialUpdate MaterialUpdate::Remove(DocumentId document_id, ElementId element_id)
{
    MaterialUpdate update;
    update.type = UpdateType::kRemove;
    update.element_id = element_id;
    update.document_id = document_id;
    return update;
}

MaterialUpdate MaterialUpdate::Clear(DocumentId document_id)
{
    MaterialUpdate update;
    update.type = UpdateType::kClearDocument;
    update.document_id = document_id;
    return update;
}

bool MaterialUpdate::IsValid() const
{
    if (document_id == 0U) {
        return false;
    }
    switch (type) {
        case UpdateType::kAdd:
            return element_id != 0U && descriptor.has_value() &&
                descriptor->element_id == element_id &&
                descriptor->document_id == document_id &&
                descriptor->IsStructurallyValid();
        case UpdateType::kModify:
            return element_id != 0U && descriptor.has_value() &&
                changed_fields != ChangedField::kNone &&
                descriptor->element_id == element_id &&
                descriptor->document_id == document_id &&
                descriptor->IsStructurallyValid();
        case UpdateType::kRemove:
            return element_id != 0U && !descriptor.has_value();
        case UpdateType::kClearDocument:
            return element_id == 0U && !descriptor.has_value();
    }
    return false;
}

bool MaterialBatch::IsValid() const
{
    if (major_version != kMaterialProtocolMajorVersion || document_id == 0U ||
        batch_sequence == 0U) {
        return false;
    }
    return std::all_of(updates.begin(), updates.end(), [this](const MaterialUpdate& update) {
        return update.document_id == document_id && update.IsValid();
    });
}

std::size_t MaterialBatch::AddedCount() const
{
    return static_cast<std::size_t>(std::count_if(updates.begin(), updates.end(),
        [](const MaterialUpdate& update) { return update.type == UpdateType::kAdd; }));
}

std::size_t MaterialBatch::ModifiedCount() const
{
    return static_cast<std::size_t>(std::count_if(updates.begin(), updates.end(),
        [](const MaterialUpdate& update) { return update.type == UpdateType::kModify; }));
}

std::size_t MaterialBatch::RemovedCount() const
{
    return static_cast<std::size_t>(std::count_if(updates.begin(), updates.end(),
        [](const MaterialUpdate& update) { return update.type == UpdateType::kRemove; }));
}

bool MaterialBatch::IsEmpty() const
{
    return updates.empty();
}

}  // namespace arkweb::material
