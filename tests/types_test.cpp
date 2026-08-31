#include "test_fixtures.h"
#include "test_framework.h"

#include "web_material/types.h"

#include <cmath>

using namespace arkweb::material;

TEST(PointFTest, ArithmeticAndFiniteChecks)
{
    const PointF first{2.0F, 3.0F};
    const PointF second{-1.0F, 4.0F};
    EXPECT_TRUE((first + second).NearlyEquals({1.0F, 7.0F}));
    EXPECT_TRUE((first - second).NearlyEquals({3.0F, -1.0F}));
    EXPECT_TRUE((first * 2.0F).NearlyEquals({4.0F, 6.0F}));
    EXPECT_TRUE(first.IsFinite());
}

TEST(RectFTest, IntersectionAndUnion)
{
    const RectF first{0.0F, 0.0F, 100.0F, 80.0F};
    const RectF second{60.0F, 30.0F, 100.0F, 80.0F};
    EXPECT_TRUE(first.Intersects(second));
    EXPECT_TRUE(first.Intersection(second).NearlyEquals({60.0F, 30.0F, 40.0F, 50.0F}));
    EXPECT_TRUE(first.Union(second).NearlyEquals({0.0F, 0.0F, 160.0F, 110.0F}));
}

TEST(RectFTest, TouchingEdgesDoNotIntersect)
{
    const RectF first{0.0F, 0.0F, 10.0F, 10.0F};
    const RectF second{10.0F, 0.0F, 10.0F, 10.0F};
    EXPECT_FALSE(first.Intersects(second));
    EXPECT_TRUE(first.Intersection(second).IsEmpty());
}

TEST(RectFTest, InsetClampsNegativeSize)
{
    const RectF rect{0.0F, 0.0F, 10.0F, 10.0F};
    const RectF inset = rect.Inset({8.0F, 8.0F, 8.0F, 8.0F});
    EXPECT_NEAR(inset.width, 0.0F, 0.001F);
    EXPECT_NEAR(inset.height, 0.0F, 0.001F);
}

TEST(Matrix3Test, TranslationScaleAndComposition)
{
    const Matrix3 transform = Matrix3::Translation(10.0F, 20.0F) *
        Matrix3::Scale(2.0F, 3.0F);
    EXPECT_TRUE(transform.MapPoint({5.0F, 4.0F}).NearlyEquals({20.0F, 32.0F}));
    EXPECT_TRUE(transform.IsAffine());
    EXPECT_TRUE(transform.PreservesAxisAlignment());
}

TEST(Matrix3Test, MapsRotatedBoundingBox)
{
    const RectF rect{0.0F, 0.0F, 100.0F, 50.0F};
    const RectF rotated = Matrix3::RotationDegrees(90.0F).MapRect(rect);
    EXPECT_NEAR(rotated.x, -50.0F, 0.001F);
    EXPECT_NEAR(rotated.y, 0.0F, 0.001F);
    EXPECT_NEAR(rotated.width, 50.0F, 0.001F);
    EXPECT_NEAR(rotated.height, 100.0F, 0.001F);
}

TEST(Matrix3Test, InverseRestoresPoint)
{
    const Matrix3 transform = Matrix3::Translation(30.0F, -12.0F) *
        Matrix3::RotationDegrees(25.0F) * Matrix3::Scale(1.5F, 0.75F);
    const auto inverse = transform.Inverse();
    ASSERT_TRUE(inverse.has_value());
    const PointF original{17.0F, 91.0F};
    const PointF round_trip = inverse->MapPoint(transform.MapPoint(original));
    EXPECT_TRUE(round_trip.NearlyEquals(original, 0.001F));
}

TEST(Matrix3Test, SingularMatrixHasNoInverse)
{
    const Matrix3 singular = Matrix3::Scale(0.0F, 1.0F);
    EXPECT_FALSE(singular.IsInvertible());
    EXPECT_FALSE(singular.Inverse().has_value());
}

TEST(CornerRadiiTest, ClampUsesHalfOfShortDimension)
{
    const CornerRadii radii = CornerRadii::Uniform(100.0F).ClampTo({80.0F, 20.0F});
    EXPECT_NEAR(radii.top_left, 10.0F, 0.001F);
    EXPECT_NEAR(radii.bottom_right, 10.0F, 0.001F);
}

TEST(EnumTest, SemanticRolesRoundTrip)
{
    for (MaterialRole role : {MaterialRole::kNone, MaterialRole::kNavigation,
             MaterialRole::kToolbar, MaterialRole::kFloatingCard, MaterialRole::kDialog,
             MaterialRole::kSheet, MaterialRole::kControl, MaterialRole::kStatus}) {
        const auto parsed = ParseMaterialRole(ToString(role));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, role);
    }
    EXPECT_FALSE(ParseMaterialRole("liquid-glass-everywhere").has_value());
}

TEST(DescriptorTest, EffectiveClipIntersectsAllAncestors)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.viewport_rect = {0.0F, 0.0F, 200.0F, 200.0F};
    ClipNode outer = arkweb::material::test::MakeClip(1U);
    outer.rect = {20.0F, 20.0F, 160.0F, 160.0F};
    outer.transform_to_viewport = Matrix3::Identity();
    ClipNode inner = arkweb::material::test::MakeClip(2U);
    inner.rect = {40.0F, 0.0F, 60.0F, 200.0F};
    inner.transform_to_viewport = Matrix3::Identity();
    descriptor.clip_chain = {outer, inner};
    EXPECT_TRUE(descriptor.EffectiveClipRect().NearlyEquals({40.0F, 20.0F, 60.0F, 160.0F}));
}

TEST(DescriptorTest, ComparisonReportsOnlyChangedFields)
{
    const MaterialDescriptor original = arkweb::material::test::MakeDescriptor();
    MaterialDescriptor changed = original;
    changed.viewport_rect.x += 10.0F;
    changed.opacity = 0.8F;
    const ChangedField fields = CompareDescriptors(original, changed);
    EXPECT_TRUE(HasField(fields, ChangedField::kGeometry));
    EXPECT_TRUE(HasField(fields, ChangedField::kOpacity));
    EXPECT_FALSE(HasField(fields, ChangedField::kRole));
}

TEST(DescriptorTest, InvalidOpacityFailsStructuralValidation)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    descriptor.opacity = 1.1F;
    EXPECT_FALSE(descriptor.IsStructurallyValid());
    descriptor.opacity = -0.1F;
    EXPECT_FALSE(descriptor.IsStructurallyValid());
}

TEST(UpdateTest, FactoriesProduceValidUpdates)
{
    MaterialDescriptor descriptor = arkweb::material::test::MakeDescriptor();
    MaterialUpdate add = MaterialUpdate::Add(descriptor);
    add.sequence = 1U;
    EXPECT_TRUE(add.IsValid());
    MaterialUpdate modify = MaterialUpdate::Modify(descriptor, ChangedField::kGeometry);
    modify.sequence = 2U;
    EXPECT_TRUE(modify.IsValid());
    EXPECT_TRUE(MaterialUpdate::Remove(descriptor.document_id, descriptor.element_id).IsValid());
    EXPECT_TRUE(MaterialUpdate::Clear(descriptor.document_id).IsValid());
}
