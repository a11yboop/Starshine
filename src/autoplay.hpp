#pragma once
#include "autoplay_policy.hpp"
inline bool autoRoadBlocked(const json& geometry,double x,double z,double y){
 bool hasRoad=false;for(const auto& object:geometry.at("objects")){
  auto path=object.at("path").get<std::string>();if(!path.starts_with("_road/"))continue;hasRoad=true;
  placement_geometry::Box box{worldVector(object.at("position")),worldVector(object.at("size")),object.at("rotation").get<std::array<double,9>>()};
  box.size.x+=4;box.size.z+=4;
  if(placement_geometry::supportTop(box,x,z,y+20))return true;
 }return !hasRoad;
}
inline json roadsideCandidates(const json& templates,const json& geometry,const json& enemies,const Snapshot& owned){
 json result=json::array();std::map<std::string,int> blockedBy;
 for(auto candidate:templates){bool exists=false;for(const auto& [a,u]:owned.units){(void)a;if(u.name==candidate.at("unitName").get<std::string>()){exists=true;break;}}if(exists)continue;
  double offset=0;if(candidate.contains("rootHeight"))offset=candidate.at("rootHeight").get<double>();else{const auto reference=worldVector(candidate.at("worldPosition"));offset=reference.y-placementGround(geometry,reference).y;}
  // Sample both sides and bends; forbid the road OBB plus a two-stud margin.
  int sampled=0;for(const auto& enemy:enemies){if(++sampled>64)break;auto p=worldVector(enemy.at("position"));
   for(double radius:{6.0,8.0})for(int angle=0;angle<12;++angle){double t=angle*6.283185307179586/12;projection::Vec3 probe{p.x+radius*std::cos(t),p.y+4,p.z+radius*std::sin(t)};
    try{auto ground=placementGround(geometry,probe);if(autoRoadBlocked(geometry,ground.x,ground.z,ground.y))continue;
     bool nearRoad=false,blocked=false;
     for(const auto& object:geometry.at("objects")){
      auto path=object.at("path").get<std::string>();placement_geometry::Box box{worldVector(object.at("position")),worldVector(object.at("size")),object.at("rotation").get<std::array<double,9>>()};
      if(path.starts_with("_road/")){box.size.x+=10;box.size.z+=10;if(placement_geometry::supportTop(box,ground.x,ground.z,ground.y+20))nearRoad=true;continue;}
      if(path.starts_with("_terrain/ground/")||path=="_map/bottom"||path.find("grass")!=std::string::npos)continue;
      // Decorative mesh bounds may enclose hollow rooms or the entire map.
      // They are not solid collision boxes; placement must still be confirmed.
      if(object.value("class",std::string{})=="MeshPart"||object.value("class",std::string{})=="UnionOperation")continue;
      box.size.x+=2;box.size.z+=2;auto hit=placement_geometry::intersectBox({{ground.x,ground.y+.3,ground.z},{0,1,0}},box);if(hit&&hit->entry<4){blocked=true;++blockedBy[path];break;}
     }
     if(!nearRoad||blocked)continue;candidate["worldPosition"]={ground.x,ground.y+offset,ground.z};candidate["placementSource"]="enemy_roadside";result.push_back(candidate);
    }catch(const std::exception&){}
   }
  }
 }if(result.empty())for(auto& [path,count]:blockedBy)std::cout<<"Surface blocked by "<<path<<": "<<count<<"\n";return result;
}
inline void autoPlay(Reader& r,HWND window,InputCapture& stop,const fs::path& directory){
 auto recording=json::parse(std::ifstream(directory/"recording.json")),geometry=json::parse(std::ifstream(directory/"geometry.json")),aliases=json::parse(std::ifstream("config/unit-names.json"));
 auto place=geometry.at("placeId").get<Address>();auto out=directory/"autoplay";fs::create_directories(out);std::ofstream log(out/"decisions.jsonl",std::ios::app);
 auto guard=[&](){if(stop.stopRequested())throw std::runtime_error("Autoplay stopped by F8");stop.drain();if(GetForegroundWindow()!=window)throw std::runtime_error("Autoplay lost focus");if(r.matchState().place!=place)throw std::runtime_error("Autoplay map changed");};
 auto record=[&](json j){log<<j.dump()<<'\n';log.flush();};
 json candidates=json::array(),farm;auto hotbar=r.hotbar();
 for(auto event:strategyEvents(recording.at("events"))){auto type=event.value("type",std::string{});if(type!="unit_placed"&&type!="unit_discovered_after_gap")continue;auto name=event.at("unitName").get<std::string>();if(!aliases.contains(name))continue;
  // Raised and hill units need a separate placement-surface solver.
  if(name!="bulma_summer:golden"&&name!="homura_summer"&&name!="sakamoto_summer"&&name!="darkness")continue;
  for(auto card:hotbar)if(card.at("displayName")==aliases.at(name)){event["cost"]=card.at("cost");event["displayName"]=card.at("displayName");if(name=="bulma_summer:golden"){if(farm.is_null())farm=event;}else candidates.push_back(event);}
 }
 if(farm.is_null()||candidates.empty())throw std::runtime_error("Autoplay needs a recorded farm location and ground defender locations");
 prepareTopDownCamera(r,window,stop,[&](){guard();return true;});
 if(recording.value("autonomousMap",false)){
  json roadSamples=json::array();for(const auto& object:geometry.at("objects"))if(object.at("path").get<std::string>().starts_with("_road/"))roadSamples.push_back({{"position",object.at("position")}});
  auto sites=roadsideCandidates(json::array({farm}),geometry,roadSamples,Snapshot{place,0,{}});if(sites.empty())throw std::runtime_error("No free farm surface found on this map");
  auto camera=r.camera(window);double best=1e30;json chosen;
  for(auto site:sites){auto p=projection::project(camera,worldVector(site.at("worldPosition")));if(!p||p->x<80||p->x>camera.width-80||p->y<130||p->y>camera.height-180)continue;double d=std::hypot(p->x-camera.width/2.,p->y-camera.height/2.);if(d<best){best=d;chosen=site;}}
  if(chosen.is_null())throw std::runtime_error("Free farm surfaces are outside the current view");farm=chosen;
 }
 auto end=Clock::now()+std::chrono::minutes(20);auto lastUpgrade=Clock::now()-std::chrono::seconds(10);int readFailures=0,rejections=0;json rejected=json::array();
 std::cout<<"Autoplay: one farm first; leading-enemy roadside defense; farm upgrades before defender upgrades. F8 stops.\n"<<std::flush;
 while(Clock::now()<end){
  guard();if(r.visibleUiIdentity({"ResultsUI","Holder"})||r.visibleUiIdentity({"ResultsUI","Finished"})){record({{"action","match_finished"}});std::cout<<"Autoplay: match finished\n";return;}
  std::optional<Snapshot> snapshot;json observation;
  try{snapshot=r.snapshot();observation=r.enemies();if(observation.at("readErrors").get<int>())throw std::runtime_error("Enemy objects still loading");readFailures=0;}catch(const std::exception&){if(++readFailures>80)throw std::runtime_error("Autoplay state unreadable for 20 seconds");std::this_thread::sleep_for(std::chrono::milliseconds(250));continue;}
  auto enemies=observation.at("enemies");std::ofstream(out/"enemies-latest.json")<<observation.dump(2);
  auto front=autoplay::leadingEnemy(enemies,geometry);
  json units=json::array();int farms=0;for(auto& [a,u]:snapshot->units){units.push_back({{"position",u.position}});if(u.name=="bulma_summer:golden")++farms;}
  if(!farms){record({{"action","place_farm"},{"wave",snapshot->wave}});placeWorldUnit(r,window,stop,farm,geometry,aliases,out/"placement.json");continue;}
  if(!enemies.empty()){
   auto dynamic=roadsideCandidates(candidates,geometry,front,*snapshot);auto occupied=units;for(auto p:rejected)occupied.push_back({{"position",p}});int choice=autoplay::choose(dynamic,occupied,front,r.currency());
   if(choice>=0){auto event=dynamic.at(choice);record({{"action","place_defender"},{"wave",snapshot->wave},{"enemyCount",enemies.size()},{"unit",event.at("unitName")},{"worldPosition",event.at("worldPosition")}});std::cout<<"Autoplay: "<<enemies.size()<<" enemies; defending with "<<event.at("unitName")<<"\n"<<std::flush;
    const double moneyBefore=r.currency();try{placeWorldUnit(r,window,stop,event,geometry,aliases,out/"placement.json");}
    catch(const std::exception& ex){if(std::string(ex.what())!="World placement was not confirmed by owned-unit memory")throw;guard();auto after=r.snapshot();if(after.units.size()!=snapshot->units.size()||r.currency()<moneyBefore)throw std::runtime_error("Uncertain placement result; stopping instead of duplicating");for(auto& [a,u]:snapshot->units){(void)u;if(!after.units.contains(a))throw std::runtime_error("Units changed during rejected placement");}rejected.push_back(event.at("worldPosition"));record({{"action","reject_position"},{"position",event.at("worldPosition")}});if(++rejections>=5)throw std::runtime_error("Five placement positions rejected; stop for review");}
    continue;
   }
  }
  if(farms>=1&&Clock::now()-lastUpgrade>std::chrono::seconds(4)){
   // Farm upgrades have strict priority. New defense placements still preempt upgrades.
   std::vector<std::pair<double,Address>> order;
   for(auto& [a,u]:snapshot->units){bool isFarm=u.name=="bulma_summer:golden";if(!aliases.contains(u.name))continue;
    auto stats=r.find(a,"_stats");if(u.upgrade>=r.number(r.find(stats,"max_upgrade")))continue;
    double score=autoplay::upgradePriority(isFarm,u.position,front);order.push_back({score,a});
   }std::sort(order.rbegin(),order.rend());
   for(auto [score,a]:order){(void)score;guard();auto u=snapshot->units.at(a);json event={{"unitName",u.name},{"worldPosition",u.position},{"upgrade",u.upgrade+1}};
    upgradeWorldUnit(r,window,stop,event,geometry,aliases,out/"upgrade.json",false,0);
    auto report=json::parse(std::ifstream(out/"upgrade.json"));if(report.at("state")=="confirmed"){record({{"action","upgrade"},{"unit",u.name},{"upgrade",u.upgrade+1},{"wave",snapshot->wave}});break;}
    if(u.name=="bulma_summer:golden")break; // Save for the farm instead of spending on another upgrade.
   }lastUpgrade=Clock::now();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
 }
 throw std::runtime_error("Autoplay session time limit reached");
}
