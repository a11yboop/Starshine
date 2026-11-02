#include "../src/projection.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace projection;
void require(bool good,const char* name){if(!good)throw std::runtime_error(name);}
void close(double actual,double expected,const char* name,double tolerance=1e-7){require(std::isfinite(actual)&&std::abs(actual-expected)<tolerance,name);}
void close(Vec3 actual,Vec3 expected,const char* name,double tolerance=1e-7){close(actual.x,expected.x,name,tolerance);close(actual.y,expected.y,name,tolerance);close(actual.z,expected.z,name,tolerance);}

int main(){
 try{
  Camera camera{{0,0,0},{1,0,0,0,1,0,0,0,1},std::numbers::pi/2,800,600};
  require(validCamera(camera),"identity camera");
  auto point=project(camera,{0,0,-10});require(point.has_value(),"center visible");
  close(point->x,400,"center x");close(point->y,300,"center y");close(point->depth,10,"axial depth");
  point=project(camera,{5,5,-10});require(point.has_value(),"quadrant visible");
  close(point->x,550,"right x");close(point->y,150,"up y");
  require(!project(camera,{0,0,10}),"behind rejected");
  require(!project(camera,{0,0,0}),"camera plane rejected");
  require(!project(camera,{100,0,-10}),"offscreen x rejected");
  require(!project(camera,{0,20,-10}),"offscreen y rejected");
  auto centerRay=ray(camera,400,300);require(centerRay.has_value(),"center ray");
  close(centerRay->direction,{0,0,-1},"negative Z forward");

  // Camera yawed 90 degrees about +Y: forward points to world -X, right to -Z.
  Camera yaw{{10,20,30},{0,0,1,0,1,0,-1,0,0},std::numbers::pi/2,800,600};
  point=project(yaw,{0,20,30});require(point.has_value(),"yaw center visible");
  close(point->x,400,"yaw center x");close(point->y,300,"yaw center y");
  point=project(yaw,{0,25,25});require(point.has_value(),"yaw quadrant visible");
  close(point->x,550,"yaw right x");close(point->y,150,"yaw up y");
  centerRay=ray(yaw,400,300);close(centerRay->direction,{-1,0,0},"yaw forward");

  // Looking straight down: image top follows world -Z; plane is ground y=0.
  Camera down{{10,100,20},{1,0,0,0,0,1,0,-1,0},std::numbers::pi/2,800,600};
  centerRay=ray(down,400,300);require(centerRay.has_value(),"down ray");
  auto ground=intersectHorizontal(*centerRay,0);require(ground.has_value(),"ground intersection");
  close(*ground,{10,0,20},"known ground position");
  auto rootPlane=intersectHorizontal(*centerRay,3);require(rootPlane.has_value(),"distinct root plane");
  close(rootPlane->y,3,"caller controls surface plane");
  point=project(down,{10,0,10});require(point.has_value(),"down north visible");
  close(point->x,400,"down north x");close(point->y,270,"down north y");

  // Many viewport locations/depths catch transposed basis, Y sign, and aspect errors.
  for(const auto& current:{camera,yaw,down})for(double x:{0.25,100.0,399.5,799.0})for(double y:{0.25,150.0,300.0,599.0}){
   auto worldRay=ray(current,x,y);require(worldRay.has_value(),"ray roundtrip input");
   close(dot(worldRay->direction,worldRay->direction),1,"unit direction");
   for(double distance:{2.0,70.0,10000.0}){
    auto world=add(worldRay->origin,scale(worldRay->direction,distance));
    auto pixel=project(current,world);require(pixel.has_value(),"project roundtrip output");
    close(pixel->x,x,"roundtrip x");close(pixel->y,y,"roundtrip y");
   }
   if(current.position.y==100){
    auto hit=intersectHorizontal(*worldRay,5);require(hit.has_value(),"ground plane ray");
    auto pixel=project(current,*hit);require(pixel.has_value(),"ground roundtrip");
    close(pixel->x,x,"ground roundtrip x");close(pixel->y,y,"ground roundtrip y");
   }
  }

  Camera wide=camera;wide.width=1600;
  point=project(wide,{5,5,-10});close(point->x,950,"aspect ratio uses vertical FOV");close(point->y,150,"height unchanged");
  Camera zoom=camera;zoom.verticalFovRadians=2*std::atan(0.5);
  point=project(zoom,{2.5,2.5,-10});close(point->x,550,"narrower FOV x");close(point->y,150,"narrower FOV y");

  const double nan=std::numeric_limits<double>::quiet_NaN();
  const double infinity=std::numeric_limits<double>::infinity();
  require(!project(camera,{nan,0,-5}),"NaN world rejected");
  require(!ray(camera,nan,300)&&!ray(camera,800,300)&&!ray(camera,400,-1),"invalid pixel rejected");
  require(!intersectHorizontal({{0,2,0},{1,0,0}},0),"parallel ray rejected");
  require(!intersectHorizontal({{0,2,0},{0,1,0}},0),"backwards plane rejected");
  require(!intersectHorizontal({{0,2,0},{0,-1,0}},nan),"invalid plane rejected");
  require(!intersectHorizontal({{0,2,0},{nan,-1,0}},0),"invalid ray rejected");
  require(!intersectHorizontal({{0,2,0},{0,0,0}},0),"zero ray rejected");
  for(int defect=0;defect<9;++defect){
   auto invalid=camera;
   if(defect==0)invalid.width=0;
   if(defect==1)invalid.height=-1;
   if(defect==2)invalid.verticalFovRadians=0;
   if(defect==3)invalid.verticalFovRadians=std::numbers::pi;
   if(defect==4)invalid.position.x=infinity;
   if(defect==5)invalid.rotation[0]=2;
   if(defect==6)invalid.rotation[1]=0.1;
   if(defect==7)invalid.rotation[0]=-1; // Reflection is not a rotation.
   if(defect==8)invalid.rotation[4]=nan;
   require(!validCamera(invalid),"invalid camera rejected");
   require(!project(invalid,{0,0,-10})&&!ray(invalid,400,300),"invalid camera fails closed");
  }
  std::cout<<"PASS: known camera geometry; yaw/down orientation; perspective and ground roundtrips; FOV/aspect; invalid, offscreen and behind rejection\n";
 }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
