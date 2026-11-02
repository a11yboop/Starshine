#pragma once
#include "road_navigation.hpp"
inline void prepareTopDownCamera(Reader& reader,HWND window,InputCapture& control,const std::function<bool()>& guard){
 InputSender input;auto pause=[&](int ms){for(int i=0;i<ms;i+=10){if(control.stopRequested())throw std::runtime_error("Camera setup stopped by F8");guard();std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
 guard();auto camera=reader.camera(window);int x=camera.width/2,y=camera.height/2;
 input.hover(x,y,window,guard);
 INPUT wheel{};wheel.type=INPUT_MOUSE;wheel.mi.dwFlags=MOUSEEVENTF_WHEEL;wheel.mi.mouseData=static_cast<DWORD>(-WHEEL_DELTA*18);
 if(SendInput(1,&wheel,sizeof(wheel))!=1)throw std::runtime_error("Camera zoom failed");pause(200);
 input.send({{"type","mouse"},{"message",WM_RBUTTONDOWN},{"x",x},{"y",y},{"data",0}},window,guard);
 int sign=1;double before=projection::column(reader.camera(window).rotation,2).y;
 for(int i=0;i<24&&before<.94;++i){guard();INPUT move{};move.type=INPUT_MOUSE;move.mi.dy=sign*35;move.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_MOVE_NOCOALESCE;if(SendInput(1,&move,sizeof(move))!=1)throw std::runtime_error("Camera pitch failed");pause(35);double after=projection::column(reader.camera(window).rotation,2).y;if(i==0&&after<before)sign=-1;before=after;}
 input.release();pause(200);
 std::cout<<"Camera overview ready; visible targets no longer require walking\n"<<std::flush;
}
// Character coordinates are transient control feedback only, never logged.
inline void approachWorldTarget(Reader& reader,HWND window,InputCapture& control,projection::Vec3 target,const std::function<bool()>& guard,const json& geometry){
 InputSender input;auto deadline=Clock::now()+std::chrono::seconds(90);int stuck=0;
 auto pause=[&](int ms){auto end=Clock::now()+std::chrono::milliseconds(ms);while(Clock::now()<end){if(control.stopRequested())throw std::runtime_error("Navigation stopped by F8");guard();std::this_thread::sleep_for(std::chrono::milliseconds(5));}};
 std::vector<placement_geometry::Box> boxes;
 for(auto& o:geometry.at("objects"))if(o.at("path").get<std::string>().starts_with("_road/")){
  auto p=o.at("position").get<std::array<double,3>>(),s=o.at("size").get<std::array<double,3>>();boxes.push_back({{p[0],p[1],p[2]},{s[0],s[1],s[2]},o.at("rotation").get<std::array<double,9>>()});
 }
 auto road=road_navigation::segments(boxes);std::vector<projection::Vec3> route;size_t next=0;std::set<WORD> held;
 while(Clock::now()<deadline){
  guard();auto pos=reader.navigationPosition();auto camera=reader.camera(window);auto pixel=projection::project(camera,target);
  if(pixel&&pixel->x>60&&pixel->x<camera.width-60&&pixel->y>120&&pixel->y<camera.height-180)return;
  if(route.empty())route=road_navigation::route(road,pos,target);
  if(!road_navigation::onRoad(road,pos))throw std::runtime_error("Navigation left the enemy road; stopped");
  while(next<route.size()&&road_navigation::distance(pos,route[next])<.65)++next;
  if(next==route.size())throw std::runtime_error("Reached road beside target but target is not visible; adjust camera");
  size_t aim=next;while(aim+1<route.size()&&road_navigation::distance(pos,route[aim+1])<3&&road_navigation::covered(road,pos,route[aim+1]))++aim;
  auto right=projection::column(camera.rotation,0);double length=std::hypot(right.x,right.z);
  if(length<.05)throw std::runtime_error("Camera heading unavailable for road navigation");right={right.x/length,0,right.z/length};
  // Derive horizontal forward from right even with an almost vertical camera.
  projection::Vec3 forward{right.z,0,-right.x};
  std::set<WORD> chosen;double best=road_navigation::distance(pos,route[aim]);
  for(int f=-1;f<=1;++f)for(int side=-1;side<=1;++side){if(!f&&!side)continue;
   auto dir=projection::add(projection::scale(forward,f),projection::scale(right,side));dir=projection::scale(dir,1/std::hypot(dir.x,dir.z));
   auto predicted=projection::add(pos,projection::scale(dir,.7));double d=road_navigation::distance(predicted,route[aim]);
   if(d<best&&road_navigation::covered(road,pos,predicted)){best=d;chosen.clear();if(f)chosen.insert(f>0?'W':'S');if(side)chosen.insert(side>0?'D':'A');}
  }
  if(chosen.empty())throw std::runtime_error("No movement step stays on the enemy road; stopped");
  for(auto key:held)if(!chosen.contains(key))input.send({{"type","key"},{"vk",key},{"down",false}},window,guard);
  for(auto key:chosen)if(!held.contains(key))input.send({{"type","key"},{"vk",key},{"down",true}},window,guard);
  held=chosen;pause(35);
  auto after=reader.navigationPosition();if(!road_navigation::onRoad(road,after))throw std::runtime_error("Road deviation detected; stopped");
  if(road_navigation::distance(after,pos)<.05){if(++stuck>=12)throw std::runtime_error("Enemy road is obstructed; stopped");}else stuck=0;
 }
 throw std::runtime_error("Road navigation timed out");
}
