#pragma once
#include <cstdint>

class UiReadinessGate {
 std::uint64_t identity_=0;
 std::int64_t since_=0,lastSample_=0;
 public:
 static constexpr std::int64_t settleMs=1200;
 static constexpr std::int64_t maxSampleGapMs=300;
 void reset(){identity_=0;since_=0;lastSample_=0;}
 bool observe(std::uint64_t visibleIdentity,std::int64_t nowMs){
  if(!visibleIdentity){reset();return false;}
  if(visibleIdentity!=identity_||nowMs<lastSample_||nowMs-lastSample_>maxSampleGapMs){
   identity_=visibleIdentity;since_=nowMs;lastSample_=nowMs;return false;
  }
  lastSample_=nowMs;
  return nowMs-since_>=settleMs;
 }
};
