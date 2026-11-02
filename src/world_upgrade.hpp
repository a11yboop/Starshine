#pragma once
#include <algorithm>
#include <cctype>
#include <regex>

namespace world_upgrade {

// Remove only balanced, known formatting tags. Markup attributes never become
// numeric text, and malformed/unknown markup is refused instead of guessed.
inline std::string plainText(const std::string& text){
 if(text.size()>4096)throw std::runtime_error("Upgrade UI text is unexpectedly long");
 std::string result;std::vector<std::string> tags;
 for(std::size_t i=0;i<text.size();){
  if(text[i]!='<'){result.push_back(text[i++]);continue;}
  std::size_t end=i+1;char quote=0;
  for(;end<text.size();++end){
   char c=text[end];if(quote){if(c==quote)quote=0;continue;}
   if(c=='\''||c=='\"'){quote=c;continue;}
   if(c=='<')throw std::runtime_error("Malformed upgrade rich text");
   if(c=='>')break;
  }
  if(end==text.size()||quote)throw std::runtime_error("Unclosed upgrade rich text");
  std::string tag=text.substr(i+1,end-i-1);bool closing=!tag.empty()&&tag.front()=='/';
  std::size_t start=closing?1:0,nameEnd=start;
  while(nameEnd<tag.size()&&std::isalpha(static_cast<unsigned char>(tag[nameEnd])))++nameEnd;
  std::string name=tag.substr(start,nameEnd-start);
  if(nameEnd<tag.size()&&!std::isspace(static_cast<unsigned char>(tag[nameEnd]))&&tag[nameEnd]!='/')throw std::runtime_error("Malformed upgrade tag");
  if(name=="br"&&!closing){result.push_back(' ');i=end+1;continue;}
  if(name!="b"&&name!="i"&&name!="u"&&name!="s"&&name!="font"&&name!="stroke"&&name!="smallcaps"&&name!="uppercase")throw std::runtime_error("Unsupported upgrade rich text tag");
  if(closing){
   for(std::size_t j=nameEnd;j<tag.size();++j)if(!std::isspace(static_cast<unsigned char>(tag[j])))throw std::runtime_error("Malformed closing upgrade tag");
   if(tags.empty()||tags.back()!=name)throw std::runtime_error("Mismatched upgrade rich text");tags.pop_back();
  }else tags.push_back(name);
  i=end+1;
 }
 if(!tags.empty())throw std::runtime_error("Unclosed upgrade formatting");
 auto first=result.find_first_not_of(" \t\r\n"),last=result.find_last_not_of(" \t\r\n");
 return first==std::string::npos?std::string{}:result.substr(first,last-first+1);
}

inline double parseCost(const std::string& text){
 const auto plain=plainText(text);
 // Complete match: no MAX, decimals, malformed grouping, second number, or
 // loose digit stripping. UTF-8 C2 A5 is the yen sign used by this UI.
 static const std::regex pattern("^(?:Upgrade[ \\t]*:[ \\t]*)?((?:0|[1-9][0-9]*|[1-9][0-9]{0,2}(?:,[0-9]{3})+))[ \\t]*(?:\xC2\xA5)?$",std::regex::icase);
 std::smatch match;if(!std::regex_match(plain,match,pattern))throw std::runtime_error("Upgrade cost text is not a single supported amount: "+plain);
 std::uint64_t amount=0;
 for(char c:match[1].str())if(c!=','){
  amount=amount*10+static_cast<unsigned>(c-'0');
  if(amount>1000000000000ULL)throw std::runtime_error("Upgrade cost is out of range");
 }
 return static_cast<double>(amount);
}
} // namespace world_upgrade

// Included after Reader, world_placement.hpp and their shared types.
inline void upgradeWorldUnit(Reader& reader,HWND window,InputCapture& control,const json& event,const json& geometry,const json& aliases,const fs::path& reportFile,bool waitForFunds=true,double reserve=0){
 const auto unitName=event.at("unitName").get<std::string>();const auto target=worldVector(event.at("worldPosition"));
 if(!event.at("upgrade").is_number_integer())throw std::runtime_error("Invalid desired upgrade level");
 const int desired=event.at("upgrade").get<int>();if(desired<0||desired>1000)throw std::runtime_error("Invalid desired upgrade level");
 const auto place=geometry.at("placeId").get<Address>();
 (void)aliases;


 InputSender sender;Address address=0;
 json report={{"unitName",unitName},{"targetRoot",event.at("worldPosition")},{"desiredUpgrade",desired},{"state","preflight"},{"playerPositionRecorded",false}};
 auto save=[&](){std::ofstream f(reportFile);f.exceptions(std::ios::badbit|std::ios::failbit);f<<report.dump(2);};
 auto guard=[&](){
  if(control.stopRequested())throw std::runtime_error("World upgrade stopped by F8");control.drain();
  if(GetForegroundWindow()!=window)throw std::runtime_error("World upgrade stopped: Roblox lost focus");
  if(reader.matchState().place!=place)throw std::runtime_error("World upgrade stopped: map changed");
  if(reader.visibleUiIdentity({"ResultsUI","Holder"})||reader.visibleUiIdentity({"ResultsUI","Finished"}))throw std::runtime_error("World upgrade stopped: match has ended");
  return true;
 };
 auto pause=[&](int ms){auto end=Clock::now()+std::chrono::milliseconds(ms);while(Clock::now()<end){guard();std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
 auto stableSnapshot=[&](){
  auto deadline=Clock::now()+std::chrono::seconds(3);std::string lastError;
  while(true){
   guard();
   try{return reader.snapshot(address);}catch(const std::exception& ex){lastError=ex.what();}
   if(Clock::now()>=deadline)throw std::runtime_error("Upgrade target read did not recover: "+lastError);
   pause(50);
  }
 };
 auto distance=[&](const Unit& unit){return std::hypot(std::hypot(unit.position[0]-target.x,unit.position[1]-target.y),unit.position[2]-target.z);};
 auto targetUnit=[&](const Snapshot& snapshot)->Unit{
  auto found=snapshot.units.find(address);
  if(found==snapshot.units.end()||found->second.name!=unitName||distance(found->second)>0.75)throw std::runtime_error("Bound unit disappeared or changed identity/position");
  std::size_t matches=0;for(const auto& [otherAddress,unit]:snapshot.units){(void)otherAddress;if(unit.name==unitName&&distance(unit)<=0.75)++matches;}
  if(matches!=1)throw std::runtime_error("Owned unit at the upgrade target is ambiguous");
  return found->second;
 };
 auto click=[&](POINT point,const std::function<bool()>& actionGuard){
  if(!sender.hover(point.x,point.y,window,actionGuard))throw std::runtime_error("Upgrade hover cancelled");pause(150);
  if(!sender.send({{"type","mouse"},{"message",WM_LBUTTONDOWN},{"x",point.x},{"y",point.y},{"data",0}},window,actionGuard,InputSender::CursorMode::Keep))throw std::runtime_error("Upgrade click cancelled");
  pause(80);sender.send({{"type","mouse"},{"message",WM_LBUTTONUP},{"x",point.x},{"y",point.y},{"data",0}},window,{},InputSender::CursorMode::Keep);
 };
 try{
  save();guard();const auto before=stableSnapshot();
  for(const auto& [candidate,unit]:before.units)if(unit.name==unitName&&distance(unit)<=0.75){if(address)throw std::runtime_error("Multiple owned units match the upgrade target");address=candidate;}
  if(!address)throw std::runtime_error("Owned unit not found at recorded upgrade position");
  auto initial=targetUnit(before);report["actualRoot"]=initial.position;report["fromUpgrade"]=initial.upgrade;
  if(initial.upgrade>=desired){report["state"]="already_upgraded";save();return;}
  if(desired!=initial.upgrade+1)throw std::runtime_error("Recorded upgrade gap: only one confirmed upgrade step is supported");
  approachWorldTarget(reader,window,control,target,guard,geometry);
  const auto camera=reader.camera(window);auto pixel=projection::project(camera,{initial.position[0],initial.position[1],initial.position[2]});
  if(!pixel)throw std::runtime_error("Unit root is outside the current camera view");
  POINT unitPoint{static_cast<LONG>(std::clamp(std::lround(pixel->x),0L,static_cast<long>(camera.width-1))),static_cast<LONG>(std::clamp(std::lround(pixel->y),0L,static_cast<long>(camera.height-1)))};
  auto selectGuard=[&](){
   guard();auto unit=targetUnit(stableSnapshot());
   if(unit.upgrade!=initial.upgrade)throw std::runtime_error("Unit upgraded externally during selection");
   auto projected=projection::project(reader.camera(window),{unit.position[0],unit.position[1],unit.position[2]});
   if(!projected||std::hypot(projected->x-unitPoint.x,projected->y-unitPoint.y)>2)throw std::runtime_error("Camera/unit moved before selection");
   return true;
  };
  report["unitScreenTarget"]={unitPoint.x,unitPoint.y};report["state"]="selecting_unit";save();click(unitPoint,selectGuard);pause(200);
  // Clicking an already-selected unit toggles its panel closed. Reopen the
  // same bound unit once; never read stale price text from an invisible panel.
  pause(300);
  if(!reader.visibleUiIdentity({"UnitUpgrade2","Main"})){click(unitPoint,selectGuard);pause(500);}
  if(!reader.visibleUiIdentity({"UnitUpgrade2","Main"}))throw std::runtime_error("Unit selection panel did not open; no R sent");
  auto prices=json::parse(std::ifstream("config/upgrade-costs.json"));
  double required=-1;
  auto priceDeadline=Clock::now()+std::chrono::seconds(3);
  while(Clock::now()<priceDeadline){
   guard();
   try{
    const auto title=world_upgrade::plainText(reader.guiText(reader.guiObject({"UnitUpgrade2","Main","Main","UnitName"})));
    if(!aliases.contains(unitName)||title!=aliases.at(unitName).get<std::string>())throw std::runtime_error("Selected unit title is not confirmed");
    required=world_upgrade::parseCost(reader.guiText(reader.guiObject({"UnitUpgrade2","Main","Main","Buttons","Upgrade","Main","Text"})));
    report["priceSource"]="selected_unit_ui";break;
   }catch(const std::exception&){pause(100);}
  }
  if(required<0){
   if(!prices.contains(unitName)||!prices.at(unitName).contains(std::to_string(initial.upgrade)))throw std::runtime_error("Cannot read selected unit upgrade price and no configured price exists; no R sent");
   required=prices.at(unitName).at(std::to_string(initial.upgrade)).get<double>();report["priceSource"]="configuration";
  }
  if(!std::isfinite(required)||required<0)throw std::runtime_error("Invalid configured upgrade cost");
  report["requiredCurrency"]=required;
  if(!waitForFunds&&reader.currency()<required+reserve){report["state"]="deferred_currency";save();return;}
  auto fundsDeadline=Clock::now()+std::chrono::seconds(90);
  while(reader.currency()<required){guard();if(Clock::now()>fundsDeadline)throw std::runtime_error("Not enough currency for R upgrade");pause(100);}
  auto upgradeGuard=[&](){guard();if(!reader.visibleUiIdentity({"UnitUpgrade2","Main"}))throw std::runtime_error("Unit panel closed before R");auto unit=targetUnit(stableSnapshot());if(unit.upgrade!=initial.upgrade)throw std::runtime_error("Unit upgrade changed before R");return true;};
  upgradeGuard();report["input"]="R";report["state"]="upgrade_key_pending";save();
  sender.send({{"type","key"},{"vk",'R'},{"down",true}},window,upgradeGuard);
  pause(100);
  sender.send({{"type","key"},{"vk",'R'},{"down",false}},window,guard);
  report["state"]="upgrade_key_sent";save();
  auto confirmDeadline=Clock::now()+std::chrono::seconds(4);std::string lastReadError;
  while(Clock::now()<confirmDeadline){
   guard();std::optional<Snapshot> after;
   try{after=reader.snapshot(address);}catch(const std::exception& ex){lastReadError=ex.what();}
   if(after){
    auto unit=targetUnit(*after);report["actualUpgrade"]=unit.upgrade;report["actualRoot"]=unit.position;
    if(unit.upgrade==desired){report["state"]="confirmed";save();std::cout<<"World upgrade confirmed: "<<unitName<<", "<<initial.upgrade<<" -> "<<unit.upgrade<<"\n"<<std::flush;return;}
    if(unit.upgrade!=initial.upgrade)throw std::runtime_error("Bound unit reached an unexpected upgrade level; no retry attempted");
   }
   pause(50);
  }
  report["lastReadError"]=lastReadError;throw std::runtime_error("R upgrade was not confirmed on the bound unit; check currency/selection; no retry attempted");
 }catch(const std::exception& ex){report["error"]=ex.what();report["state"]="failed";save();throw;}
}




