#include "autoplay_policy.hpp"
#include <stdexcept>
#include <iostream>
using nlohmann::json;
void require(bool b){if(!b)throw std::runtime_error("autoplay policy regression");}
int main(){
 json geometry={{"objects",json::array({
  {{"path","_BASES/pve/LANES/1/spawn"},{"position",{0,0,0}}},
  {{"path","_BASES/pve/LANES/1/1"},{"position",{0,0,100}}},
  {{"path","_BASES/pve/LANES/1/2"},{"position",{10,0,100}}},
  {{"path","_BASES/pve/LANES/1/final"},{"position",{10,0,0}}}
 })}};
 json group=json::array();for(int i=0;i<30;++i)group.push_back({{"id","rear"},{"position",{0,0,5}}});
 group.push_back({{"id","front"},{"position",{10,0,50}}});
 auto front=autoplay::leadingEnemy(group,geometry);require(front.size()==1&&front[0]["id"]=="front");
 require(autoplay::upgradePriority(true,json::array({1000,0,1000}),group)>autoplay::upgradePriority(false,json::array({0,0,5}),group));
 require(autoplay::leadingEnemy(json::array(),geometry).empty());
 json candidates=json::array({{{"worldPosition",{0,3,0}},{"cost",100}},{{"worldPosition",{100,3,0}},{"cost",100}}});
 json units=json::array(),left=json::array({{{"position",{2,3,0}}}}),right=json::array({{{"position",{98,3,0}}}});
 require(autoplay::choose(candidates,units,left,100)==0);
 require(autoplay::choose(candidates,units,right,100)==1);
 require(autoplay::choose(candidates,units,left,99)==-1);
 units.push_back({{"position",{0,3,0}}});require(autoplay::choose(candidates,units,left,100)==1);
 units.push_back({{"position",{100,3,0}}});require(autoplay::choose(candidates,units,left,100)==-1);
 std::cout<<"PASS: enemy movement changes defense selection; affordability; occupied positions\n";
}
