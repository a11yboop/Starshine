#include "../src/placement_geometry.hpp"
#include <iostream>
#include <stdexcept>

using namespace projection;
using namespace placement_geometry;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void close(double actual,double expected,const char* message){require(std::isfinite(actual)&&std::abs(actual-expected)<1e-7,message);}

int main(){
 try{
  Box box{{10,2,20},{4,2,6}};
  auto hit=intersectBox({{10,2,30},{0,0,-1}},box);
  require(hit.has_value(),"axis aligned hit");close(hit->entry,7,"axis entry");close(hit->exit,13,"axis exit");
  require(!intersectBox({{10,2,30},{0,0,1}},box),"behind ray rejected");
  require(!intersectBox({{13,2,30},{0,0,-1}},box),"parallel outside slab rejected");
  hit=intersectBox({{10,2,20},{0,1,0}},box);
  require(hit.has_value(),"inside origin hit");close(hit->entry,0,"inside entry zero");close(hit->exit,1,"inside exit");
  auto top=supportTop(box,10,20,8);require(top.has_value(),"axis support");close(*top,3,"axis top");
  top=supportTop(box,12,23,8);require(top.has_value(),"corner support");close(*top,3,"corner top");
  require(!supportTop(box,13,20,8),"outside support rejected");
  require(!supportTop(box,10,20,2),"root inside box rejected");
  require(!supportTop(box,10,20,0),"root below box rejected");
  top=supportTop(box,10,20,3);require(top.has_value(),"root on surface");close(*top,3,"same surface height");

  // Rotation about Y, with expected interval derived from local origin z=10.
  const double c=std::sqrt(0.5);
  Box yaw{{10,2,20},{4,2,2},{c,0,c,0,1,0,-c,0,c}};
  hit=intersectBox({{10+10*c,2,20+10*c},{-c,0,-c}},yaw);
  require(hit.has_value(),"yaw ray hit");close(hit->entry,9,"yaw entry");close(hit->exit,11,"yaw exit");
  top=supportTop(yaw,11,19.5,8);require(top.has_value(),"yaw footprint support");close(*top,3,"yaw top");
  require(!supportTop(yaw,11.5,20,8),"point inside AABB but outside OBB rejected");

  // 45 degree Z tilt: vertical ray at x=0 meets local +Y, x=1 meets +X.
  Box tilted{{0,0,0},{4,2,2},{c,-c,0,c,c,0,0,0,1}};
  top=supportTop(tilted,0,0,5);require(top.has_value(),"tilted center support");close(*top,std::sqrt(2.0),"tilted local Y face");
  top=supportTop(tilted,1,0,5);require(top.has_value(),"tilted side support");close(*top,2*std::sqrt(2.0)-1,"tilted local X face");
  require(!supportTop(tilted,3,0,5),"tilted outside rejected");
  require(!supportTop(tilted,0,0,1),"inside tilted box cannot choose bottom");
  hit=intersectBox({{0,5,0},{0,-1,0}},tilted);
  require(hit.has_value(),"tilted vertical hit");close(hit->entry,5-std::sqrt(2.0),"tilted entry");close(hit->exit,5+std::sqrt(2.0),"tilted exit");

  const double nan=std::numeric_limits<double>::quiet_NaN();
  const double infinity=std::numeric_limits<double>::infinity();
  require(!intersectBox({{10,2,30},{0,0,-2}},box),"nonunit direction rejected");
  require(!intersectBox({{10,2,30},{0,0,0}},box),"zero direction rejected");
  require(!intersectBox({{nan,2,30},{0,0,-1}},box),"invalid origin rejected");
  require(!supportTop(box,10,20,nan),"invalid root rejected");
  require(!supportTop(box,infinity,20,8),"invalid X rejected");
  for(int defect=0;defect<7;++defect){
   Box invalid=box;
   if(defect==0)invalid.size.x=0;
   if(defect==1)invalid.size.y=-1;
   if(defect==2)invalid.size.z=infinity;
   if(defect==3)invalid.center.x=nan;
   if(defect==4)invalid.rotation[0]=2;
   if(defect==5)invalid.rotation[0]=-1;
   if(defect==6)invalid.rotation[2]=0.2;
   require(!validBox(invalid),"invalid box rejected");
   require(!intersectBox({{10,2,30},{0,0,-1}},invalid),"invalid box ray rejected");
   require(!supportTop(invalid,10,20,8),"invalid box support rejected");
  }
  std::cout<<"PASS: OBB ray intervals; inside and behind handling; yaw footprint; tilted support faces; invalid geometry rejection\n";
 }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
