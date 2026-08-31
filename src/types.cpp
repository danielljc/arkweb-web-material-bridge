#include "web_material/types.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arkweb::material {
namespace {

constexpr float kPi = 3.14159265358979323846F;

float ClampRadius(float radius, float maximum)
{
    return std::max(0.0F, std::min(radius, maximum));
}

}  // namespace

bool NearlyEqual(float lhs, float rhs, float epsilon)
{
    return std::abs(lhs - rhs) <= epsilon;
}

bool IsFinite(float value)
{
    return std::isfinite(value);
}

PointF PointF::operator+(const PointF& other) const
{
    return {x + other.x, y + other.y};
}

PointF PointF::operator-(const PointF& other) const
{
    return {x - other.x, y - other.y};
}

PointF PointF::operator*(float scale) const
{
    return {x * scale, y * scale};
}

bool PointF::NearlyEquals(const PointF& other, float epsilon) const
{
    return NearlyEqual(x, other.x, epsilon) && NearlyEqual(y, other.y, epsilon);
}

bool PointF::IsFinite() const
{
    return arkweb::material::IsFinite(x) && arkweb::material::IsFinite(y);
}

bool SizeF::IsEmpty() const
{
    return width <= 0.0F || height <= 0.0F;
}

bool SizeF::IsFinite() const
{
    return arkweb::material::IsFinite(width) && arkweb::material::IsFinite(height);
}

float SizeF::Area() const
{
    return IsEmpty() ? 0.0F : width * height;
}

bool SizeF::NearlyEquals(const SizeF& other, float epsilon) const
{
    return NearlyEqual(width, other.width, epsilon) &&
        NearlyEqual(height, other.height, epsilon);
}

bool InsetsF::IsFinite() const
{
    return arkweb::material::IsFinite(top) && arkweb::material::IsFinite(right) &&
        arkweb::material::IsFinite(bottom) && arkweb::material::IsFinite(left);
}

bool InsetsF::IsNonNegative() const
{
    return top >= 0.0F && right >= 0.0F && bottom >= 0.0F && left >= 0.0F;
}

float InsetsF::Horizontal() const
{
    return left + right;
}

float InsetsF::Vertical() const
{
    return top + bottom;
}

float RectF::Left() const
{
    return x;
}

float RectF::Top() const
{
    return y;
}

float RectF::Right() const
{
    return x + width;
}

float RectF::Bottom() const
{
    return y + height;
}

PointF RectF::Origin() const
{
    return {x, y};
}

PointF RectF::Center() const
{
    return {x + width * 0.5F, y + height * 0.5F};
}

SizeF RectF::Size() const
{
    return {width, height};
}

float RectF::Area() const
{
    return IsEmpty() ? 0.0F : width * height;
}

bool RectF::IsEmpty() const
{
    return width <= 0.0F || height <= 0.0F;
}

bool RectF::IsFinite() const
{
    return arkweb::material::IsFinite(x) && arkweb::material::IsFinite(y) &&
        arkweb::material::IsFinite(width) && arkweb::material::IsFinite(height);
}

bool RectF::Contains(const PointF& point) const
{
    return point.x >= Left() && point.x <= Right() &&
        point.y >= Top() && point.y <= Bottom();
}

bool RectF::Contains(const RectF& rect) const
{
    return rect.Left() >= Left() && rect.Right() <= Right() &&
        rect.Top() >= Top() && rect.Bottom() <= Bottom();
}

bool RectF::Intersects(const RectF& rect) const
{
    if (IsEmpty() || rect.IsEmpty()) {
        return false;
    }
    return Left() < rect.Right() && Right() > rect.Left() &&
        Top() < rect.Bottom() && Bottom() > rect.Top();
}

RectF RectF::Intersection(const RectF& rect) const
{
    const float left = std::max(Left(), rect.Left());
    const float top = std::max(Top(), rect.Top());
    const float right = std::min(Right(), rect.Right());
    const float bottom = std::min(Bottom(), rect.Bottom());
    if (right <= left || bottom <= top) {
        return {left, top, 0.0F, 0.0F};
    }
    return {left, top, right - left, bottom - top};
}

RectF RectF::Union(const RectF& rect) const
{
    if (IsEmpty()) {
        return rect;
    }
    if (rect.IsEmpty()) {
        return *this;
    }
    const float left = std::min(Left(), rect.Left());
    const float top = std::min(Top(), rect.Top());
    const float right = std::max(Right(), rect.Right());
    const float bottom = std::max(Bottom(), rect.Bottom());
    return {left, top, right - left, bottom - top};
}

RectF RectF::Translate(const PointF& offset) const
{
    return {x + offset.x, y + offset.y, width, height};
}

RectF RectF::Scale(float x_scale, float y_scale) const
{
    return {x * x_scale, y * y_scale, width * x_scale, height * y_scale};
}

RectF RectF::Inset(const InsetsF& insets) const
{
    const float result_width = std::max(0.0F, width - insets.Horizontal());
    const float result_height = std::max(0.0F, height - insets.Vertical());
    return {x + insets.left, y + insets.top, result_width, result_height};
}

RectF RectF::Expand(const InsetsF& insets) const
{
    return {
        x - insets.left,
        y - insets.top,
        width + insets.Horizontal(),
        height + insets.Vertical(),
    };
}

RectF RectF::ClampTo(const RectF& bounds) const
{
    return Intersection(bounds);
}

bool RectF::NearlyEquals(const RectF& other, float epsilon) const
{
    return NearlyEqual(x, other.x, epsilon) && NearlyEqual(y, other.y, epsilon) &&
        NearlyEqual(width, other.width, epsilon) &&
        NearlyEqual(height, other.height, epsilon);
}

Matrix3 Matrix3::Identity()
{
    return {};
}

Matrix3 Matrix3::Translation(float x, float y)
{
    Matrix3 matrix;
    matrix.values[2] = x;
    matrix.values[5] = y;
    return matrix;
}

Matrix3 Matrix3::Scale(float x, float y)
{
    Matrix3 matrix;
    matrix.values[0] = x;
    matrix.values[4] = y;
    return matrix;
}

Matrix3 Matrix3::RotationDegrees(float degrees)
{
    const float radians = degrees * kPi / 180.0F;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    Matrix3 matrix;
    matrix.values = {
        cosine, -sine, 0.0F,
        sine, cosine, 0.0F,
        0.0F, 0.0F, 1.0F,
    };
    return matrix;
}

float Matrix3::At(std::size_t row, std::size_t column) const
{
    return values[row * 3U + column];
}

PointF Matrix3::MapPoint(const PointF& point) const
{
    const float denominator = At(2U, 0U) * point.x + At(2U, 1U) * point.y + At(2U, 2U);
    if (NearlyEqual(denominator, 0.0F)) {
        const float infinity = std::numeric_limits<float>::infinity();
        return {infinity, infinity};
    }
    const float mapped_x = At(0U, 0U) * point.x + At(0U, 1U) * point.y + At(0U, 2U);
    const float mapped_y = At(1U, 0U) * point.x + At(1U, 1U) * point.y + At(1U, 2U);
    return {mapped_x / denominator, mapped_y / denominator};
}

RectF Matrix3::MapRect(const RectF& rect) const
{
    const std::array<PointF, 4> points = {
        MapPoint({rect.Left(), rect.Top()}),
        MapPoint({rect.Right(), rect.Top()}),
        MapPoint({rect.Right(), rect.Bottom()}),
        MapPoint({rect.Left(), rect.Bottom()}),
    };
    float left = points[0].x;
    float top = points[0].y;
    float right = points[0].x;
    float bottom = points[0].y;
    for (const PointF& point : points) {
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    }
    return {left, top, right - left, bottom - top};
}

Matrix3 Matrix3::operator*(const Matrix3& other) const
{
    Matrix3 result;
    for (std::size_t row = 0; row < 3U; ++row) {
        for (std::size_t column = 0; column < 3U; ++column) {
            result.values[row * 3U + column] =
                At(row, 0U) * other.At(0U, column) +
                At(row, 1U) * other.At(1U, column) +
                At(row, 2U) * other.At(2U, column);
        }
    }
    return result;
}

float Matrix3::Determinant() const
{
    return At(0U, 0U) * (At(1U, 1U) * At(2U, 2U) - At(1U, 2U) * At(2U, 1U)) -
        At(0U, 1U) * (At(1U, 0U) * At(2U, 2U) - At(1U, 2U) * At(2U, 0U)) +
        At(0U, 2U) * (At(1U, 0U) * At(2U, 1U) - At(1U, 1U) * At(2U, 0U));
}

bool Matrix3::IsInvertible() const
{
    return IsFinite() && !NearlyEqual(Determinant(), 0.0F);
}

bool Matrix3::IsFinite() const
{
    return std::all_of(values.begin(), values.end(), [](float value) {
        return arkweb::material::IsFinite(value);
    });
}

bool Matrix3::IsAffine() const
{
    return NearlyEqual(At(2U, 0U), 0.0F) && NearlyEqual(At(2U, 1U), 0.0F) &&
        NearlyEqual(At(2U, 2U), 1.0F);
}

bool Matrix3::PreservesAxisAlignment(float epsilon) const
{
    const bool no_rotation = NearlyEqual(At(0U, 1U), 0.0F, epsilon) &&
        NearlyEqual(At(1U, 0U), 0.0F, epsilon);
    const bool quarter_turn = NearlyEqual(At(0U, 0U), 0.0F, epsilon) &&
        NearlyEqual(At(1U, 1U), 0.0F, epsilon);
    return IsAffine() && (no_rotation || quarter_turn);
}

std::optional<Matrix3> Matrix3::Inverse() const
{
    const float determinant = Determinant();
    if (!IsFinite() || NearlyEqual(determinant, 0.0F)) {
        return std::nullopt;
    }

    Matrix3 inverse;
    inverse.values = {
        At(1U, 1U) * At(2U, 2U) - At(1U, 2U) * At(2U, 1U),
        At(0U, 2U) * At(2U, 1U) - At(0U, 1U) * At(2U, 2U),
        At(0U, 1U) * At(1U, 2U) - At(0U, 2U) * At(1U, 1U),
        At(1U, 2U) * At(2U, 0U) - At(1U, 0U) * At(2U, 2U),
        At(0U, 0U) * At(2U, 2U) - At(0U, 2U) * At(2U, 0U),
        At(0U, 2U) * At(1U, 0U) - At(0U, 0U) * At(1U, 2U),
        At(1U, 0U) * At(2U, 1U) - At(1U, 1U) * At(2U, 0U),
        At(0U, 1U) * At(2U, 0U) - At(0U, 0U) * At(2U, 1U),
        At(0U, 0U) * At(1U, 1U) - At(0U, 1U) * At(1U, 0U),
    };
    for (float& value : inverse.values) {
        value /= determinant;
    }
    return inverse;
}

bool Matrix3::NearlyEquals(const Matrix3& other, float epsilon) const
{
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (!NearlyEqual(values[index], other.values[index], epsilon)) {
            return false;
        }
    }
    return true;
}

CornerRadii CornerRadii::Uniform(float radius)
{
    return {radius, radius, radius, radius};
}

bool CornerRadii::IsFinite() const
{
    return arkweb::material::IsFinite(top_left) && arkweb::material::IsFinite(top_right) &&
        arkweb::material::IsFinite(bottom_right) &&
        arkweb::material::IsFinite(bottom_left);
}

bool CornerRadii::IsNonNegative() const
{
    return top_left >= 0.0F && top_right >= 0.0F &&
        bottom_right >= 0.0F && bottom_left >= 0.0F;
}

bool CornerRadii::IsZero() const
{
    return NearlyEqual(top_left, 0.0F) && NearlyEqual(top_right, 0.0F) &&
        NearlyEqual(bottom_right, 0.0F) && NearlyEqual(bottom_left, 0.0F);
}

CornerRadii CornerRadii::Scale(float scale) const
{
    return {
        top_left * scale,
        top_right * scale,
        bottom_right * scale,
        bottom_left * scale,
    };
}

CornerRadii CornerRadii::ClampTo(const SizeF& size) const
{
    const float maximum = std::max(0.0F, std::min(size.width, size.height) * 0.5F);
    return {
        ClampRadius(top_left, maximum),
        ClampRadius(top_right, maximum),
        ClampRadius(bottom_right, maximum),
        ClampRadius(bottom_left, maximum),
    };
}

bool CornerRadii::NearlyEquals(const CornerRadii& other, float epsilon) const
{
    return NearlyEqual(top_left, other.top_left, epsilon) &&
        NearlyEqual(top_right, other.top_right, epsilon) &&
        NearlyEqual(bottom_right, other.bottom_right, epsilon) &&
        NearlyEqual(bottom_left, other.bottom_left, epsilon);
}

std::string_view ToString(MaterialRole role)
{
    switch (role) {
        case MaterialRole::kNone:
            return "none";
        case MaterialRole::kNavigation:
            return "navigation";
        case MaterialRole::kToolbar:
            return "toolbar";
        case MaterialRole::kFloatingCard:
            return "floating-card";
        case MaterialRole::kDialog:
            return "dialog";
        case MaterialRole::kSheet:
            return "sheet";
        case MaterialRole::kControl:
            return "control";
        case MaterialRole::kStatus:
            return "status";
    }
    return "unknown";
}

std::string_view ToString(ColorScheme scheme)
{
    switch (scheme) {
        case ColorScheme::kSystem:
            return "system";
        case ColorScheme::kLight:
            return "light";
        case ColorScheme::kDark:
            return "dark";
    }
    return "unknown";
}

std::string_view ToString(ContrastPreference preference)
{
    switch (preference) {
        case ContrastPreference::kSystem:
            return "system";
        case ContrastPreference::kNormal:
            return "normal";
        case ContrastPreference::kHigh:
            return "high";
    }
    return "unknown";
}

std::string_view ToString(PowerPreference preference)
{
    switch (preference) {
        case PowerPreference::kDefault:
            return "default";
        case PowerPreference::kLowPower:
            return "low-power";
        case PowerPreference::kHighQuality:
            return "high-quality";
    }
    return "unknown";
}

std::string_view ToString(TrustLevel trust_level)
{
    switch (trust_level) {
        case TrustLevel::kUntrusted:
            return "untrusted";
        case TrustLevel::kApplication:
            return "application";
        case TrustLevel::kSystem:
            return "system";
    }
    return "unknown";
}

std::optional<MaterialRole> ParseMaterialRole(std::string_view text)
{
    if (text == "none") {
        return MaterialRole::kNone;
    }
    if (text == "navigation") {
        return MaterialRole::kNavigation;
    }
    if (text == "toolbar") {
        return MaterialRole::kToolbar;
    }
    if (text == "floating-card") {
        return MaterialRole::kFloatingCard;
    }
    if (text == "dialog") {
        return MaterialRole::kDialog;
    }
    if (text == "sheet") {
        return MaterialRole::kSheet;
    }
    if (text == "control") {
        return MaterialRole::kControl;
    }
    if (text == "status") {
        return MaterialRole::kStatus;
    }
    return std::nullopt;
}

std::optional<ColorScheme> ParseColorScheme(std::string_view text)
{
    if (text == "system") {
        return ColorScheme::kSystem;
    }
    if (text == "light") {
        return ColorScheme::kLight;
    }
    if (text == "dark") {
        return ColorScheme::kDark;
    }
    return std::nullopt;
}

std::optional<PowerPreference> ParsePowerPreference(std::string_view text)
{
    if (text == "default") {
        return PowerPreference::kDefault;
    }
    if (text == "low-power") {
        return PowerPreference::kLowPower;
    }
    if (text == "high-quality") {
        return PowerPreference::kHighQuality;
    }
    return std::nullopt;
}

}  // namespace arkweb::material
