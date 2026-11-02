#pragma once
#include "projection.hpp"
#include <algorithm>
#include <limits>

namespace placement_geometry {

// An oriented bounding box, not a mesh/collision/placement permission model.
// rotation is row-major local-to-world, with the same convention as Camera.
struct Box {
 projection::Vec3 center{},size{};
 std::array<double,9> rotation{1,0,0, 0,1,0, 0,0,1};
};
struct BoxHit { double entry,exit; };

inline bool validBox(const Box& box){
 return projection::finite(box.center)&&projection::finite(box.size)&&box.size.x>0&&box.size.y>0&&box.size.z>0&&projection::validRotation(box.rotation);
}

namespace detail {
// Unclipped signed distances along a unit ray. Negative entry means the origin
// is already inside the box; retaining it is essential for supportTop.
inline std::optional<BoxHit> lineBox(const projection::Ray& ray,const Box& box){
 if(!validBox(box)||!projection::finite(ray.origin)||!projection::finite(ray.direction))return std::nullopt;
 const double magnitudeSquared=projection::dot(ray.direction,ray.direction);
 if(!std::isfinite(magnitudeSquared)||std::abs(magnitudeSquared-1)>1e-6)return std::nullopt;
 const auto delta=projection::subtract(ray.origin,box.center);
 if(!projection::finite(delta))return std::nullopt;
 const std::array<double,3> half{box.size.x/2,box.size.y/2,box.size.z/2};
 double entry=-std::numeric_limits<double>::infinity(),exit=std::numeric_limits<double>::infinity();
 for(int axis=0;axis<3;++axis){
  const auto basis=projection::column(box.rotation,axis);
  const double origin=projection::dot(basis,delta),direction=projection::dot(basis,ray.direction);
  if(!std::isfinite(origin)||!std::isfinite(direction))return std::nullopt;
  if(std::abs(direction)<=1e-12){
   if(origin< -half[axis]||origin>half[axis])return std::nullopt;
   continue;
  }
  double first=(-half[axis]-origin)/direction,last=(half[axis]-origin)/direction;
  if(!std::isfinite(first)||!std::isfinite(last))return std::nullopt;
  if(first>last)std::swap(first,last);
  entry=std::max(entry,first);exit=std::min(exit,last);
  if(entry>exit)return std::nullopt;
 }
 if(!std::isfinite(entry)||!std::isfinite(exit))return std::nullopt;
 return BoxHit{entry,exit};
}
} // namespace detail

// Ray direction must be unit length. Returned distances are world units/studs.
// An inside origin has entry=0. A box entirely behind the ray is rejected.
inline std::optional<BoxHit> intersectBox(const projection::Ray& ray,const Box& box){
 auto hit=detail::lineBox(ray,box);
 if(!hit||hit->exit<0)return std::nullopt;
 hit->entry=std::max(0.0,hit->entry);
 return hit;
}

// Topmost vertical intersection at (x,z) that is no higher than rootY. A root
// inside/under this box cannot use its lower face as the supporting surface.
// Caller must select appropriate ground objects and validate root-to-floor gap.
inline std::optional<double> supportTop(const Box& box,double x,double z,double rootY){
 if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(rootY))return std::nullopt;
 auto hit=detail::lineBox({{x,rootY,z},{0,-1,0}},box);
 if(!hit||hit->entry< -1e-6||hit->exit<0)return std::nullopt;
 const double top=rootY-std::max(0.0,hit->entry);
 if(!std::isfinite(top))return std::nullopt;
 return top;
}

} // namespace placement_geometry
