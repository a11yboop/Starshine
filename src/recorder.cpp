#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include "json.hpp"
using json=nlohmann::json;
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
using Address=std::uint64_t;
#include "input.hpp"
#include "ui_readiness.hpp"
#include "projection.hpp"
#include "placement_geometry.hpp"
#include "placement_binding.hpp"
#include "strategy_recording.hpp"
struct Unit { std::string name; int upgrade; std::array<float,3> position; };
struct Snapshot { Address place; int wave; std::map<Address,Unit> units; };
struct MatchState { Address place;int wave; };
struct Handle { HANDLE h=nullptr; ~Handle(){if(h && h!=INVALID_HANDLE_VALUE)CloseHandle(h);} };
class Reader {
 Handle process; json layout; Address base=0;
 public:
 Reader(DWORD pid,const json& l):layout(l){
  process.h=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
  if(!process.h)throw std::runtime_error("OpenProcess failed: "+std::to_string(GetLastError()));
  wchar_t path[32768]; DWORD size=32768;
  if(!QueryFullProcessImageNameW(process.h,0,path,&size))throw std::runtime_error("Cannot verify executable");
  auto exe=fs::path(std::wstring(path,size));
  if(exe.filename()!=L"RobloxPlayerBeta.exe" || exe.parent_path().filename().string()!=layout.at("version").get<std::string>())throw std::runtime_error("Unsupported executable/version");
  Handle modules; modules.h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
  MODULEENTRY32W m{};m.dwSize=sizeof(m);
  if(modules.h==INVALID_HANDLE_VALUE || !Module32FirstW(modules.h,&m))throw std::runtime_error("Cannot resolve module");
  base=reinterpret_cast<Address>(m.modBaseAddr);
 }
 template<class T>T read(Address a){
  if(a<65536 || a>0x7fffffffffffULL-sizeof(T))throw std::runtime_error("Invalid address");
  T value{};SIZE_T got=0;
  if(!ReadProcessMemory(process.h,reinterpret_cast<void*>(a),&value,sizeof(T),&got)||got!=sizeof(T))throw std::runtime_error("Memory read unavailable");return value;
 }
 Address off(const char* key){return layout.at(key).get<Address>();}
 Address dataModel(){return read<Address>(read<Address>(base+off("fakePointer"))+off("realDataModel"));}
 std::string str(Address a){
  auto n=read<Address>(a+16),capacity=read<Address>(a+24);
  if(n>200 || capacity<n)throw std::runtime_error("Invalid string");
  if(!n)return {};if(capacity>15)a=read<Address>(a);
  std::string s(n,'\0');SIZE_T got=0;
  if(!ReadProcessMemory(process.h,reinterpret_cast<void*>(a),s.data(),n,&got)||got!=n)throw std::runtime_error("Unreadable string");
  return s;
 }
 std::string name(Address a){return str(read<Address>(a+off("name")));}
 std::vector<Address> children(Address a){
  auto v=read<Address>(a+off("children"));if(!v)return {};
  auto start=read<Address>(v),end=read<Address>(v+8);
  if(end<start || (end-start)%16 || end-start>160000)throw std::runtime_error("Invalid child list");
  std::vector<Address> out;for(auto i=start;i<end;i+=16)out.push_back(read<Address>(i));
  if(read<Address>(v)!=start || read<Address>(v+8)!=end)throw std::runtime_error("Child list changed during read");return out;
 }
 Address find(Address a,const std::string& target){for(auto c:children(a))if(name(c)==target)return c;throw std::runtime_error("Missing object: "+target);}
 std::string className(Address a){return str(read<Address>(read<Address>(a+off("classDescriptor"))+off("className")));}
 Address cameraObject(){
  Address found=0;
  for(auto child:children(find(dataModel(),"Workspace")))if(className(child)=="Camera"){
   if(found)throw std::runtime_error("Multiple workspace cameras; active camera is ambiguous");found=child;
  }
  if(!found)throw std::runtime_error("Workspace camera unavailable");return found;
 }
 projection::Camera camera(HWND window){
  auto object=cameraObject();RECT rect{};
  if(!GetClientRect(window,&rect))throw std::runtime_error("Camera viewport unavailable");
  auto rotation=read<std::array<float,9>>(object+off("cameraRotation"));auto p=read<std::array<float,3>>(object+off("cameraPosition"));
  projection::Camera result;result.position={p[0],p[1],p[2]};for(size_t i=0;i<9;++i)result.rotation[i]=rotation[i];
  result.verticalFovRadians=read<float>(object+off("cameraFieldOfView"));result.width=rect.right;result.height=rect.bottom;
  if(!projection::validCamera(result))throw std::runtime_error("Invalid camera transform/FOV");return result;
 }
 Address localPlayer(){return read<Address>(find(dataModel(),"Players")+off("localPlayer"));}
 Address guiObject(const std::vector<std::string>& path){auto a=find(localPlayer(),"PlayerGui");for(const auto& name:path)a=find(a,name);return a;}
 std::string guiText(Address object){if(className(object)!="TextLabel"&&className(object)!="TextButton")throw std::runtime_error("Expected UI text");return str(object+off("guiText"));}
 POINT guiCenter(const std::vector<std::string>& path,HWND window){auto a=guiObject(path);auto p=read<std::array<float,2>>(a+244),size=read<std::array<float,2>>(a+252);RECT bounds{};GetClientRect(window,&bounds);double x=p[0]+size[0]/2,y=p[1]+size[1]/2;if(!std::isfinite(x)||!std::isfinite(y)||size[0]<=0||size[1]<=0||x<0||y<0||x>=bounds.right||y>=bounds.bottom)throw std::runtime_error("Invalid UI rectangle");return {static_cast<LONG>(std::lround(x)),static_cast<LONG>(std::lround(y))};}
 projection::Vec3 navigationPosition(){auto local=localPlayer();auto character=find(find(dataModel(),"Workspace"),name(local));auto root=find(character,"HumanoidRootPart");auto primitive=read<Address>(root+off("primitive"));auto p=read<std::array<float,3>>(primitive+off("position"));projection::Vec3 result{p[0],p[1],p[2]};if(!projection::finite(result))throw std::runtime_error("Navigation position unavailable");return result;}
 double currency(){auto value=number(find(find(localPlayer(),"_stats"),"resource"));if(value<0||value>1e12)throw std::runtime_error("Invalid game currency");return value;}
 json hotbar(){
  auto root=guiObject({"spawn_units","Lives","Frame","Units"});json slots=json::array();
  for(int slot=1;slot<=6;++slot){auto card=find(root,std::to_string(slot));auto label=guiText(find(card,"name"));auto costText=guiText(find(find(card,"Cost"),"text"));
   std::string digits;for(unsigned char c:costText)if(c>='0'&&c<='9')digits.push_back(static_cast<char>(c));
   if(label.empty()||digits.empty())throw std::runtime_error("Hotbar slot is not readable");auto cost=std::stod(digits);if(cost<=0||cost>1e9)throw std::runtime_error("Hotbar cost invalid");
   slots.push_back({{"slot",slot},{"displayName",label},{"cost",cost},{"costText",costText}});
  }return slots;
 }
 double number(Address a){auto n=read<double>(a+off("value"));if(!std::isfinite(n))throw std::runtime_error("Invalid numeric value");return n;}
 Address visibleUiIdentity(const std::vector<std::string>& path){
  try{
   auto dm=read<Address>(read<Address>(base+off("fakePointer"))+off("realDataModel"));
   auto local=read<Address>(find(dm,"Players")+off("localPlayer"));auto node=find(local,"PlayerGui");
   for(size_t i=0;i<path.size();++i){node=find(node,path[i]);auto flag=read<unsigned char>(node+(i==0?1164:1397));if(flag!=1)return 0;}
   return node;
  }catch(...){return 0;}
 }
 MatchState matchState(){
  auto dm=read<Address>(read<Address>(base+off("fakePointer"))+off("realDataModel"));
  auto place=read<Address>(dm+off("placeId"));auto w=number(find(find(dm,"Workspace"),"_wave_num"));
  if(!place||w<0||w>100000||std::floor(w)!=w)throw std::runtime_error("Match state unavailable");
  return {place,static_cast<int>(w)};
 }
 json geometry(){
  auto dm=read<Address>(read<Address>(base+off("fakePointer"))+off("realDataModel"));auto ws=find(dm,"Workspace");
  struct Node{Address a;std::string path;int depth;};std::vector<Node> queue;json objects=json::array();int failures=0;
  for(auto a:children(ws)){auto n=name(a);if(n=="_map"||n=="_terrain"||n=="_road"||n=="_BASES"||n=="_water_blocks")queue.push_back({a,n,0});}
  size_t index=0;
  for(;index<queue.size()&&index<60000;++index){auto node=queue[index];try{
   auto cls=str(read<Address>(read<Address>(node.a+off("classDescriptor"))+off("className")));
   if(cls=="Part"||cls=="MeshPart"||cls=="UnionOperation"||cls=="WedgePart"){
    auto primitive=read<Address>(node.a+off("primitive"));auto pos=read<std::array<float,3>>(primitive+off("position"));auto size=read<std::array<float,3>>(primitive+452);auto rotation=read<std::array<float,9>>(primitive+176);
    for(auto v:pos)if(!std::isfinite(v))throw std::runtime_error("Invalid geometry");
    for(auto v:size)if(!std::isfinite(v)||v<=0)throw std::runtime_error("Invalid geometry size");
    for(auto v:rotation)if(!std::isfinite(v))throw std::runtime_error("Invalid geometry rotation");
    objects.push_back({{"path",node.path},{"class",cls},{"position",pos},{"size",size},{"rotation",rotation}});
   }
   if((cls=="Folder"||cls=="Model"||cls=="Configuration")&&node.depth<16)for(auto c:children(node.a))queue.push_back({c,node.path+"/"+name(c),node.depth+1});
  }catch(...){++failures;}}
  return {{"placeId",read<Address>(dm+off("placeId"))},{"objects",objects},{"readErrors",failures},{"truncated",index<queue.size()},{"meshRepresentation","oriented bounding boxes, not exact placement polygons"},{"playerPositionRecorded",false}};
 }
 Snapshot snapshot(Address onlyUnit=0){
  auto dm=read<Address>(read<Address>(base+off("fakePointer"))+off("realDataModel"));
  auto ws=find(dm,"Workspace"),players=find(dm,"Players");
  auto local=read<Address>(players+off("localPlayer"));if(!local)throw std::runtime_error("Local player unavailable");
  auto place=read<Address>(dm+off("placeId"));if(!place)throw std::runtime_error("Place unavailable");
  double w=number(find(ws,"_wave_num"));if(w<0||w>100000||std::floor(w)!=w)throw std::runtime_error("Invalid wave");
  Snapshot result{place,static_cast<int>(w),{}};
  auto group=find(ws,"_UNITS");
  for(auto u:children(group)){
   // After binding an upgrade target, unrelated spawning/despawning units
   // must not abort the click because their _stats is still unavailable.
   if(onlyUnit&&u!=onlyUnit)continue;
   auto stats=find(u,"_stats");auto owner=read<Address>(find(stats,"player")+off("value"));if(owner!=local)continue;
   auto up=number(find(stats,"upgrade"));if(up<0||up>1000||std::floor(up)!=up)throw std::runtime_error("Invalid upgrade");
   auto root=find(u,"HumanoidRootPart");auto primitive=read<Address>(root+off("primitive"));
   auto pos=read<std::array<float,3>>(primitive+off("position"));for(auto x:pos)if(!std::isfinite(x)||std::abs(x)>1e7)throw std::runtime_error("Invalid unit position");
   result.units.emplace(u,Unit{name(u),static_cast<int>(up),pos});
  }
  if(read<Address>(dm+off("placeId"))!=place || number(find(ws,"_wave_num"))!=w)throw std::runtime_error("Match state changed during read");
  return result;
 }
 json enemies(){
  auto dm=dataModel(),ws=find(dm,"Workspace");auto place=read<Address>(dm+off("placeId"));json result={{"placeId",place},{"enemies",json::array()},{"readErrors",0}};
  for(auto u:children(find(ws,"_UNITS")))try{
   auto stats=find(u,"_stats");auto owner=read<Address>(find(stats,"player")+off("value"));
   // Explicit NPC reward plus no player owner; never classify another player's
   // tower or an unknown partially constructed object as an enemy.
   if(owner)continue;double reward=number(find(stats,"resource_reward")),hp=number(find(stats,"health"));if(reward<=0||hp<=0)continue;
   auto root=find(u,"HumanoidRootPart"),primitive=read<Address>(root+off("primitive"));auto pos=read<std::array<float,3>>(primitive+off("position"));for(float n:pos)if(!std::isfinite(n)||std::abs(n)>1e7)throw std::runtime_error("Invalid enemy position");
   result["enemies"].push_back({{"id",std::to_string(u)},{"name",name(u)},{"position",pos},{"health",hp},{"speed",number(find(stats,"speed"))}});
  }catch(const std::exception&){result["readErrors"]=result["readErrors"].get<int>()+1;}
  if(read<Address>(dataModel()+off("placeId"))!=place)throw std::runtime_error("Map changed during enemy read");return result;
 }
};
class Tracker {
 std::map<Address,Unit> previous;std::map<Address,std::string> ids;unsigned nextId=1;bool initialized=false,gap=false;Address place=0;
 json unitData(Address address,const Unit& u){if(!ids.contains(address))ids[address]="unit-"+std::to_string(nextId++);return {{"unitId",ids.at(address)},{"unitName",u.name},{"upgrade",u.upgrade},{"worldPosition",u.position}};}
 public:
 void interrupted(){gap=true;}
 std::vector<json> update(const Snapshot& s){
  std::vector<json> events;
  if(initialized && s.place!=place)throw std::runtime_error("Map changed; recording must be restarted");
  for(auto& [a,u]:s.units){auto data=unitData(a,u);data["wave"]=s.wave;
   if(!initialized)data["type"]="baseline_unit";
   else if(!previous.contains(a)){data["type"]=gap?"unit_discovered_after_gap":"unit_placed";data["placementTimeExact"]=false;if(u.upgrade>0)data["missingPriorActions"]=true;}
   else if(previous.at(a).name!=u.name){data["type"]="identity_conflict";}
   else if(previous.at(a).upgrade!=u.upgrade){auto old=previous.at(a).upgrade;data["type"]="unit_upgrade_changed";data["fromUpgrade"]=old;data["toUpgrade"]=u.upgrade;data["intermediateActionsUnknown"]=gap||u.upgrade!=old+1;}
   else continue;
   data["afterReadGap"]=gap;events.push_back(data);
  }
  if(initialized)for(auto& [a,u]:previous)if(!s.units.contains(a)){auto d=unitData(a,u);d["type"]="unit_sold";d["wave"]=s.wave;d["classificationSource"]="user_rule_disappearance";d["saleConfirmedByGame"]=false;d["afterReadGap"]=gap;events.push_back(d);ids.erase(a);}
  previous=s.units;place=s.place;initialized=true;gap=false;return events;
 }
};
void require(bool condition){if(!condition)throw std::runtime_error("Self-test failed");}
bool confirms(const json& event,const Snapshot& snapshot);
void tests(){
 UiReadinessGate gate;
 require(!gate.observe(0,0));require(!gate.observe(10,100));
 for(int ms=200;ms<=1200;ms+=100)require(!gate.observe(10,ms));
 require(gate.observe(10,1300));
 require(!gate.observe(0,1400));require(!gate.observe(10,1500));
 for(int ms=1600;ms<2700;ms+=100)require(!gate.observe(10,ms));
 require(gate.observe(10,2700));
 require(!gate.observe(11,2800)); // A recreated button must settle again.
 require(!gate.observe(11,4000)); // No readiness inference across a polling gap.
 gate.reset();require(!gate.observe(11,4100));
 require(normalizedMouseAxis(0,0,1920)==17);
 require(normalizedMouseAxis(-1920,-1920,3840)==8);
 require(normalizedMouseAxis(1919,0,1920)<65536);
 bool badMouse=false;try{normalizedMouseAxis(1920,0,1920);}catch(...){badMouse=true;}require(badMouse);
 require(gameplayKey('W')&&!gameplayKey(VK_RETURN)&&!gameplayKey('Z'));
 Tracker t;Snapshot s{1,1,{{100,{"same",0,{1,2,3}}}}};require(t.update(s).at(0)["type"]=="baseline_unit");require(t.update(s).empty());
 s.units[200]={"same",0,{4,5,6}};auto e=t.update(s);require(e.size()==1&&e[0]["type"]=="unit_placed"&&e[0]["unitId"]=="unit-2");
 s.units[100].upgrade=1;e=t.update(s);require(e[0]["unitId"]=="unit-1"&&!e[0]["intermediateActionsUnknown"].get<bool>());
 s.units[100].upgrade=3;e=t.update(s);require(e[0]["intermediateActionsUnknown"]==true);
 t.interrupted();s.units[300]={"new",0,{1,2,3}};require(t.update(s).at(0)["type"]=="unit_discovered_after_gap");
 s.units.erase(200);e=t.update(s);require(e.at(0)["type"]=="unit_sold" && e.at(0)["unitId"]=="unit-2" && e.at(0)["saleConfirmedByGame"]==false);
 s.units[200]={"same",0,{7,8,9}};e=t.update(s);require(e.at(0)["type"]=="unit_placed" && e.at(0)["unitId"]!="unit-2");
 t.interrupted();s.units.erase(200);e=t.update(s);require(e.at(0)["type"]=="unit_sold" && e.at(0)["afterReadGap"]==true);
 s.place=2;bool rejected=false;try{t.update(s);}catch(...){rejected=true;}require(rejected);
 json checkpoint={{"type","unit_placed"},{"unitName","same"},{"worldPosition",{1,2,3}},{"upgrade",3}};
 require(confirms(checkpoint,s));checkpoint["upgrade"]=4;require(!confirms(checkpoint,s));
 checkpoint["type"]="unit_sold";require(!confirms(checkpoint,s));s.units.erase(100);require(confirms(checkpoint,s));
 std::cout<<"PASS: UI appearance delay, flicker, replacement, polling gaps, reset; mouse coordinates; unit tracking and replay checkpoints\n";
}
bool confirms(const json& event,const Snapshot& snapshot){
 auto p=event.at("worldPosition").get<std::array<float,3>>();bool present=false;
 for(auto& [address,u]:snapshot.units){(void)address;float distance=0;for(int i=0;i<3;++i)distance+=(p[i]-u.position[i])*(p[i]-u.position[i]);
  if(distance<=1&&u.name==event.at("unitName").get<std::string>()){
   present=true;if(event.at("type")!="unit_sold"&&u.upgrade>=event.at("upgrade").get<int>())return true;
  }
 }
 return event.at("type")=="unit_sold"&&!present;
}
#include "navigation.hpp"
#include "world_placement.hpp"
#include "world_upgrade.hpp"
#include "lobby.hpp"
#include "autoplay.hpp"
bool clickLifecycle(Reader& reader,HWND window,const std::string& action,UiReadinessGate& readiness,InputCapture& control){
 const std::map<std::string,std::vector<std::string>> paths={
  {"vote_yes",{"VoteStart","Holder","ButtonHolder","Yes"}},
  {"results_next",{"ResultsUI","Holder","Buttons","Next"}},
  {"replay",{"ResultsUI","Finished","NextRetry"}}};
 const std::map<std::string,std::pair<int,int>> centers={{"vote_yes",{882,184}},{"results_next",{770,745}},{"replay",{1120,216}}};
 if(control.stopRequested())throw std::runtime_error("UI action cancelled by F8");
 auto identity=reader.visibleUiIdentity(paths.at(action));
 if(GetForegroundWindow()!=window)identity=0;
 auto nowMs=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
 if(!readiness.observe(identity,nowMs))return false;
 RECT size{};GetClientRect(window,&size);
 // Calibrated from the supplied 1920x1080 screenshots; never guess another layout.
 if(size.right<=0||size.bottom<=0)throw std::runtime_error("Invalid client dimensions");
 if(GetForegroundWindow()!=window){readiness.reset();return false;}
 MatchState expected{};
 if(action=="vote_yes"){
  try{expected=reader.matchState();}catch(const std::exception&){readiness.reset();return false;}
  if(expected.wave!=0){readiness.reset();return false;}
 }
 InputSender sender;auto point=reader.guiCenter(paths.at(action),window);int x=point.x,y=point.y;
 auto stillReady=[&](){
  if(control.stopRequested())throw std::runtime_error("UI action cancelled by F8");
  control.drain();RECT current{};
  if(GetForegroundWindow()!=window||!GetClientRect(window,&current)||current.right!=size.right||current.bottom!=size.bottom||reader.visibleUiIdentity(paths.at(action))!=identity)return false;
  if(action=="vote_yes"){
   try{auto state=reader.matchState();return state.place==expected.place&&state.wave==0;}catch(const std::exception&){return false;}
  }
  return true;
 };
 std::cout<<"UI ready for 1200 ms: "<<action<<"; moving cursor and waiting for hover\n"<<std::flush;
 if(!sender.hover(x,y,window,stillReady)){readiness.reset();return false;}
 auto hoverUntil=Clock::now()+std::chrono::milliseconds(350);
 while(Clock::now()<hoverUntil){
  if(!stillReady()){readiness.reset();std::cout<<"UI changed during hover: "<<action<<"; waiting again\n"<<std::flush;return false;}
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
 }
 if(!sender.send({{"type","mouse"},{"message",WM_LBUTTONDOWN},{"x",x},{"y",y},{"data",0}},window,stillReady,InputSender::CursorMode::Keep)){
  readiness.reset();std::cout<<"UI changed before click: "<<action<<"; waiting again\n"<<std::flush;return false;
 }
 for(int i=0;i<6;++i){if(control.stopRequested())throw std::runtime_error("UI action cancelled by F8");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 sender.send({{"type","mouse"},{"message",WM_LBUTTONUP},{"x",x},{"y",y},{"data",0}},window,{},InputSender::CursorMode::Keep);
 std::cout<<"UI action sent: "<<action<<"\n"<<std::flush;return true;
}
void replay(DWORD pid,const json& layout,const fs::path& directory,bool validateOnly,bool armed=false,bool placeOnly=false,bool strategy=false,bool autonomous=false){
 auto recording=json::parse(std::ifstream(directory/"recording.json"));auto inputs=json::parse(std::ifstream(directory/"input.json"));auto geometry=json::parse(std::ifstream(directory/"geometry.json"));
 if(!recording.value("inputReplayEligible",false))throw std::runtime_error("Recording is incomplete or has focus gaps; cannot replay");
 if(inputs.at("events").empty())throw std::runtime_error("Recording contains no input");
 long long previous=-1;for(auto& e:inputs.at("events")){auto t=e.at("elapsedMs").get<long long>();if(t<previous||t<0)throw std::runtime_error("Invalid input timeline");previous=t;auto type=e.at("type").get<std::string>();if(type=="key"){if(!gameplayKey(e.at("vk")))throw std::runtime_error("Invalid key");e.at("down").get<bool>();}else if(type=="mouse"){int x=e.at("x"),y=e.at("y");if(x<0||y<0||x>=inputs.at("width").get<int>()||y>=inputs.at("height").get<int>())throw std::runtime_error("Invalid input coordinates");}else if(type!="release_all"&&type!="focus_resumed")throw std::runtime_error("Invalid event type");}
 if(strategy)recording["events"] = strategyEvents(recording.at("events"));
 Reader reader(pid,layout);
 if(strategy&&fs::exists(directory/"lobby.json")){
  auto profile=json::parse(std::ifstream(directory/"lobby.json"));bool inLobby=false;
  try{reader.find(reader.find(reader.dataModel(),"Workspace"),"_LOBBIES");inLobby=true;}catch(const std::exception&){}
  if(inLobby&&profile.value("enabled",false)){
   auto window=gameWindow(pid);InputCapture stop(window,Clock::now());auto deadline=Clock::now()+std::chrono::minutes(3);
   std::cout<<"Lobby profile loaded; focus Roblox to enter the selected level. F8 cancels.\n"<<std::flush;
   while(GetForegroundWindow()!=window){if(stop.stopRequested())throw std::runtime_error("Lobby launch cancelled");stop.drain();if(Clock::now()>deadline)throw std::runtime_error("Timed out waiting for Roblox focus");std::this_thread::sleep_for(std::chrono::milliseconds(50));}
   launchLobbyLevel(reader,window,stop,profile);
  }
 }
 json initial;for(auto& e:recording.at("events"))if(e.at("type")=="baseline"){initial=e;break;}
 if(initial.is_null())throw std::runtime_error("Recording has no baseline");
 if(armed){
  auto waitWindow=gameWindow(pid);InputCapture stop(waitWindow,Clock::now());auto deadline=Clock::now()+std::chrono::minutes(3);bool found=false;
  std::cout<<"Armed: waiting up to 3 minutes for the recorded initial map/wave/unit count. F8 cancels.\n"<<std::flush;
  std::string lastReason;std::set<std::string> lifecycleSent;std::map<std::string,UiReadinessGate> lifecycleReady;
  while(Clock::now()<deadline){if(stop.stopRequested())throw std::runtime_error("Armed playback cancelled by F8 before any replay input");stop.drain();
   if(GetForegroundWindow()==waitWindow){
    for(const auto& action:{std::string("results_next"),std::string("replay")})if(!lifecycleSent.contains(action)&&clickLifecycle(reader,waitWindow,action,lifecycleReady[action],stop)){lifecycleSent.insert(action);std::this_thread::sleep_for(std::chrono::milliseconds(200));break;}
   }else{for(auto& [action,gate]:lifecycleReady){(void)action;gate.reset();}}
   try{auto s=reader.snapshot();auto reason="Waiting: place="+std::to_string(s.place)+", wave="+std::to_string(s.wave)+", units="+std::to_string(s.units.size())+", focused="+(GetForegroundWindow()==waitWindow?"yes":"no");if(reason!=lastReason){std::cout<<reason<<"\n"<<std::flush;lastReason=reason;}
    if(GetForegroundWindow()==waitWindow&&s.place==geometry.at("placeId").get<Address>()&&s.wave==initial.at("wave")&&s.units.size()==initial.at("ownedUnits")){found=true;break;}
   }catch(const std::exception& ex){std::string reason=ex.what();if(reason!=lastReason){std::cout<<"Waiting for readable match: "<<reason<<"\n"<<std::flush;lastReason=reason;}}
   std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }if(!found)throw std::runtime_error("Timed out waiting for initial match");
 }
 auto baseline=reader.snapshot();if(baseline.place!=geometry.at("placeId").get<Address>())throw std::runtime_error("Wrong map");
 auto window=gameWindow(pid);RECT client{};GetClientRect(window,&client);if(!strategy&&(client.right!=inputs.at("width")||client.bottom!=inputs.at("height")))throw std::runtime_error("Client size differs from recording");
 if(initial.is_null()||baseline.wave!=initial.at("wave")||baseline.units.size()!=initial.at("ownedUnits"))throw std::runtime_error("Starting wave/unit count differs from recording");
 std::set<Address> matched;
 for(auto& e:recording.at("events"))if(e.at("type")=="baseline_unit"){
  auto p=e.at("worldPosition").get<std::array<float,3>>();bool found=false;
  for(auto& [a,u]:baseline.units){float distance=0;for(int i=0;i<3;++i)distance+=(p[i]-u.position[i])*(p[i]-u.position[i]);
   if(!matched.contains(a)&&u.name==e.at("unitName").get<std::string>()&&u.upgrade==e.at("upgrade").get<int>()&&distance<1){matched.insert(a);found=true;break;}}
  if(!found)throw std::runtime_error("Initial unit positions/upgrades differ from recording");
 }
 if(validateOnly){std::cout<<"Replay preflight passed; no input sent. Camera and interface must match manually.\n";return;}
 std::vector<json> timeline;
 auto bindings=strategy?std::vector<placement_binding::PlacementBinding>{}:placement_binding::bindPlacements(recording.at("events"),inputs.at("events"));
 std::map<size_t,json> worldPlacements;std::set<size_t> suppressed;
 for(const auto& binding:bindings){worldPlacements[binding.downIndex]=recording.at("events").at(binding.eventIndex);suppressed.insert(binding.upIndex);suppressed.insert(binding.selectionDownIndex);suppressed.insert(binding.selectionUpIndex);}
 for(size_t i=0;i<inputs.at("events").size();++i){
  if(suppressed.contains(i))continue;auto e=inputs.at("events").at(i);
  if(worldPlacements.contains(i)){e["worldPlacement"]=worldPlacements.at(i);e["type"]="world_placement";}
  timeline.push_back(e);
 }
 for(auto e:recording.at("events")){auto type=e.at("type").get<std::string>();if(type=="unit_placed"||type=="unit_discovered_after_gap"||type=="unit_upgrade_changed"||type=="unit_sold"){e["checkpoint"]=true;timeline.push_back(e);}}
 std::stable_sort(timeline.begin(),timeline.end(),[](const json& a,const json& b){return a.at("elapsedMs").get<long long>()<b.at("elapsedMs").get<long long>();});
 InputCapture control(window,Clock::now());
 std::cout<<(armed?"Initial match found. Starting playback now. F8 stops.\n":"Focus Roblox within five seconds. F8 stops playback.\n")<<std::flush;
 for(int i=0;i<(armed?0:500);++i){if(control.stopRequested())throw std::runtime_error("Playback stopped by F8");control.drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 if(baseline.wave==0){
  auto timeout=Clock::now()+std::chrono::seconds(45);auto voteSentAt=Clock::now();bool sent=false,startedMatch=false;UiReadinessGate voteReady;
  std::cout<<"Waiting for Vote start: Yes to remain visible for 1200 ms (45 s timeout)\n"<<std::flush;
  while(Clock::now()<timeout){if(control.stopRequested())throw std::runtime_error("Start cancelled by F8");control.drain();
   if(GetForegroundWindow()!=window)throw std::runtime_error("Start cancelled: Roblox lost focus");
   MatchState state{};
   try{state=reader.matchState();}catch(const std::exception&){voteReady.reset();std::this_thread::sleep_for(std::chrono::milliseconds(50));continue;}
   if(state.place!=baseline.place)throw std::runtime_error("Map changed before start");
   if(state.wave>0){startedMatch=true;break;}
   if(!sent&&clickLifecycle(reader,window,"vote_yes",voteReady,control)){
    sent=true;voteSentAt=Clock::now();timeout=voteSentAt+std::chrono::seconds(45);
    std::cout<<"Vote click sent; waiting up to 45 s for the first wave\n"<<std::flush;
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if(!startedMatch)throw std::runtime_error(sent?"Vote start not confirmed within 45 s after click":"Vote start button did not become ready within 45 s");
  std::cout<<"Match start confirmed by wave transition";
  if(sent)std::cout<<"; "<<std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-voteSentAt).count()<<" ms after vote input";
  std::cout<<"\n"<<std::flush;
 }
 if(autonomous){autoPlay(reader,window,control,directory);return;}
 if(placeOnly){
  for(const auto& event:recording.at("events"))if(event.at("type")=="unit_placed"||event.at("type")=="unit_discovered_after_gap"){
   placeWorldUnit(reader,window,control,event,geometry,json::parse(std::ifstream("config/unit-names.json")),directory/"world-placement-latest.json");return;
  }
  throw std::runtime_error("Recording has no placement action");
 }
 if(strategy){
  auto aliases=json::parse(std::ifstream("config/unit-names.json"));
  for(const auto& event:recording.at("events")){
   auto type=event.at("type").get<std::string>();if(type!="unit_placed"&&type!="unit_discovered_after_gap"&&type!="unit_upgrade_changed")continue;
   int wave=event.at("wave");auto timeout=Clock::now()+std::chrono::minutes(15);
   std::cout<<"Waiting for recorded wave "<<wave<<" for "<<type<<"\n"<<std::flush;
   while(true){if(control.stopRequested())throw std::runtime_error("Strategy stopped by F8");control.drain();if(GetForegroundWindow()!=window)throw std::runtime_error("Strategy lost focus");auto state=reader.matchState();if(state.place!=baseline.place)throw std::runtime_error("Strategy map changed");if(reader.visibleUiIdentity({"ResultsUI","Holder"})||reader.visibleUiIdentity({"ResultsUI","Finished"}))throw std::runtime_error("Match ended before action");if(state.wave>=wave)break;if(Clock::now()>timeout)throw std::runtime_error("Wave wait expired");std::this_thread::sleep_for(std::chrono::milliseconds(100));}
   if(type=="unit_upgrade_changed")upgradeWorldUnit(reader,window,control,event,geometry,aliases,directory/"world-upgrade-latest.json");
   else placeWorldUnit(reader,window,control,event,geometry,aliases,directory/"world-placement-latest.json");
  }
  std::cout<<"Recorded unit strategy finished; abilities and sales are not included in this mode\n"<<std::flush;return;
 }
 InputSender sender;auto start=Clock::now();auto lastCheck=start;
 for(auto& e:timeline){
  auto deadline=start+std::chrono::milliseconds(e.at("elapsedMs").get<long long>());
  while(Clock::now()<deadline){if(control.stopRequested())throw std::runtime_error("Playback stopped by F8");control.drain();if(GetForegroundWindow()!=window)throw std::runtime_error("Playback stopped: focus lost");std::this_thread::sleep_for(std::chrono::milliseconds(5));}
  if(control.stopRequested())throw std::runtime_error("Playback stopped by F8");
  GetClientRect(window,&client);if(!strategy&&(client.right!=inputs.at("width")||client.bottom!=inputs.at("height")))throw std::runtime_error("Window size changed");
  if(Clock::now()-lastCheck>std::chrono::seconds(1)){if(reader.snapshot().place!=baseline.place)throw std::runtime_error("Map changed");lastCheck=Clock::now();}
  if(e.contains("worldPlacement")){
   auto began=Clock::now();sender.release();
   placeWorldUnit(reader,window,control,e.at("worldPlacement"),geometry,json::parse(std::ifstream("config/unit-names.json")),directory/"world-placement-latest.json");
   start+=Clock::now()-began;
  }else if(e.value("checkpoint",false)&&e.at("type")=="unit_upgrade_changed"){
   auto began=Clock::now();sender.release();
   upgradeWorldUnit(reader,window,control,e,geometry,json::object(),directory/"world-upgrade-latest.json");
   start+=Clock::now()-began;
  }else if(e.value("checkpoint",false)){
   auto began=Clock::now();bool confirmed=false;
   do{
    if(control.stopRequested())throw std::runtime_error("Playback stopped by F8");control.drain();
    if(GetForegroundWindow()!=window)throw std::runtime_error("Checkpoint stopped: focus lost");
    try{auto s=reader.snapshot();if(s.place!=baseline.place)throw std::runtime_error("Map changed");if(confirms(e,s)){confirmed=true;break;}}catch(const std::exception&){ }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }while(Clock::now()-began<std::chrono::seconds(3));
   if(!confirmed)throw std::runtime_error("Action not confirmed: "+e.at("type").get<std::string>()+" / "+e.at("unitName").get<std::string>());
   std::cout<<"Confirmed: "<<e.at("type").get<std::string>()<<" / "<<e.at("unitName").get<std::string>()<<"\n"<<std::flush;
   start+=Clock::now()-began;
  }else if(e.at("type")=="release_all")sender.release();
  else if(e.at("type")!="focus_resumed")sender.send(e,window,[&](){if(control.stopRequested())throw std::runtime_error("Playback stopped by F8 before input");return true;});
 }
 std::cout<<"Input playback finished; match success not verified.\n";
}
int main(int argc,char** argv){
 try{
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  if(GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext())!=DPI_AWARENESS_PER_MONITOR_AWARE)throw std::runtime_error("Per-monitor DPI awareness unavailable; input coordinates cannot be trusted");
  if(argc==5&&std::string(argv[1])=="--upgrade-recorded"){Reader reader(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));auto window=gameWindow(std::stoul(argv[2]));InputCapture control(window,Clock::now());fs::path dir=argv[4];auto recording=json::parse(std::ifstream(dir/"recording.json"));for(const auto& event:recording.at("events"))if(event.at("type")=="unit_upgrade_changed"){upgradeWorldUnit(reader,window,control,event,json::parse(std::ifstream(dir/"geometry.json")),json::parse(std::ifstream("config/unit-names.json")),dir/"world-upgrade-latest.json");return 0;}throw std::runtime_error("No recorded upgrade");}
  if(argc==5&&std::string(argv[1])=="--launch-lobby-level"){Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));auto window=gameWindow(std::stoul(argv[2]));InputCapture stop(window,Clock::now());launchLobbyLevel(r,window,stop,json::parse(std::ifstream(argv[4])));return 0;}
  if(argc==4&&std::string(argv[1])=="--enter-lobby-door"){Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));auto window=gameWindow(std::stoul(argv[2]));InputCapture stop(window,Clock::now());enterLobbyDoor(r,window,stop);return 0;}
  if(argc==4&&std::string(argv[1])=="--inspect-hotbar"){Reader reader(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));std::cout<<reader.hotbar().dump(2)<<"\n";return 0;}
  if(argc==5&&std::string(argv[1])=="--autoplay"){Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));auto window=gameWindow(std::stoul(argv[2]));InputCapture stop(window,Clock::now());autoPlay(r,window,stop,argv[4]);return 0;}
  if(argc==4&&std::string(argv[1])=="--inspect-enemies"){Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));std::cout<<r.enemies().dump(2);return 0;}
  if(argc==5&&std::string(argv[1])=="--save-geometry"){Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));auto g=r.geometry();std::ofstream(argv[4])<<g.dump();std::cout<<"Geometry objects: "<<g.at("objects").size()<<"\n";return 0;}
  if(argc==4&&std::string(argv[1])=="--inspect-units"){
   Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));json out=json::array();auto group=r.find(r.find(r.dataModel(),"Workspace"),"_UNITS");int count=0;
   for(auto u:r.children(group)){if(++count>12)break;json item={{"name",r.name(u)}};for(auto c:r.children(u)){auto name=r.name(c);item["children"].push_back({name,r.className(c)});if(name=="_stats")for(auto v:r.children(c)){json stat={{"name",r.name(v)},{"class",r.className(v)}};try{if(r.className(v)=="NumberValue")stat["value"]=r.number(v);if(r.className(v)=="ObjectValue")stat["isLocalPlayer"]=r.read<Address>(v+r.off("value"))==r.localPlayer();}catch(...){}item["stats"].push_back(stat);}}out.push_back(item);}std::cout<<out.dump(2);return 0;
  }
  if(argc==5&&std::string(argv[1])=="--inspect-lobby-tree"){
   Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));std::string root=argv[4];
   if(root!="_LOBBIES"&&root!="TeleportsUI"&&root!="MapSelectGui"&&root!="LevelSelectGui"&&root!="DifficultySelect"&&root!="LeftUI_test"&&root!="MatchmakingUI")throw std::runtime_error("Unsupported lobby inspection root");
   Address a=root=="_LOBBIES"?r.find(r.find(r.dataModel(),"Workspace"),root):r.guiObject({root});
   struct N{Address a;std::string path;int depth;};std::vector<N> q{{a,root,0}};json out=json::array();
   for(size_t i=0;i<q.size()&&i<8000;++i){auto n=q[i];try{auto cls=r.className(n.a);json item={{"path",n.path},{"class",cls}};if(cls=="TextLabel"||cls=="TextButton")try{item["text"]=r.guiText(n.a);}catch(...){}if(cls=="Part"&&root=="_LOBBIES"){auto p=r.read<Address>(n.a+r.off("primitive"));item["position"]=r.read<std::array<float,3>>(p+r.off("position"));}out.push_back(item);if(n.depth<9)for(auto child:r.children(n.a))q.push_back({child,n.path+"/"+r.name(child),n.depth+1});}catch(...){}}
   std::cout<<out.dump(2)<<"\n";return 0;
  }
  if(argc==4&&std::string(argv[1])=="--inspect-lobby"){
   Reader r(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])));json result;
   for(auto a:r.children(r.find(r.dataModel(),"Workspace")))result["workspace"].push_back({{"name",r.name(a)},{"class",r.className(a)}});
   for(auto a:r.children(r.guiObject({})))result["gui"].push_back({{"name",r.name(a)},{"class",r.className(a)}});
   std::cout<<result.dump(2)<<"\n";return 0;
  }
  if(argc==5&&std::string(argv[1])=="--arm-auto"){replay(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])),argv[4],false,true,false,true,true);return 0;}
  if(argc==5&&std::string(argv[1])=="--arm-strategy"){replay(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])),argv[4],false,true,false,true);return 0;}
  if(argc==5&&std::string(argv[1])=="--arm-place"){replay(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])),argv[4],false,true,true);return 0;}
  if(argc==5&&(std::string(argv[1])=="--replay"||std::string(argv[1])=="--validate-replay"||std::string(argv[1])=="--arm-replay")){replay(std::stoul(argv[2]),json::parse(std::ifstream(argv[3])),argv[4],std::string(argv[1])=="--validate-replay",std::string(argv[1])=="--arm-replay");return 0;}
  if(argc==2&&std::string(argv[1])=="--self-test"){tests();return 0;}
  if(argc<4){std::cerr<<"Usage: recorder.exe PID layout.json output-directory [duration-seconds]\nF8 or create STOP in output-directory to stop.\n";return 2;}
  DWORD pid=std::stoul(argv[1]);auto layout=json::parse(std::ifstream(argv[2]));fs::path out=argv[3];fs::create_directories(out);
  if(fs::exists(out/"events.jsonl"))throw std::runtime_error("Output already contains a recording; choose a new directory");
  std::ofstream log(out/"events.jsonl",std::ios::binary);log.exceptions(std::ios::badbit|std::ios::failbit);
  Reader reader(pid,layout);auto window=gameWindow(pid);RECT windowSize{};GetClientRect(window,&windowSize);json initialHotbar;
  try{initialHotbar=reader.hotbar();}catch(const std::exception&){initialHotbar=nullptr;}
  {auto geometry=reader.geometry();if(geometry.at("objects").empty())throw std::runtime_error("Enter a match first: map geometry not available in lobby");std::ofstream geometryFile(out/"geometry.json");geometryFile.exceptions(std::ios::badbit|std::ios::failbit);geometryFile<<geometry.dump();}
  const auto started=Clock::now();auto elapsed=[&](){return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-started).count();};
  InputCapture input(window,started);json inputEvents=json::array();bool inputComplete=true;
  std::ofstream inputLog(out/"input.jsonl");inputLog.exceptions(std::ios::badbit|std::ios::failbit);
  auto drainInput=[&](){for(auto& event:input.drain()){inputLog<<event.dump()<<'\n';inputEvents.push_back(event);}inputLog.flush();};
  json savedEvents=json::array();
  auto emit=[&](json j){j["elapsedMs"]=elapsed();log<<j.dump()<<'\n';log.flush();savedEvents.push_back(j);};
  auto status=[&](std::string state,int count,std::string error=""){std::ofstream f(out/"status.json");f<<json{{"state",state},{"events",count},{"elapsedMs",elapsed()},{"error",error}}.dump(2);};
  Tracker tracker;int count=0,failures=0;bool ready=false,failed=false;std::string failureReason;auto nextBeat=Clock::now();
  emit({{"type","session_start"},{"schemaVersion",1},{"clientVersion",layout.at("version")},{"sampleIntervalMs",200},{"playerPositionRecorded",false},{"currencyRecorded",false},{"replayReady",false},{"hotbar",initialHotbar}});
  auto duration=argc>4?std::stoi(argv[4]):0;
  while(true){
   if(input.stopRequested()||fs::exists(out/"STOP")||(duration>0&&elapsed()>=duration*1000LL))break;
   auto tick=Clock::now();
   drainInput();RECT sizeNow{};GetClientRect(window,&sizeNow);if(sizeNow.right!=windowSize.right||sizeNow.bottom!=windowSize.bottom)inputComplete=false;
   try{
    auto snapshot=reader.snapshot();
    if(!ready){emit({{"type","baseline"},{"placeId",snapshot.place},{"wave",snapshot.wave},{"ownedUnits",snapshot.units.size()}});ready=true;}
    if(failures)emit({{"type","read_recovered"},{"failedPolls",failures}});
    for(auto event:tracker.update(snapshot)){emit(event);++count;}
    failures=0;status("recording",count);
    if(Clock::now()>=nextBeat){emit({{"type","heartbeat"},{"wave",snapshot.wave},{"ownedUnits",snapshot.units.size()}});nextBeat=Clock::now()+std::chrono::seconds(5);}
   }catch(const std::exception& ex){tracker.interrupted();++failures;if(failures==1)emit({{"type","read_gap"},{"reason",ex.what()}});status("waiting_for_valid_data",count,ex.what());if(failures>=50){failureReason=ex.what();failed=true;inputComplete=false;emit({{"type","session_failed"},{"reason",ex.what()}});break;}}
   std::this_thread::sleep_until(tick+std::chrono::milliseconds(200));
  }
  emit({{"type","session_stop"},{"unitEvents",count}});
  input.finish();
  drainInput();
  std::string placementBindingError;
  try{
   auto bindings=placement_binding::bindPlacements(savedEvents,inputEvents);
   if(initialHotbar.is_array())for(const auto& binding:bindings)for(const auto& card:initialHotbar)if(card.at("slot")==binding.slot){savedEvents.at(binding.eventIndex)["displayName"]=card.at("displayName");savedEvents.at(binding.eventIndex)["recordedSlot"]=binding.slot;}
  }catch(const std::exception& ex){placementBindingError=ex.what();}
  {std::ofstream file(out/"input.json");file.exceptions(std::ios::badbit|std::ios::failbit);file<<json{{"width",windowSize.right},{"height",windowSize.bottom},{"events",inputEvents}}.dump();}
  std::ofstream config(out/"recording.json",std::ios::binary);config.exceptions(std::ios::badbit|std::ios::failbit);
  config<<json{{"schemaVersion",3},{"replayReady",false},{"inputReplayEligible",inputComplete&&!inputEvents.empty()},{"replayMode","experimental_input_with_world_placements"},{"placementBindingError",placementBindingError},{"events",savedEvents}}.dump(2);config.close();
  status(failed?"failed":"stopped",count,failureReason);return failed?1:0;
 }catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
}






