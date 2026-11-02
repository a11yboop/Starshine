#pragma once
#include "json.hpp"
#include <cmath>
#include <limits>
#include <map>
#include "road_navigation.hpp"
namespace autoplay {
// Enemy lanes are ordered from spawn to final; distance is measured along
// the route, so a later bend cannot be mistaken for the start of the lane.
inline nlohmann::json leadingEnemy(const nlohmann::json& enemies,const nlohmann::json& geometry){
 using namespace road_navigation;std::map<std::string,std::map<int,Vec3>> lanes;
 for(auto& o:geometry.at("objects")){auto path=o.at("path").get<std::string>();std::string prefix="_BASES/pve/LANES/";if(!path.starts_with(prefix))continue;
  auto rest=path.substr(prefix.size());auto slash=rest.find('/');if(slash==std::string::npos)continue;auto label=rest.substr(slash+1);int index;
  if(label=="spawn")index=0;else if(label=="final")index=1000000;else{if(label.empty()||label.find_first_not_of("0123456789")!=std::string::npos)continue;index=std::stoi(label);}
  auto p=o.at("position").get<std::array<double,3>>();lanes[rest.substr(0,slash)][index]={p[0],p[1],p[2]};
 }
 nlohmann::json leader;double best=1e30;
 for(auto& enemy:enemies){auto p=enemy.at("position").get<std::array<double,3>>();Vec3 point{p[0],p[1],p[2]};double nearest=1e30,remaining=1e30;
  for(auto& [name,points]:lanes){(void)name;if(!points.contains(0)||!points.contains(1000000))continue;std::vector<Vec3> nodes;for(auto& [i,v]:points){(void)i;nodes.push_back(v);}double tail=0;
   for(size_t i=nodes.size()-1;i>0;--i){Segment segment{nodes[i-1],nodes[i],0};auto q=closest(segment,point);double d=distance(point,q);if(d<nearest){nearest=d;remaining=tail+distance(q,nodes[i]);}tail+=distance(nodes[i-1],nodes[i]);}
  }
  if(nearest<=10&&remaining<best){best=remaining;leader=enemy;}
 }
 if(!enemies.empty()&&leader.is_null())throw std::runtime_error("Cannot locate leading enemy on an ordered lane");
 return leader.is_null()?nlohmann::json::array():nlohmann::json::array({leader});
}
inline double coverage(const nlohmann::json& position,const nlohmann::json& enemies){
 double score=0;for(const auto& enemy:enemies){const auto& p=enemy.at("position");double d=std::hypot(position[0].get<double>()-p[0].get<double>(),position[2].get<double>()-p[2].get<double>());score+=1.0/(1.0+d); }return score;
}
inline bool occupied(const nlohmann::json& p,const nlohmann::json& units){
 for(const auto& u:units){auto q=u.at("position");if(std::hypot(p[0].get<double>()-q[0].get<double>(),p[2].get<double>()-q[2].get<double>())<3)return true;}return false;
}
inline double upgradePriority(bool farm,const nlohmann::json& position,const nlohmann::json& enemies){
 return farm?std::numeric_limits<double>::infinity():coverage(position,enemies);
}
inline int choose(const nlohmann::json& candidates,const nlohmann::json& units,const nlohmann::json& enemies,double money){
 int best=-1;double score=-1;for(size_t i=0;i<candidates.size();++i){auto& c=candidates[i];if(c.at("cost").get<double>()>money||occupied(c.at("worldPosition"),units))continue;double s=coverage(c.at("worldPosition"),enemies);if(s>score){score=s;best=static_cast<int>(i);}}return best;
}
}
