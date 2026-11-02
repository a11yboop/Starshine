#include "strategy_recording.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using nlohmann::json;
void require(bool v){if(!v)throw std::runtime_error("strategy normalization regression");}
int main(int argc,char** argv){
 auto old=json::array({{{"type","baseline"},{"wave",7},{"ownedUnits",1}},{{"type","baseline_unit"}},{{"type","unit_sold"}},{{"type","heartbeat"},{"wave",0},{"ownedUnits",0}},{{"type","unit_placed"},{"wave",1}}});
 auto copy=old;auto result=strategyEvents(old);
 require(old==copy);require(result.size()==3);require(result[0]["wave"]==0);require(result[0]["ownedUnits"]==0);require(result[2]["type"]=="unit_placed");
 auto noReset=json::array({old[0],old[1]});require(strategyEvents(noReset)==noReset);
 auto fresh=json::array({{{"type","baseline"},{"wave",0},{"ownedUnits",0}},old[3],old[4]});require(strategyEvents(fresh).size()==3);
 std::cout<<"Strategy round normalization tests passed\n";
 if(argc>1){auto recording=json::parse(std::ifstream(argv[1]));auto original=recording;auto events=strategyEvents(recording.at("events"));require(events[0]["wave"]==0);require(events[0]["ownedUnits"]==0);int placements=0,upgrades=0;for(auto& e:events){placements+=e["type"]=="unit_placed"||e["type"]=="unit_discovered_after_gap";upgrades+=e["type"]=="unit_upgrade_changed";}require(recording==original);require(placements>0&&upgrades>0);std::cout<<"Saved recording: "<<placements<<" placements, "<<upgrades<<" upgrades; fresh-round baseline; source unchanged\n";}
}
