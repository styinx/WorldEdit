#pragma once

#include "matrix_funcs.hpp"
#include "types.hpp"
#include "vector_funcs.hpp"

#include <cmath>

namespace we {

constexpr auto operator*(const quaternion& quat, const float3& vec) noexcept -> float3
{
   const float3 u{quat.x, quat.y, quat.z};
   const float s = quat.w;

   return 2.0f * dot(u, vec) * u + (s * s - dot(u, u)) * vec +
          (2.0f * s * cross(u, vec));
}

constexpr auto operator*(const quaternion& a, const quaternion& b) noexcept -> quaternion
{
   return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
           a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
           a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
           a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x};
}

constexpr auto conjugate(const quaternion& quat) noexcept -> quaternion
{
   return {quat.w, -quat.x, -quat.y, -quat.z};
}

inline auto normalize(const quaternion& quat) noexcept -> quaternion
{
   float4 normalized = normalize(float4{quat.x, quat.y, quat.z, quat.w});

   return {normalized.w, normalized.x, normalized.y, normalized.z};
}

inline auto make_quat_from_matrix(const float3x3& matrix) noexcept -> quaternion
{
   quaternion quat;

   quat.w = std::sqrt(1.0f + matrix[0].x + matrix[1].y + matrix[2].z) / 2.0f;
   quat.x = (matrix[1].z - matrix[2].y) / (4.0f * quat.w);
   quat.y = (matrix[2].x - matrix[0].z) / (4.0f * quat.w);
   quat.z = (matrix[0].y - matrix[1].x) / (4.0f * quat.w);

   return quat;
}

inline auto make_quat_from_matrix(const float4x4& matrix) noexcept -> quaternion
{
   return make_quat_from_matrix(float3x3{matrix});
}

inline auto to_matrix(quaternion quat) noexcept -> float4x4
{
   quat = normalize(quat);

   float4 quat_sq{quat.x * quat.x, quat.y * quat.y, quat.z * quat.z,
                  quat.w * quat.w};

   const float3 row0 = {1.0f - 2.0f * quat_sq.y - 2.0f * quat_sq.z,
                        2.0f * quat.x * quat.y + 2.0f * quat.z * quat.w,
                        2.0f * quat.x * quat.z - 2.0f * quat.y * quat.w};

   const float3 row1 = {2.0f * quat.x * quat.y - 2.0f * quat.z * quat.w,
                        1.0f - 2.0f * quat_sq.x - 2.0f * quat_sq.z,
                        2.0f * quat.y * quat.z + 2.0f * quat.x * quat.w};

   const float3 row2 = {2.0f * quat.x * quat.z + 2.0f * quat.y * quat.w,
                        2.0f * quat.y * quat.z - 2.0f * quat.x * quat.w,
                        1.0f - 2.0f * quat_sq.x - 2.0f * quat_sq.y};

   return float4x4{{row0, 0.0f}, {row1, 0.0f}, {row2, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
}

}