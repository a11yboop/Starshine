#include "../src/placement_binding.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using nlohmann::json;
using placement_binding::bindPlacements;
void require(bool good,const char* message){if(!good)throw std::runtime_error(message);}
json key(int time,int vk,bool down){return {{"type","key"},{"elapsedMs",time},{"vk",vk},{"down",down}};}
json mouse(int time,int message){return {{"type","mouse"},{"elapsedMs",time},{"message",message},{"x",1396},{"y",679}};}
json placed(int time,const char* type="unit_placed"){return {{"type",type},{"elapsedMs",time},{"unitName","bulma_summer:golden"}};}
json base(){return json::array({key(20379,'3',true),key(20520,'3',false),mouse(20722,512),mouse(20769,513),mouse(20847,514)});}
void refuses(const json& semantic,const json& input,const char* name){
 bool failed=false;try{bindPlacements(semantic,input);}catch(const std::exception&){failed=true;}require(failed,name);
}
int main(int argc,char** argv){
 try{
  auto semantic=json::array({placed(20000,"heartbeat"),placed(21208,"unit_discovered_after_gap")});
  auto input=base();auto bindings=bindPlacements(semantic,input);
  require(bindings.size()==1,"one known placement");const auto& b=bindings[0];
  require(b.eventIndex==1&&b.selectionDownIndex==0&&b.selectionUpIndex==1&&b.downIndex==3&&b.upIndex==4&&b.slot==3,"known indices and slot");
  require(bindPlacements(json::array({placed(30000,"unit_upgrade_changed")}),input).empty(),"upgrade does not bind");

  auto ambiguous=input;ambiguous.push_back(mouse(20900,513));ambiguous.push_back(mouse(20950,514));
  refuses(semantic,ambiguous,"two candidate clicks must refuse");
  auto missingRelease=input;missingRelease.erase(4);refuses(semantic,missingRelease,"missing click release");
  missingRelease=input;missingRelease.push_back(mouse(20900,513));refuses(semantic,missingRelease,"complete older click cannot hide missing release");
  auto doubleDown=json::array({key(20379,'3',true),key(20520,'3',false),mouse(20769,513),mouse(20800,513),mouse(20847,514)});
  refuses(semantic,doubleDown,"repeated down cannot borrow a later release");
  auto lateRelease=input;lateRelease[4]["elapsedMs"]=22000;refuses(semantic,lateRelease,"release after detection");
  auto missingSelection=input;missingSelection.erase(1);refuses(semantic,missingSelection,"selection held through click");
  missingSelection=base();missingSelection.erase(0);refuses(semantic,missingSelection,"missing selection down");
  auto cancelled=json::array({key(20379,'3',true),key(20520,'3',false),key(20600,'Q',true),mouse(20769,513),mouse(20847,514)});
  refuses(semantic,cancelled,"Q cancels selection");
  cancelled[2]={{"type","release_all"},{"elapsedMs",20600}};refuses(semantic,cancelled,"focus loss cancels selection");
  cancelled=json::array({key(20379,'3',true),key(20520,'3',false),mouse(20769,513),key(20800,'Q',true),mouse(20847,514)});
  refuses(semantic,cancelled,"Q during click");
  cancelled[3]=key(20800,'4',true);refuses(semantic,cancelled,"slot change during click");
  auto overlap=json::array({key(20379,'3',true),key(20400,'4',true),key(20500,'4',false),key(20520,'3',false),mouse(20769,513),mouse(20847,514)});
  refuses(semantic,overlap,"overlapping slot selections");
  auto changed=json::array({key(20300,'3',true),key(20350,'3',false),key(20400,'4',true),key(20520,'4',false),mouse(20769,513),mouse(20847,514)});
  require(bindPlacements(semantic,changed)[0].slot==4,"most recent completed selection wins");

  refuses(json::array({placed(21208),placed(21300)}),input,"duplicate semantic cannot reuse click");
  auto reuse=input;reuse.push_back(mouse(21400,513));reuse.push_back(mouse(21450,514));
  refuses(json::array({placed(21208),placed(21500)}),reuse,"selection cannot be reused");
  auto multiple=input;multiple.push_back(key(21300,'5',true));multiple.push_back(key(21350,'5',false));multiple.push_back(mouse(21400,513));multiple.push_back(mouse(21450,514));
  auto two=bindPlacements(json::array({placed(21208),placed(21500)}),multiple);
  require(two.size()==2&&two[1].slot==5&&two[1].downIndex==7&&two[1].selectionDownIndex==5,"separate close placements do not appear ambiguous");

  auto boundary=json::array({key(100,'1',true),key(101,'1',false),mouse(5100,513),mouse(5150,514)});
  require(bindPlacements(json::array({placed(6600)}),boundary).size()==1,"inclusive time bounds");
  refuses(json::array({placed(6601)}),boundary,"click too old");
  boundary[0]["elapsedMs"]=99;refuses(json::array({placed(6600)}),boundary,"selection too old");
  refuses(json::array({placed(20700)}),input,"future click cannot bind");
  auto unordered=input;unordered[1]["elapsedMs"]=1;refuses(semantic,unordered,"unordered input");
  auto malformed=input;malformed[0]["elapsedMs"]=20379.5;refuses(semantic,malformed,"fractional timestamp");
  malformed=input;malformed[0]["down"]=1;refuses(semantic,malformed,"malformed key state");
  malformed=input;malformed[0]["vk"]=std::numeric_limits<std::uint64_t>::max();refuses(semantic,malformed,"overflowing code");
  refuses(json::object(),input,"semantic object not array");
  require(bindPlacements(json::array(),json::array()).empty(),"empty recording");
  if(argc==2){
   const std::string directory=argv[1];
   auto recorded=json::parse(std::ifstream(directory+"/recording.json"));auto inputs=json::parse(std::ifstream(directory+"/input.json"));
   auto real=bindPlacements(recorded.at("events"),inputs.at("events"));
   require(real.size()==1&&real[0].slot==3,"saved recording has one slot 3 binding");
   require(inputs["events"][real[0].downIndex]["elapsedMs"]==20769&&inputs["events"][real[0].upIndex]["elapsedMs"]==20847,"saved placement click timestamps");
  }
  std::cout<<"PASS: placement/input binding; exact indices; ambiguity and cancellation refusal; completed pairs; reuse; multiple placements; time bounds; malformed input\n";
 }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
