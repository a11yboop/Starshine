#include "road_navigation.hpp"
#include <cassert>
#include <iostream>
int main(){
 using namespace road_navigation;
 std::vector<Segment> road{{{0,0,0},{0,0,10},1},{{0,0,10},{10,0,10},1},{{10,0,10},{10,0,0},1}};
 auto path=route(road,{0,3,0},{10,0,0});double length=0;Vec3 last{0,3,0};bool wentAround=false;
 for(auto p:path){assert(covered(road,last,p));length+=distance(last,p);wentAround|=p.z>9;last=p;}
 assert(wentAround&&length>25);assert(!covered(road,{0,0,0},{10,0,0}));
 bool rejected=false;try{route(road,{5,0,5},{10,0,0});}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 rejected=false;try{route({{{0,0,0},{0,0,5},1},{{10,0,0},{10,0,5},1}},{0,0,0},{10,0,0});}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 auto segmentsResult=segments({{{0,0,0},{2,1,10},{1,0,0,0,1,0,0,0,1}}});assert(segmentsResult.size()==1&&distance(segmentsResult[0].a,segmentsResult[0].b)==10);
 std::cout<<"Road navigation checks passed\n";
}
