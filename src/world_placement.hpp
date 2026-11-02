#pragma once

inline projection::Vec3 worldVector(const json& values){
 auto a=values.get<std::array<double,3>>();projection::Vec3 result{a[0],a[1],a[2]};
 if(!projection::finite(result))throw std::runtime_error("Invalid world position");return result;
}
inline projection::Vec3 placementGround(const json& geometry,projection::Vec3 target){
 std::optional<double> highest;
 for(const auto& object:geometry.at("objects")){
  auto path=object.at("path").get<std::string>();if(!path.starts_with("_terrain/ground/")&&path!="_map/bottom")continue;
  placement_geometry::Box box{worldVector(object.at("position")),worldVector(object.at("size")),object.at("rotation").get<std::array<double,9>>()};
  auto top=placement_geometry::supportTop(box,target.x,target.z,target.y);
  if(top&&target.y-*top>=0.1&&target.y-*top<=8&&(!highest||*top>*highest))highest=top;
 }
 if(!highest)throw std::runtime_error("No supporting ground estimate for recorded world position");return {target.x,*highest,target.z};
}
inline void placeWorldUnit(Reader& reader,HWND window,InputCapture& control,const json& event,const json& geometry,const json& aliases,const fs::path& reportFile){
 auto unitName=event.at("unitName").get<std::string>();auto target=worldVector(event.at("worldPosition"));auto ground=placementGround(geometry,target);
 auto place=geometry.at("placeId").get<Address>();InputSender sender;json report={{"unitName",unitName},{"targetRoot",event.at("worldPosition")},{"estimatedGround",{ground.x,ground.y,ground.z}},{"state","preflight"},{"playerPositionRecorded",false}};
 auto save=[&](){std::ofstream f(reportFile);f.exceptions(std::ios::badbit|std::ios::failbit);f<<report.dump(2);};save();
 auto guard=[&](){
  if(control.stopRequested())throw std::runtime_error("World placement stopped by F8");control.drain();
  if(GetForegroundWindow()!=window)throw std::runtime_error("World placement stopped: Roblox lost focus");
  auto state=reader.matchState();if(state.place!=place)throw std::runtime_error("World placement stopped: map changed");
  if(reader.visibleUiIdentity({"ResultsUI","Holder"})||reader.visibleUiIdentity({"ResultsUI","Finished"}))throw std::runtime_error("Match has ended");return true;
 };
 auto pause=[&](int ms){auto until=Clock::now()+std::chrono::milliseconds(ms);while(Clock::now()<until){guard();std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
 auto key=[&](WORD vk){sender.send({{"type","key"},{"vk",vk},{"down",true}},window,guard);pause(100);sender.send({{"type","key"},{"vk",vk},{"down",false}},window,guard);};
 try{
  guard();auto before=reader.snapshot();
  for(const auto& [address,unit]:before.units){(void)address;auto distance=std::sqrt(std::pow(unit.position[0]-target.x,2)+std::pow(unit.position[1]-target.y,2)+std::pow(unit.position[2]-target.z,2));
   if(unit.name==unitName&&distance<=0.75){report["state"]="already_present";report["actualRoot"]=unit.position;save();std::cout<<"World target already occupied by the expected unit; no duplicate placed\n"<<std::flush;return;}
  }
  std::string display=event.value("displayName",std::string{});if(display.empty()){if(!aliases.contains(unitName))throw std::runtime_error("No recorded display name for "+unitName);display=aliases.at(unitName).get<std::string>();}
  json selected;for(const auto& card:reader.hotbar())if(card.at("displayName")==display){if(!selected.is_null())throw std::runtime_error("Ambiguous hotbar name: "+display);selected=card;}
  if(selected.is_null())throw std::runtime_error("Unit not equipped: "+display);int slot=selected.at("slot");double cost=selected.at("cost");
  report["hotbar"]=selected;report["state"]="waiting_for_currency";save();std::cout<<"World placement: "<<display<<" resolved from memory to slot "<<slot<<", cost "<<cost<<"\n"<<std::flush;
  auto moneyDeadline=Clock::now()+std::chrono::seconds(60);while(reader.currency()<cost&&Clock::now()<moneyDeadline)pause(100);if(reader.currency()<cost)throw std::runtime_error("Not enough currency for world placement");
  key('Q');pause(100);approachWorldTarget(reader,window,control,ground,guard,geometry);
  auto camera=reader.camera(window);auto pixel=projection::project(camera,ground);if(!pixel)throw std::runtime_error("Recorded world target is outside the current camera view");
  int x=static_cast<int>(std::lround(pixel->x)),y=static_cast<int>(std::lround(pixel->y));
  if(x<30||y<110||x>=camera.width-30||y>=camera.height-160)throw std::runtime_error("World target projects into the UI edge area; adjust camera");
  if(!sender.hover(x,y,window,guard))throw std::runtime_error("World hover cancelled");key(static_cast<WORD>('0'+slot));
  auto previewDeadline=Clock::now()+std::chrono::seconds(2);while(!reader.visibleUiIdentity({"PlacementInfo"})&&Clock::now()<previewDeadline)pause(50);
  if(!reader.visibleUiIdentity({"PlacementInfo"}))throw std::runtime_error("Unit key did not activate placement preview");
  if(!sender.hover(x,y,window,guard))throw std::runtime_error("Placement hover cancelled");pause(250);
  auto stable=[&](){guard();if(!reader.visibleUiIdentity({"PlacementInfo"}))throw std::runtime_error("Placement preview disappeared");auto current=projection::project(reader.camera(window),ground);if(!current||std::hypot(current->x-x,current->y-y)>2)throw std::runtime_error("Camera moved before world placement");return true;};
  stable();report["screenTarget"]={x,y};report["state"]="click_sent";save();
  sender.send({{"type","mouse"},{"message",WM_LBUTTONDOWN},{"x",x},{"y",y},{"data",0}},window,stable,InputSender::CursorMode::Keep);pause(100);
  sender.send({{"type","mouse"},{"message",WM_LBUTTONUP},{"x",x},{"y",y},{"data",0}},window,{},InputSender::CursorMode::Keep);
  auto deadline=Clock::now()+std::chrono::seconds(4);std::string lastReadError;
  while(Clock::now()<deadline){
   guard();std::optional<Snapshot> after;try{after=reader.snapshot();}catch(const std::exception& ex){lastReadError=ex.what();}
   if(after)for(const auto& [address,unit]:after->units)if(!before.units.contains(address)){
    double distance=std::sqrt(std::pow(unit.position[0]-target.x,2)+std::pow(unit.position[1]-target.y,2)+std::pow(unit.position[2]-target.z,2));
    report["actualUnitName"]=unit.name;report["actualRoot"]=unit.position;report["distanceFromTarget"]=distance;
    if(unit.name==unitName&&distance<=0.75){report["state"]="confirmed";save();std::cout<<"World placement confirmed: "<<unit.name<<", error="<<distance<<" studs\n"<<std::flush;return;}
    report["state"]="unexpected_unit_or_position";save();throw std::runtime_error("New unit differs from world target; no second placement attempted");
   }
   pause(50);
  }
  report["lastReadError"]=lastReadError;throw std::runtime_error("World placement was not confirmed by owned-unit memory");
 }catch(const std::exception& ex){report["error"]=ex.what();if(report["state"]!="unexpected_unit_or_position")report["state"]="failed";save();throw;}
}

