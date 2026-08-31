#ifndef ARKWEB_WEB_MATERIAL_POLICY_H_
#define ARKWEB_WEB_MATERIAL_POLICY_H_

#include "web_material/descriptor.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arkweb::material {

enum class PolicyCode {
    kNone = 0,
    kInvalidContext,
    kInvalidDescriptor,
    kDocumentMismatch,
    kRoleNotAllowed,
    kUntrustedDocument,
    kCrossOriginDenied,
    kInvisible,
    kRegionTooSmall,
    kRegionTooLarge,
    kTooManyClips,
    kComplexClipDenied,
    kPerspectiveDenied,
    kOpacityTooLow,
    kRegionCountExceeded,
    kTotalAreaExceeded,
    kRoleCountExceeded,
    kDuplicateElement,
    kStaleRevision,
};

enum class PolicySeverity {
    kInfo = 0,
    kWarning = 1,
    kError = 2,
};

std::string_view ToString(PolicyCode code);
std::string_view ToString(PolicySeverity severity);

struct PolicyIssue {
    PolicyCode code = PolicyCode::kNone;
    PolicySeverity severity = PolicySeverity::kInfo;
    ElementId element_id = 0U;
    std::string message;
};

struct PolicyDecision {
    bool accepted = false;
    bool use_fallback = false;
    std::optional<MaterialDescriptor> sanitized_descriptor;
    std::vector<PolicyIssue> issues;

    bool HasError() const;
    bool HasWarning() const;
};

struct RoleLimit {
    std::size_t maximum_count = 8U;
    float maximum_area_ratio = 0.5F;
};

struct MaterialPolicy {
    bool enabled = true;
    bool allow_untrusted_documents = false;
    bool allow_cross_origin = false;
    bool allow_complex_clips = false;
    bool allow_perspective = false;
    bool clamp_oversized_regions = true;
    bool discard_invisible_regions = true;
    std::size_t maximum_regions = 32U;
    std::size_t maximum_clip_depth = 8U;
    float minimum_region_area = 16.0F;
    float maximum_region_area_ratio = 0.8F;
    float maximum_total_area_ratio = 1.5F;
    float minimum_opacity = 0.01F;
    std::unordered_set<MaterialRole> allowed_roles;
    std::unordered_map<MaterialRole, RoleLimit> role_limits;

    static MaterialPolicy ApplicationDefault();
    static MaterialPolicy SystemDefault();
};

struct PolicySummary {
    std::size_t evaluated = 0U;
    std::size_t accepted = 0U;
    std::size_t rejected = 0U;
    std::size_t fallback = 0U;
    float accepted_visible_area = 0.0F;
    std::unordered_map<PolicyCode, std::size_t> issue_counts;
};

class MaterialPolicyEngine {
public:
    explicit MaterialPolicyEngine(MaterialPolicy policy = MaterialPolicy::ApplicationDefault());

    PolicyDecision Evaluate(const DocumentContext& context,
        const MaterialDescriptor& descriptor) const;

    std::vector<MaterialDescriptor> SelectAccepted(const DocumentContext& context,
        const std::vector<MaterialDescriptor>& descriptors,
        PolicySummary* summary = nullptr) const;

    const MaterialPolicy& Policy() const;
    void SetPolicy(MaterialPolicy policy);

private:
    PolicyDecision EvaluateStructureAndPermissions(const DocumentContext& context,
        const MaterialDescriptor& descriptor) const;
    void ApplyGeometryPolicy(const DocumentContext& context,
        MaterialDescriptor* descriptor,
        PolicyDecision* decision) const;
    bool IsRoleAllowed(MaterialRole role) const;

    MaterialPolicy policy_;
};

class MaterialBudget {
public:
    MaterialBudget(const DocumentContext& context, const MaterialPolicy& policy);

    std::optional<PolicyIssue> TryReserve(const MaterialDescriptor& descriptor);
    void Release(const MaterialDescriptor& descriptor);
    void Clear();

    std::size_t RegionCount() const;
    float ReservedArea() const;
    std::size_t RoleCount(MaterialRole role) const;

private:
    const DocumentContext& context_;
    const MaterialPolicy& policy_;
    std::size_t region_count_ = 0U;
    float reserved_area_ = 0.0F;
    std::unordered_map<MaterialRole, std::size_t> role_counts_;
    std::unordered_map<MaterialRole, float> role_areas_;
};

}  // namespace arkweb::material

#endif  // ARKWEB_WEB_MATERIAL_POLICY_H_
