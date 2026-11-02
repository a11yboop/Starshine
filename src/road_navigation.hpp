#pragma once
#include "placement_geometry.hpp"
#include <vector>
#include <queue>
#include <stdexcept>
namespace road_navigation {
using projection::Vec3;
struct Segment { Vec3 a,b; double halfWidth; };
inline double distance(Vec3 a,Vec3 b){return std::hypot(a.x-b.x,a.z-b.z);}
inline Vec3 closest(const Segment& s,Vec3 p){
 auto d=projection::subtract(s.b,s.a);double n=d.x*d.x+d.z*d.z;
 double t=n>0?std::clamp(((p.x-s.a.x)*d.x+(p.z-s.a.z)*d.z)/n,0.,1.):0.;
 return projection::add(s.a,projection::scale(d,t));
}
inline std::vector<Segment> segments(const std::vector<placement_geometry::Box>& boxes){
 std::vector<Segment> result;
 for(auto& b:boxes){if(!placement_geometry::validBox(b))continue;int axis=b.size.x>b.size.z?0:2;
  auto d=projection::column(b.rotation,axis);if(std::abs(d.y)>.3)continue;
  double length=axis==0?b.size.x:b.size.z;double width=axis==0?b.size.z:b.size.x;
  result.push_back({projection::add(b.center,projection::scale(d,-length/2)),projection::add(b.center,projection::scale(d,length/2)),width/2});
 }return result;
}
inline bool onRoad(const std::vector<Segment>& road,Vec3 p,double margin=.5){
 for(auto& s:road){auto q=closest(s,p);if(std::abs(q.y-p.y)<8&&distance(q,p)<=s.halfWidth+margin)return true;}return false;
}
inline bool covered(const std::vector<Segment>& road,Vec3 a,Vec3 b){
 int steps=std::max(1,static_cast<int>(std::ceil(distance(a,b)/.25)));
 for(int i=0;i<=steps;++i)if(!onRoad(road,projection::add(a,projection::scale(projection::subtract(b,a),double(i)/steps))))return false;return true;
}
inline std::vector<Vec3> route(const std::vector<Segment>& road,Vec3 start,Vec3 target){
 if(road.empty()||!onRoad(road,start))throw std::runtime_error("Character is outside the enemy road; move onto the road before navigation");
 std::vector<Vec3> nodes{start};
 for(auto& s:road){int count=std::max(1,static_cast<int>(std::ceil(distance(s.a,s.b))));for(int i=0;i<=count;++i)nodes.push_back(projection::add(s.a,projection::scale(projection::subtract(s.b,s.a),double(i)/count)));}
 size_t goal=1;for(size_t i=2;i<nodes.size();++i)if(distance(nodes[i],target)<distance(nodes[goal],target))goal=i;
 std::vector<double> costs(nodes.size(),1e30);std::vector<size_t> previous(nodes.size(),nodes.size());
 using Entry=std::pair<double,size_t>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;costs[0]=0;queue.push({0,0});
 while(!queue.empty()){auto [cost,i]=queue.top();queue.pop();if(cost!=costs[i])continue;if(i==goal)break;
  for(size_t j=1;j<nodes.size();++j){double d=distance(nodes[i],nodes[j]);if(d>1.6||std::abs(nodes[i].y-nodes[j].y)>8||!covered(road,nodes[i],nodes[j]))continue;
   if(cost+d<costs[j]){costs[j]=cost+d;previous[j]=i;queue.push({costs[j],j});}
  }
 }
 if(costs[goal]==1e30)throw std::runtime_error("Enemy road is disconnected; no road-only route available");
 std::vector<Vec3> result;for(size_t i=goal;i!=0;i=previous[i])result.push_back(nodes[i]);std::reverse(result.begin(),result.end());return result;
}
}
