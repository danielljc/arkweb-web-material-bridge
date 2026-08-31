#include "web_material/policy.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace arkweb::material {
namespace {

PolicyIssue MakeIssue(PolicyCode code,
    PolicySeverity severity,
    ElementId element_id,
    std::string message)
{
    return {code, severity, element_id, std::move(message)};
}

void AddIssue(PolicyDecision* decision,
    PolicyCode code,
    PolicySeverity severity,
    ElementId element_id,
    std::string message)
{
    decision->issues.push_back(MakeIssue(code, severity, element_id, std::move(message)));
}

float SafeRatio(float numerator, float denominator)
{
    return denominator > 0.0F ? numerator / denominator : 0.0F;
}

}  // namespace

std::string_view ToString(PolicyCode code)
{
    switch (code) {
        case PolicyCode::kNone:
            return "none";
        case PolicyCode::kInvalidContext:
            return "invalid-context";
        case PolicyCode::kInvalidDescriptor:
            return "invalid-descriptor";
        case PolicyCode::kDocumentMismatch:
            return "document-mismatch";
        case PolicyCode::kRoleNotAllowed:
            return "role-not-allowed";
        case PolicyCode::kUntrustedDocument:
            return "untrusted-document";
        case PolicyCode::kCrossOriginDenied:
            return "cross-origin-denied";
        case PolicyCode::kInvisible:
            return "invisible";
        case PolicyCode::kRegionTooSmall:
            return "region-too-small";
        case PolicyCode::kRegionTooLarge:
            return "region-too-large";
        case PolicyCode::kTooManyClips:
            return "too-many-clips";
        case PolicyCode::kComplexClipDenied:
            return "complex-clip-denied";
        case PolicyCode::kPerspectiveDenied:
            return "perspective-denied";
        case PolicyCode::kOpacityTooLow:
            return "opacity-too-low";
        case PolicyCode::kRegionCountExceeded:
            return "region-count-exceeded";
        case PolicyCode::kTotalAreaExceeded:
            return "total-area-exceeded";
        case PolicyCode::kRoleCountExceeded:
            return "role-count-exceeded";
        case PolicyCode::kDuplicateElement:
            return "duplicate-element";
        case PolicyCode::kStaleRevision:
            return "stale-revision";
    }
    return "unknown";
}

std::string_view ToString(PolicySeverity severity)
{
    switch (severity) {
        case PolicySeverity::kInfo:
            return "info";
        case PolicySeverity::kWarning:
            return "warning";
        case PolicySeverity::kError:
            return "error";
    }
    return "unknown";
}

bool PolicyDecision::HasError() const
{
    return std::any_of(issues.begin(), issues.end(), [](const PolicyIssue& issue) {
        return issue.severity == PolicySeverity::kError;
    });
}

bool PolicyDecision::HasWarning() const
{
    return std::any_of(issues.begin(), issues.end(), [](const PolicyIssue& issue) {
        return issue.severity == PolicySeverity::kWarning;
    });
}

MaterialPolicy MaterialPolicy::ApplicationDefault()
{
    MaterialPolicy policy;
    policy.allowed_roles = {
        MaterialRole::kNavigation,
        MaterialRole::kToolbar,
        MaterialRole::kFloatingCard,
        MaterialRole::kDialog,
        MaterialRole::kSheet,
        MaterialRole::kControl,
        MaterialRole::kStatus,
    };
    policy.role_limits = {
        {MaterialRole::kNavigation, {2U, 0.35F}},
        {MaterialRole::kToolbar, {4U, 0.35F}},
        {MaterialRole::kFloatingCard, {12U, 0.8F}},
        {MaterialRole::kDialog, {2U, 0.8F}},
        {MaterialRole::kSheet, {2U, 0.9F}},
        {MaterialRole::kControl, {16U, 0.5F}},
        {MaterialRole::kStatus, {4U, 0.25F}},
    };
    return policy;
}

MaterialPolicy MaterialPolicy::SystemDefault()
{
    MaterialPolicy policy = ApplicationDefault();
    policy.allow_untrusted_documents = true;
    policy.allow_cross_origin = true;
    policy.allow_complex_clips = true;
    policy.maximum_regions = 64U;
    policy.maximum_clip_depth = 16U;
    policy.maximum_total_area_ratio = 2.5F;
    for (auto& [role, limit] : policy.role_limits) {
        static_cast<void>(role);
        limit.maximum_count *= 2U;
        limit.maximum_area_ratio = std::min(1.5F, limit.maximum_area_ratio * 1.5F);
    }
    return policy;
}

MaterialPolicyEngine::MaterialPolicyEngine(MaterialPolicy policy) : policy_(std::move(policy)) {}

PolicyDecision MaterialPolicyEngine::Evaluate(const DocumentContext& context,
    const MaterialDescriptor& descriptor) const
{
    PolicyDecision decision = EvaluateStructureAndPermissions(context, descriptor);
    if (decision.HasError()) {
        decision.accepted = false;
        decision.use_fallback = true;
        return decision;
    }

    decision.sanitized_descriptor = descriptor;
    ApplyGeometryPolicy(context, &*decision.sanitized_descriptor, &decision);
    decision.accepted = !decision.HasError();
    decision.use_fallback = !decision.accepted;
    if (!decision.accepted) {
        decision.sanitized_descriptor.reset();
    }
    return decision;
}

std::vector<MaterialDescriptor> MaterialPolicyEngine::SelectAccepted(
    const DocumentContext& context,
    const std::vector<MaterialDescriptor>& descriptors,
    PolicySummary* summary) const
{
    PolicySummary local_summary;
    MaterialBudget budget(context, policy_);
    std::unordered_set<ElementId> seen_elements;
    std::vector<MaterialDescriptor> accepted;
    accepted.reserve(std::min(descriptors.size(), policy_.maximum_regions));

    std::vector<const MaterialDescriptor*> priority_order;
    priority_order.reserve(descriptors.size());
    for (const MaterialDescriptor& descriptor : descriptors) {
        priority_order.push_back(&descriptor);
    }
    std::stable_sort(priority_order.begin(), priority_order.end(),
        [&context](const MaterialDescriptor* lhs, const MaterialDescriptor* rhs) {
            if (lhs->z_order != rhs->z_order) {
                return lhs->z_order > rhs->z_order;
            }
            return lhs->VisibleArea(context.viewport) > rhs->VisibleArea(context.viewport);
        });

    for (const MaterialDescriptor* candidate : priority_order) {
        ++local_summary.evaluated;
        if (!seen_elements.insert(candidate->element_id).second) {
            ++local_summary.rejected;
            ++local_summary.fallback;
            ++local_summary.issue_counts[PolicyCode::kDuplicateElement];
            continue;
        }

        PolicyDecision decision = Evaluate(context, *candidate);
        for (const PolicyIssue& issue : decision.issues) {
            ++local_summary.issue_counts[issue.code];
        }
        if (!decision.accepted || !decision.sanitized_descriptor.has_value()) {
            ++local_summary.rejected;
            if (decision.use_fallback) {
                ++local_summary.fallback;
            }
            continue;
        }

        std::optional<PolicyIssue> budget_issue = budget.TryReserve(*decision.sanitized_descriptor);
        if (budget_issue.has_value()) {
            ++local_summary.rejected;
            ++local_summary.fallback;
            ++local_summary.issue_counts[budget_issue->code];
            continue;
        }

        local_summary.accepted_visible_area +=
            decision.sanitized_descriptor->VisibleArea(context.viewport);
        ++local_summary.accepted;
        accepted.push_back(std::move(*decision.sanitized_descriptor));
    }

    std::sort(accepted.begin(), accepted.end(), [](const MaterialDescriptor& lhs,
        const MaterialDescriptor& rhs) {
        if (lhs.z_order != rhs.z_order) {
            return lhs.z_order < rhs.z_order;
        }
        return lhs.element_id < rhs.element_id;
    });
    if (summary != nullptr) {
        *summary = std::move(local_summary);
    }
    return accepted;
}

const MaterialPolicy& MaterialPolicyEngine::Policy() const
{
    return policy_;
}

void MaterialPolicyEngine::SetPolicy(MaterialPolicy policy)
{
    policy_ = std::move(policy);
}

PolicyDecision MaterialPolicyEngine::EvaluateStructureAndPermissions(
    const DocumentContext& context,
    const MaterialDescriptor& descriptor) const
{
    PolicyDecision decision;
    if (!policy_.enabled) {
        AddIssue(&decision, PolicyCode::kRoleNotAllowed, PolicySeverity::kError,
            descriptor.element_id, "material bridge is disabled by policy");
        return decision;
    }
    if (!context.IsValid()) {
        AddIssue(&decision, PolicyCode::kInvalidContext, PolicySeverity::kError,
            descriptor.element_id, "document context is incomplete or invalid");
    }
    if (!descriptor.IsStructurallyValid()) {
        AddIssue(&decision, PolicyCode::kInvalidDescriptor, PolicySeverity::kError,
            descriptor.element_id, "descriptor failed structural validation");
    }
    if (descriptor.document_id != context.document_id) {
        AddIssue(&decision, PolicyCode::kDocumentMismatch, PolicySeverity::kError,
            descriptor.element_id, "descriptor belongs to another document");
    }
    if (!IsRoleAllowed(descriptor.role)) {
        AddIssue(&decision, PolicyCode::kRoleNotAllowed, PolicySeverity::kError,
            descriptor.element_id, "semantic role is not enabled");
    }
    if (context.trust_level == TrustLevel::kUntrusted && !policy_.allow_untrusted_documents) {
        AddIssue(&decision, PolicyCode::kUntrustedDocument, PolicySeverity::kError,
            descriptor.element_id, "untrusted document cannot request native material");
    }
    if (HasFlag(descriptor.flags, DescriptorFlag::kCrossOrigin) &&
        !policy_.allow_cross_origin) {
        AddIssue(&decision, PolicyCode::kCrossOriginDenied, PolicySeverity::kError,
            descriptor.element_id, "cross-origin material request is denied");
    }
    if (HasFlag(descriptor.flags, DescriptorFlag::kHasComplexClip) &&
        !policy_.allow_complex_clips) {
        AddIssue(&decision, PolicyCode::kComplexClipDenied, PolicySeverity::kError,
            descriptor.element_id, "complex clip requires CSS fallback");
    }
    if (HasFlag(descriptor.flags, DescriptorFlag::kHasPerspective) &&
        !policy_.allow_perspective) {
        AddIssue(&decision, PolicyCode::kPerspectiveDenied, PolicySeverity::kError,
            descriptor.element_id, "perspective transform requires CSS fallback");
    }
    if (descriptor.clip_chain.size() > policy_.maximum_clip_depth) {
        AddIssue(&decision, PolicyCode::kTooManyClips, PolicySeverity::kError,
            descriptor.element_id, "clip chain exceeds policy depth");
    }
    return decision;
}

void MaterialPolicyEngine::ApplyGeometryPolicy(const DocumentContext& context,
    MaterialDescriptor* descriptor,
    PolicyDecision* decision) const
{
    if (!descriptor->IsVisible(context.viewport)) {
        const PolicySeverity severity = policy_.discard_invisible_regions ?
            PolicySeverity::kError : PolicySeverity::kWarning;
        AddIssue(decision, PolicyCode::kInvisible, severity, descriptor->element_id,
            "material region is outside viewport or fully occluded");
    }
    if (descriptor->opacity < policy_.minimum_opacity) {
        AddIssue(decision, PolicyCode::kOpacityTooLow, PolicySeverity::kError,
            descriptor->element_id, "material opacity is below useful threshold");
    }

    const float viewport_area = context.viewport.Area();
    const float visible_area = descriptor->VisibleArea(context.viewport);
    if (visible_area < policy_.minimum_region_area) {
        AddIssue(decision, PolicyCode::kRegionTooSmall, PolicySeverity::kError,
            descriptor->element_id, "material region is too small to render efficiently");
    }
    if (SafeRatio(visible_area, viewport_area) > policy_.maximum_region_area_ratio) {
        if (policy_.clamp_oversized_regions) {
            descriptor->viewport_rect = descriptor->viewport_rect.Intersection(context.viewport);
            descriptor->corner_radii = descriptor->corner_radii.ClampTo(
                descriptor->viewport_rect.Size());
            AddIssue(decision, PolicyCode::kRegionTooLarge, PolicySeverity::kWarning,
                descriptor->element_id, "oversized material region was clamped to viewport");
        } else {
            AddIssue(decision, PolicyCode::kRegionTooLarge, PolicySeverity::kError,
                descriptor->element_id, "material region exceeds area policy");
        }
    }
}

bool MaterialPolicyEngine::IsRoleAllowed(MaterialRole role) const
{
    return policy_.allowed_roles.find(role) != policy_.allowed_roles.end();
}

MaterialBudget::MaterialBudget(const DocumentContext& context, const MaterialPolicy& policy)
    : context_(context), policy_(policy)
{
}

std::optional<PolicyIssue> MaterialBudget::TryReserve(const MaterialDescriptor& descriptor)
{
    if (region_count_ >= policy_.maximum_regions) {
        return MakeIssue(PolicyCode::kRegionCountExceeded, PolicySeverity::kError,
            descriptor.element_id, "document material region count is exhausted");
    }

    const float area = descriptor.VisibleArea(context_.viewport);
    const float viewport_area = context_.viewport.Area();
    if (SafeRatio(reserved_area_ + area, viewport_area) > policy_.maximum_total_area_ratio) {
        return MakeIssue(PolicyCode::kTotalAreaExceeded, PolicySeverity::kError,
            descriptor.element_id, "document material area budget is exhausted");
    }

    const auto limit_iterator = policy_.role_limits.find(descriptor.role);
    if (limit_iterator != policy_.role_limits.end()) {
        const RoleLimit& limit = limit_iterator->second;
        if (RoleCount(descriptor.role) >= limit.maximum_count) {
            return MakeIssue(PolicyCode::kRoleCountExceeded, PolicySeverity::kError,
                descriptor.element_id, "semantic role count budget is exhausted");
        }
        const float role_area = role_areas_[descriptor.role] + area;
        if (SafeRatio(role_area, viewport_area) > limit.maximum_area_ratio) {
            return MakeIssue(PolicyCode::kTotalAreaExceeded, PolicySeverity::kError,
                descriptor.element_id, "semantic role area budget is exhausted");
        }
    }

    ++region_count_;
    reserved_area_ += area;
    ++role_counts_[descriptor.role];
    role_areas_[descriptor.role] += area;
    return std::nullopt;
}

void MaterialBudget::Release(const MaterialDescriptor& descriptor)
{
    if (region_count_ == 0U) {
        return;
    }
    --region_count_;
    const float area = descriptor.VisibleArea(context_.viewport);
    reserved_area_ = std::max(0.0F, reserved_area_ - area);

    auto count_iterator = role_counts_.find(descriptor.role);
    if (count_iterator != role_counts_.end()) {
        if (count_iterator->second <= 1U) {
            role_counts_.erase(count_iterator);
        } else {
            --count_iterator->second;
        }
    }
    auto area_iterator = role_areas_.find(descriptor.role);
    if (area_iterator != role_areas_.end()) {
        area_iterator->second = std::max(0.0F, area_iterator->second - area);
        if (NearlyEqual(area_iterator->second, 0.0F)) {
            role_areas_.erase(area_iterator);
        }
    }
}

void MaterialBudget::Clear()
{
    region_count_ = 0U;
    reserved_area_ = 0.0F;
    role_counts_.clear();
    role_areas_.clear();
}

std::size_t MaterialBudget::RegionCount() const
{
    return region_count_;
}

float MaterialBudget::ReservedArea() const
{
    return reserved_area_;
}

std::size_t MaterialBudget::RoleCount(MaterialRole role) const
{
    const auto iterator = role_counts_.find(role);
    return iterator == role_counts_.end() ? 0U : iterator->second;
}

}  // namespace arkweb::material
