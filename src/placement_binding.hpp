#pragma once
#include "json.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace placement_binding {

struct PlacementBinding {
 std::size_t eventIndex,downIndex,upIndex,selectionDownIndex,selectionUpIndex;
 int slot;
};

namespace detail {
using json=nlohmann::json;
constexpr int leftDown=0x0201,leftUp=0x0202;
struct Input {
 std::int64_t time;
 std::string type;
 int key=0,message=0;
 bool down=false;
};
inline std::int64_t integer(const json& value,const char* field){
 if(!value.is_number_integer()||
    (value.is_number_unsigned()&&value.get<std::uint64_t>()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())))
  throw std::runtime_error(std::string("Placement binding: invalid ")+field);
 return value.get<std::int64_t>();
}
inline std::vector<std::int64_t> times(const json& events,const char* label){
 if(!events.is_array())throw std::runtime_error(std::string("Placement binding: ")+label+" must be an array");
 std::vector<std::int64_t> result;
 std::int64_t previous=-1;
 for(const auto& event:events){
  if(!event.is_object()||!event.contains("elapsedMs")||!event.contains("type")||!event.at("type").is_string())
   throw std::runtime_error(std::string("Placement binding: malformed ")+label+" event");
  auto time=integer(event.at("elapsedMs"),"elapsedMs");
  if(time<0||time<previous)throw std::runtime_error(std::string("Placement binding: unordered/negative ")+label+" time");
  result.push_back(time);previous=time;
 }
 return result;
}
inline bool slotDown(const Input& input){return input.type=="key"&&input.down&&input.key>='1'&&input.key<='6';}
inline bool cancel(const Input& input){return input.type=="release_all"||(input.type=="key"&&input.down&&input.key=='Q');}
inline bool mouse(const Input& input,int message){return input.type=="mouse"&&input.message==message;}
inline std::runtime_error refusal(std::size_t index,const std::string& reason){
 return std::runtime_error("Placement event "+std::to_string(index)+" cannot be bound: "+reason);
}
} // namespace detail

// Pure, all-or-nothing binding. No input or memory access occurs here.
// Both arguments are event arrays in chronological order. A placement requires
// a completed 1..6 selection, followed by one unambiguous completed left click.
// Bounds are inclusive: selection-down to click-down <=5 s, click-down to
// semantic detection <=1.5 s. Used selection/click pairs cannot be reused.
inline std::vector<PlacementBinding> bindPlacements(const nlohmann::json& semanticEvents,const nlohmann::json& inputEvents){
 using namespace detail;
 const auto semanticTimes=times(semanticEvents,"semantic"),inputTimes=times(inputEvents,"input");
 std::vector<Input> inputs;
 for(std::size_t i=0;i<inputEvents.size();++i){
  const auto& event=inputEvents[i];Input input{inputTimes[i],event.at("type").get<std::string>()};
  if(input.type=="key"){
   if(!event.contains("vk")||!event.contains("down")||!event.at("down").is_boolean())throw std::runtime_error("Placement binding: malformed key event");
   auto key=integer(event.at("vk"),"vk");
   if(key<0||key>255)throw std::runtime_error("Placement binding: invalid key code");
   input.key=static_cast<int>(key);input.down=event.at("down").get<bool>();
  }else if(input.type=="mouse"){
   if(!event.contains("message"))throw std::runtime_error("Placement binding: missing mouse message");
   auto message=integer(event.at("message"),"message");
   if(message<0||message>65535)throw std::runtime_error("Placement binding: invalid mouse message");
   input.message=static_cast<int>(message);
  }else if(input.type!="release_all"&&input.type!="focus_resumed")throw std::runtime_error("Placement binding: unsupported input event");
  inputs.push_back(input);
 }
 std::set<std::size_t> used;
 std::vector<PlacementBinding> result;
 for(std::size_t eventIndex=0;eventIndex<semanticEvents.size();++eventIndex){
  const auto type=semanticEvents[eventIndex].at("type").get<std::string>();
  if(type!="unit_placed"&&type!="unit_discovered_after_gap")continue;
  const auto eventTime=semanticTimes[eventIndex];
  std::vector<PlacementBinding> candidates;
  bool incompleteCandidate=false;
  std::string reason="no completed placement click within 1500 ms";
  for(std::size_t downIndex=0;downIndex<inputs.size();++downIndex){
   const auto& click=inputs[downIndex];
   if(click.time>eventTime)break;
   if(!mouse(click,leftDown)||eventTime-click.time>1500)continue;
   if(used.contains(downIndex)){reason="input already bound to another placement";continue;}
   std::optional<std::size_t> selectionDown;
   for(std::size_t j=downIndex;j>0;){--j;if(slotDown(inputs[j])){selectionDown=j;break;}}
   if(!selectionDown||click.time-inputs[*selectionDown].time>5000){reason="no recent slot selection within 5000 ms";continue;}
   std::optional<std::size_t> selectionUp;
   bool invalid=false;
   const int key=inputs[*selectionDown].key;
   // An overlapping selection key still held from another slot is ambiguous.
   std::set<int> selectedKeys;
   for(std::size_t j=0;j<=*selectionDown;++j){
    const auto& input=inputs[j];
    if(input.type=="release_all")selectedKeys.clear();
    if(input.type=="key"&&input.key>='1'&&input.key<='6'){
     if(input.down){if(j==*selectionDown&&!selectedKeys.empty())invalid=true;selectedKeys.insert(input.key);}
     else selectedKeys.erase(input.key);
    }
   }
   for(std::size_t j=*selectionDown+1;j<downIndex;++j){
    const auto& input=inputs[j];
    if(cancel(input)||slotDown(input)){invalid=true;break;}
    if(input.type=="key"&&input.key==key&&!input.down){if(selectionUp){invalid=true;break;}selectionUp=j;}
   }
   if(invalid||!selectionUp){reason="slot changed/cancelled or selection release is missing";continue;}
   std::optional<std::size_t> upIndex;
   for(std::size_t j=downIndex+1;j<inputs.size()&&inputs[j].time<=eventTime;++j){
    if(cancel(inputs[j])||slotDown(inputs[j])||mouse(inputs[j],leftDown))break;
    if(mouse(inputs[j],leftUp)){upIndex=j;break;}
   }
   if(!upIndex){incompleteCandidate=true;reason="matching mouse release missing before placement, or click cancelled";continue;}
   if(used.contains(*selectionDown)||used.contains(*selectionUp)||used.contains(*upIndex)){
    reason="selection/input already bound to another placement";continue;
   }
   candidates.push_back({eventIndex,downIndex,*upIndex,*selectionDown,*selectionUp,key-'0'});
  }
  // An incomplete second attempt could be the placement being detected. Do
  // not silently select an older complete click merely because it has an up.
  if(incompleteCandidate)throw refusal(eventIndex,"incomplete/cancelled candidate click makes placement uncertain");
  if(candidates.empty())throw refusal(eventIndex,reason);
  if(candidates.size()!=1)throw refusal(eventIndex,"multiple candidate clicks are ambiguous");
  const auto binding=candidates.front();
  for(auto index:{binding.downIndex,binding.upIndex,binding.selectionDownIndex,binding.selectionUpIndex})used.insert(index);
  result.push_back(binding);
 }
 return result;
}
} // namespace placement_binding
