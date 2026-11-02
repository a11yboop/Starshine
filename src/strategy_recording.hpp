#pragma once
#include "json.hpp"

// A recording can begin on a results screen from the preceding round.
// Discard that round only when a fresh, empty round was actually observed.
inline nlohmann::json strategyEvents(const nlohmann::json& events){
 for(size_t i=0;i<events.size();++i){
  const auto& e=events[i];
  if(e.value("type",std::string{})=="heartbeat"&&e.value("wave",-1)==0&&e.value("ownedUnits",-1)==0){
   auto result=nlohmann::json::array({{{"type","baseline"},{"wave",0},{"ownedUnits",0},{"elapsedMs",0}}});
   for(size_t j=i;j<events.size();++j)if(events[j].at("type")!="baseline_unit")result.push_back(events[j]);
   return result;
  }
 }
 return events;
}
