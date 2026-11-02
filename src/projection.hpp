#pragma once
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace projection {

struct Vec3 { double x=0, y=0, z=0; };

// Camera-to-world rotation, row-major. Columns are right, up and back (-look).
// Input/output pixels use the full render viewport, with no GUI inset applied.
// The caller must establish how that viewport maps to the Windows client area.
struct Camera {
 Vec3 position{};
 std::array<double,9> rotation{1,0,0, 0,1,0, 0,0,1};
 double verticalFovRadians=0;
 int width=0,height=0;
};
struct ScreenPoint { double x,y,depth; };
struct Ray { Vec3 origin,direction; };

inline bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 subtract(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 scale(Vec3 a,double k){return {a.x*k,a.y*k,a.z*k};}
inline Vec3 column(const std::array<double,9>& r,int i){return {r[i],r[i+3],r[i+6]};}

inline bool validRotation(const std::array<double,9>& r){
 for(double v:r)if(!std::isfinite(v))return false;
 for(int i=0;i<3;++i){
  if(std::abs(dot(column(r,i),column(r,i))-1)>1e-3)return false;
  for(int j=i+1;j<3;++j)if(std::abs(dot(column(r,i),column(r,j)))>1e-3)return false;
 }
 const double determinant=r[0]*(r[4]*r[8]-r[5]*r[7])-r[1]*(r[3]*r[8]-r[5]*r[6])+r[2]*(r[3]*r[7]-r[4]*r[6]);
 return std::isfinite(determinant)&&std::abs(determinant-1)<=2e-3;
}

inline bool validCamera(const Camera& camera){
 return finite(camera.position)&&camera.width>0&&camera.height>0&&std::isfinite(camera.verticalFovRadians)&&
        camera.verticalFovRadians>1e-6&&camera.verticalFovRadians<std::numbers::pi-1e-6&&validRotation(camera.rotation);
}

inline bool inside(const Camera& camera,double x,double y){
 return std::isfinite(x)&&std::isfinite(y)&&x>=0&&y>=0&&x<camera.width&&y<camera.height;
}

// Visibility here means only the perspective frustum, never occlusion or valid
// unit placement. Re-read the camera before acting if it might have moved.
inline std::optional<ScreenPoint> project(const Camera& camera,Vec3 world){
 if(!validCamera(camera)||!finite(world))return std::nullopt;
 const Vec3 delta=subtract(world,camera.position);
 const double depth=-dot(column(camera.rotation,2),delta);
 if(!std::isfinite(depth)||depth<=1e-6)return std::nullopt;
 const double focal=static_cast<double>(camera.height)/(2*std::tan(camera.verticalFovRadians/2));
 const double x=static_cast<double>(camera.width)/2+dot(column(camera.rotation,0),delta)*focal/depth;
 const double y=static_cast<double>(camera.height)/2-dot(column(camera.rotation,1),delta)*focal/depth;
 if(!inside(camera,x,y))return std::nullopt;
 return ScreenPoint{x,y,depth};
}

inline std::optional<Ray> ray(const Camera& camera,double pixelX,double pixelY){
 if(!validCamera(camera)||!inside(camera,pixelX,pixelY))return std::nullopt;
 const double focal=static_cast<double>(camera.height)/(2*std::tan(camera.verticalFovRadians/2));
 const Vec3 local{(pixelX-static_cast<double>(camera.width)/2)/focal,
                  (static_cast<double>(camera.height)/2-pixelY)/focal,-1};
 const auto& r=camera.rotation;
 Vec3 direction{r[0]*local.x+r[1]*local.y+r[2]*local.z,
                r[3]*local.x+r[4]*local.y+r[5]*local.z,
                r[6]*local.x+r[7]*local.y+r[8]*local.z};
 const double length=std::sqrt(dot(direction,direction));
 if(!std::isfinite(length)||length<=1e-12)return std::nullopt;
 direction=scale(direction,1/length);
 if(!finite(direction))return std::nullopt;
 return Ray{camera.position,direction};
}

// worldY is the clicked surface plane, NOT the unit HumanoidRootPart height.
// This does not test map bounds, mesh obstacles, or placement exclusions.
inline std::optional<Vec3> intersectHorizontal(const Ray& ray,double worldY){
 if(!finite(ray.origin)||!finite(ray.direction)||!std::isfinite(worldY)||std::abs(ray.direction.y)<=1e-10)return std::nullopt;
 const double distance=(worldY-ray.origin.y)/ray.direction.y;
 if(!std::isfinite(distance)||distance<0)return std::nullopt;
 Vec3 point=add(ray.origin,scale(ray.direction,distance));
 if(!finite(point))return std::nullopt;
 point.y=worldY;
 return point;
}

} // namespace projection
