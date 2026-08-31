#include "test_fixtures.h"
#include "test_framework.h"

#include "web_material/policy.h"

#include <vector>

using namespace arkweb::material;

TEST(PolicyTest, AcceptsValidApplicationDescriptor)
{
    MaterialPolicyEngine engine;
    const PolicyDecision decision = engine.Evaluate(
        arkweb::material::test::MakeContext(),
        arkweb::material::test::MakeDescriptor());
    EXPECT_TRUE(decision.accepted);
    EXPECT_FALSE(decision.use_fallback);
    EXPECT_TRUE(decision.sanitized_descriptor.has_value());
}

TEST(PolicyTest, RejectsUntrustedDocumentByDefault)
{
    DocumentContext context = arkweb::material::test::MakeContext();
    context.trust_level = TrustLevel::kUntrusted;
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.trust_level = TrustLevel::kUntrusted;
    const PolicyDecision decision = MaterialPolicyEngine().Evaluate(context, descriptor);
    EXPECT_FALSE(decision.accepted);
    EXPECT_TRUE(decision.use_fallback);
    ASSERT_TRUE(!decision.issues.empty());
    EXPECT_EQ(decision.issues.front().code, PolicyCode::kUntrustedDocument);
}

TEST(PolicyTest, SystemPolicyAllowsUntrustedAndCrossOrigin)
{
    DocumentContext context = arkweb::material::test::MakeContext();
    context.trust_level = TrustLevel::kUntrusted;
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.trust_level = TrustLevel::kUntrusted;
    descriptor.flags |= DescriptorFlag::kCrossOrigin;
    MaterialPolicyEngine engine(MaterialPolicy::SystemDefault());
    EXPECT_TRUE(engine.Evaluate(context, descriptor).accepted);
}

TEST(PolicyTest, RejectsDocumentMismatch)
{
    const DocumentContext context = arkweb::material::test::MakeContext(10U);
    const MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor(1U, 11U);
    const PolicyDecision decision = MaterialPolicyEngine().Evaluate(context, descriptor);
    EXPECT_FALSE(decision.accepted);
    EXPECT_TRUE(decision.HasError());
}

TEST(PolicyTest, RejectsComplexClipWithoutPermission)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.flags |= DescriptorFlag::kHasComplexClip;
    const PolicyDecision decision = MaterialPolicyEngine().Evaluate(
        arkweb::material::test::MakeContext(), descriptor);
    EXPECT_FALSE(decision.accepted);
    EXPECT_EQ(decision.issues.front().code, PolicyCode::kComplexClipDenied);
}

TEST(PolicyTest, ClampsOversizedRegion)
{
    DocumentContext context = arkweb::material::test::MakeContext();
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.viewport_rect = {-100.0F, -100.0F, 2000.0F, 3000.0F};
    const PolicyDecision decision = MaterialPolicyEngine().Evaluate(context, descriptor);
    ASSERT_TRUE(decision.accepted);
    ASSERT_TRUE(decision.sanitized_descriptor.has_value());
    EXPECT_TRUE(context.viewport.Contains(decision.sanitized_descriptor->viewport_rect));
    EXPECT_TRUE(decision.HasWarning());
}

TEST(PolicyTest, RejectsTinyRegion)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.viewport_rect = {1.0F, 1.0F, 2.0F, 2.0F};
    const PolicyDecision decision = MaterialPolicyEngine().Evaluate(
        arkweb::material::test::MakeContext(), descriptor);
    EXPECT_FALSE(decision.accepted);
}

TEST(PolicyTest, SelectAcceptedPrioritizesHigherZOrder)
{
    MaterialPolicy policy = MaterialPolicy::ApplicationDefault();
    policy.maximum_regions = 1U;
    MaterialPolicyEngine engine(policy);
    MaterialDescriptor low = arkweb::material::test::MakeDescriptor(1U);
    low.z_order = 1;
    MaterialDescriptor high = arkweb::material::test::MakeDescriptor(2U);
    high.z_order = 100;
    PolicySummary summary;
    const auto accepted = engine.SelectAccepted(
        arkweb::material::test::MakeContext(), {low, high}, &summary);
    ASSERT_EQ(accepted.size(), 1U);
    EXPECT_EQ(accepted.front().element_id, high.element_id);
    EXPECT_EQ(summary.accepted, 1U);
    EXPECT_EQ(summary.rejected, 1U);
}

TEST(PolicyTest, DuplicateElementsAreRejected)
{
    const MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    PolicySummary summary;
    const auto accepted = MaterialPolicyEngine().SelectAccepted(
        arkweb::material::test::MakeContext(), {descriptor, descriptor}, &summary);
    EXPECT_EQ(accepted.size(), 1U);
    EXPECT_EQ(summary.rejected, 1U);
    EXPECT_EQ(summary.issue_counts[PolicyCode::kDuplicateElement], 1U);
}

TEST(PolicyTest, PerRoleCountLimitIsEnforced)
{
    MaterialPolicy policy = MaterialPolicy::ApplicationDefault();
    policy.role_limits[MaterialRole::kNavigation].maximum_count = 1U;
    MaterialDescriptor first = arkweb::material::test::MakeDescriptor(1U);
    MaterialDescriptor second = arkweb::material::test::MakeDescriptor(2U);
    second.viewport_rect.y = 300.0F;
    PolicySummary summary;
    const auto accepted = MaterialPolicyEngine(policy).SelectAccepted(
        arkweb::material::test::MakeContext(), {first, second}, &summary);
    EXPECT_EQ(accepted.size(), 1U);
    EXPECT_EQ(summary.issue_counts[PolicyCode::kRoleCountExceeded], 1U);
}

TEST(MaterialBudgetTest, ReleaseReturnsCapacity)
{
    DocumentContext context = arkweb::material::test::MakeContext();
    MaterialPolicy policy = MaterialPolicy::ApplicationDefault();
    policy.maximum_regions = 1U;
    policy.maximum_total_area_ratio = 10.0F;
    MaterialBudget budget(context, policy);
    MaterialDescriptor first = arkweb::material::test::MakeDescriptor(1U);
    MaterialDescriptor second = arkweb::material::test::MakeDescriptor(2U);
    EXPECT_FALSE(budget.TryReserve(first).has_value());
    EXPECT_TRUE(budget.TryReserve(second).has_value());
    budget.Release(first);
    EXPECT_FALSE(budget.TryReserve(second).has_value());
}

TEST(MaterialBudgetTest, ClearResetsAllCounters)
{
    DocumentContext context = arkweb::material::test::MakeContext();
    MaterialPolicy policy = MaterialPolicy::ApplicationDefault();
    MaterialBudget budget(context, policy);
    EXPECT_FALSE(budget.TryReserve(arkweb::material::test::MakeDescriptor()).has_value());
    budget.Clear();
    EXPECT_EQ(budget.RegionCount(), 0U);
    EXPECT_NEAR(budget.ReservedArea(), 0.0F, 0.001F);
}
