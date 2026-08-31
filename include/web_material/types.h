#ifndef ARKWEB_WEB_MATERIAL_TYPES_H_
#define ARKWEB_WEB_MATERIAL_TYPES_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arkweb::material {

constexpr float kGeometryEpsilon = 0.0001F;

bool NearlyEqual(float lhs, float rhs, float epsilon = kGeometryEpsilon);
bool IsFinite(float value);

struct PointF {
    float x = 0.0F;
    float y = 0.0F;

    PointF operator+(const PointF& other) const;
    PointF operator-(const PointF& other) const;
    PointF operator*(float scale) const;
    bool NearlyEquals(const PointF& other, float epsilon = kGeometryEpsilon) const;
    bool IsFinite() const;
};

struct SizeF {
    float width = 0.0F;
    float height = 0.0F;

    bool IsEmpty() const;
    bool IsFinite() const;
    float Area() const;
    bool NearlyEquals(const SizeF& other, float epsilon = kGeometryEpsilon) const;
};

struct InsetsF {
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
    float left = 0.0F;

    bool IsFinite() const;
    bool IsNonNegative() const;
    float Horizontal() const;
    float Vertical() const;
};

struct RectF {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;

    float Left() const;
    float Top() const;
    float Right() const;
    float Bottom() const;
    PointF Origin() const;
    PointF Center() const;
    SizeF Size() const;
    float Area() const;
    bool IsEmpty() const;
    bool IsFinite() const;
    bool Contains(const PointF& point) const;
    bool Contains(const RectF& rect) const;
    bool Intersects(const RectF& rect) const;
    RectF Intersection(const RectF& rect) const;
    RectF Union(const RectF& rect) const;
    RectF Translate(const PointF& offset) const;
    RectF Scale(float x_scale, float y_scale) const;
    RectF Inset(const InsetsF& insets) const;
    RectF Expand(const InsetsF& insets) const;
    RectF ClampTo(const RectF& bounds) const;
    bool NearlyEquals(const RectF& other, float epsilon = kGeometryEpsilon) const;
};

struct Matrix3 {
    std::array<float, 9> values = {
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 1.0F,
    };

    static Matrix3 Identity();
    static Matrix3 Translation(float x, float y);
    static Matrix3 Scale(float x, float y);
    static Matrix3 RotationDegrees(float degrees);

    float At(std::size_t row, std::size_t column) const;
    PointF MapPoint(const PointF& point) const;
    RectF MapRect(const RectF& rect) const;
    Matrix3 operator*(const Matrix3& other) const;
    float Determinant() const;
    bool IsInvertible() const;
    bool IsFinite() const;
    bool IsAffine() const;
    bool PreservesAxisAlignment(float epsilon = kGeometryEpsilon) const;
    std::optional<Matrix3> Inverse() const;
    bool NearlyEquals(const Matrix3& other, float epsilon = kGeometryEpsilon) const;
};

struct CornerRadii {
    float top_left = 0.0F;
    float top_right = 0.0F;
    float bottom_right = 0.0F;
    float bottom_left = 0.0F;

    static CornerRadii Uniform(float radius);

    bool IsFinite() const;
    bool IsNonNegative() const;
    bool IsZero() const;
    CornerRadii Scale(float scale) const;
    CornerRadii ClampTo(const SizeF& size) const;
    bool NearlyEquals(const CornerRadii& other, float epsilon = kGeometryEpsilon) const;
};

enum class MaterialRole : std::uint8_t {
    kNone = 0,
    kNavigation = 1,
    kToolbar = 2,
    kFloatingCard = 3,
    kDialog = 4,
    kSheet = 5,
    kControl = 6,
    kStatus = 7,
};

enum class ColorScheme : std::uint8_t {
    kSystem = 0,
    kLight = 1,
    kDark = 2,
};

enum class ContrastPreference : std::uint8_t {
    kSystem = 0,
    kNormal = 1,
    kHigh = 2,
};

enum class PowerPreference : std::uint8_t {
    kDefault = 0,
    kLowPower = 1,
    kHighQuality = 2,
};

enum class TrustLevel : std::uint8_t {
    kUntrusted = 0,
    kApplication = 1,
    kSystem = 2,
};

std::string_view ToString(MaterialRole role);
std::string_view ToString(ColorScheme scheme);
std::string_view ToString(ContrastPreference preference);
std::string_view ToString(PowerPreference preference);
std::string_view ToString(TrustLevel trust_level);

std::optional<MaterialRole> ParseMaterialRole(std::string_view text);
std::optional<ColorScheme> ParseColorScheme(std::string_view text);
std::optional<PowerPreference> ParsePowerPreference(std::string_view text);

}  // namespace arkweb::material

#endif  // ARKWEB_WEB_MATERIAL_TYPES_H_
