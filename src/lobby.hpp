#pragma once
// Read-only world feedback; all movement is normal keyboard input.
// Character coordinates are deliberately never serialized or printed.
inline void enterLobbyDoor(Reader& r,HWND window,InputCapture& stop){
 auto ws=r.find(r.dataModel(),"Workspace");auto doors=r.find(r.find(ws,"_LOBBIES"),"Story");
 auto position=[&](Address a){auto p=r.read<Address>(a+r.off("primitive"));auto v=r.read<std::array<float,3>>(p+r.off("position"));for(float n:v)if(!std::isfinite(n))throw std::runtime_error("Invalid door position");return projection::Vec3{v[0],v[1],v[2]};};
 Address selected=0;double best=1e30;auto current=r.navigationPosition();
 for(auto door:r.children(doors))try{if(r.read<Address>(r.find(door,"Owner")+r.off("value"))||!r.children(r.find(door,"Players")).empty())continue;auto p=position(r.find(door,"Out"));auto distance=std::hypot(current.x-p.x,current.z-p.z);if(distance<best){best=distance;selected=door;}}catch(const std::exception&){}
 if(!selected||best>180)throw std::runtime_error("No empty door nearby; use Areas > Play Area first");
 auto guard=[&](){if(stop.stopRequested())throw std::runtime_error("Lobby navigation stopped by F8");stop.drain();if(GetForegroundWindow()!=window)throw std::runtime_error("Lobby navigation lost focus");auto owner=r.read<Address>(r.find(selected,"Owner")+r.off("value"));if(owner&&owner!=r.localPlayer())throw std::runtime_error("Door was claimed by another player");return true;};
 InputSender input;auto pause=[&](int ms){for(int n=0;n<ms;n+=10){guard();std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
 for(auto point:{std::string("Out"),std::string("Teleport")}){
  auto target=position(r.find(selected,point));auto deadline=Clock::now()+std::chrono::seconds(30);int stuck=0;bool reached=false;
  while(Clock::now()<deadline){guard();if(r.visibleUiIdentity({"LevelSelectGui","MapSelect"})){std::cout<<"Map selection opened\n";return;}auto p=r.navigationPosition();double dx=target.x-p.x,dz=target.z-p.z;if(std::hypot(dx,dz)<2.5){reached=true;break;}
   auto camera=r.camera(window);auto forward=projection::column(camera.rotation,2),right=projection::column(camera.rotation,0);double fl=std::hypot(forward.x,forward.z),rl=std::hypot(right.x,right.z);if(fl<.05||rl<.05)throw std::runtime_error("Camera too vertical for lobby movement");double f=-(dx*forward.x+dz*forward.z)/fl,side=(dx*right.x+dz*right.z)/rl;WORD key=std::abs(f)>std::abs(side)?(f>0?'W':'S'):(side>0?'D':'A');
   input.send({{"type","key"},{"vk",key},{"down",true}},window,guard);pause(120);input.release();pause(70);auto next=r.navigationPosition();if(std::hypot(next.x-p.x,next.z-p.z)<.08){if(++stuck>10)throw std::runtime_error("Lobby path blocked");}else stuck=0;
  }if(!reached)throw std::runtime_error("Lobby approach timed out");
 }
 pause(800);if(!r.visibleUiIdentity({"LevelSelectGui","MapSelect"}))throw std::runtime_error("Door reached but map selector not confirmed");std::cout<<"Map selection opened\n";
}

inline void launchLobbyLevel(Reader& r,HWND window,InputCapture& stop,const json& profile){
 const auto world=profile.at("world").get<std::string>();const int act=profile.at("act").get<int>();
 const auto difficulty=profile.value("difficulty",std::string("Normal"));
 if(world.empty()||act<0||act>6||(difficulty!="Normal"&&difficulty!="Hard"))throw std::runtime_error("Invalid lobby profile");
 auto lobbyPlace=r.matchState().place;InputSender input;
 auto guard=[&](){if(stop.stopRequested())throw std::runtime_error("Lobby launch stopped by F8");stop.drain();if(GetForegroundWindow()!=window)throw std::runtime_error("Lobby launch lost focus");return true;};
 auto pause=[&](int ms){for(int i=0;i<ms;i+=20){guard();std::this_thread::sleep_for(std::chrono::milliseconds(20));}};
 auto wait=[&](std::vector<std::string> path){auto end=Clock::now()+std::chrono::seconds(15);while(!r.visibleUiIdentity(path)){guard();if(Clock::now()>end)throw std::runtime_error("Lobby menu not ready: "+path.back());pause(100);}pause(400);};
 auto center=[&](Address a){auto p=r.read<std::array<float,2>>(a+244),s=r.read<std::array<float,2>>(a+252);RECT rect{};GetClientRect(window,&rect);double x=p[0]+s[0]/2,y=p[1]+s[1]/2;if(!std::isfinite(x)||!std::isfinite(y)||s[0]<=0||s[1]<=0||x<0||y<0||x>=rect.right||y>=rect.bottom)throw std::runtime_error("Lobby button outside screen");return POINT{(LONG)std::lround(x),(LONG)std::lround(y)};};
 auto clickObject=[&](Address a){guard();auto p=center(a);auto readyEnd=Clock::now()+std::chrono::seconds(10);int stable=0;while(stable<1200){pause(100);auto next=center(a);stable=std::hypot(next.x-p.x,next.y-p.y)<=1?stable+100:0;p=next;if(Clock::now()>readyEnd)throw std::runtime_error("Lobby button animation did not settle");}input.hover(p.x,p.y,window,guard);pause(350);auto fresh=center(a);for(int adjust=0;std::hypot(fresh.x-p.x,fresh.y-p.y)>2&&adjust<5;++adjust){p=fresh;input.hover(p.x,p.y,window,guard);pause(350);fresh=center(a);}if(std::hypot(fresh.x-p.x,fresh.y-p.y)>2)throw std::runtime_error("Lobby button moved before click");input.send({{"type","mouse"},{"message",WM_LBUTTONDOWN},{"x",p.x},{"y",p.y},{"data",0}},window,guard,InputSender::CursorMode::Keep);pause(80);input.release();pause(300);};
 auto click=[&](std::vector<std::string> path){wait(path);clickObject(r.guiObject(path));};
 if(!r.visibleUiIdentity({"LevelSelectGui","MapSelect"})){
  click({"LeftUI_test","ButtonsRight","Teleports"});
  click({"TeleportsUI","Main","Main","Main","ScrollingOuter","Scroll","Play Area","ImageLabel"});
  pause(1800);enterLobbyDoor(r,window,stop);
 }
 wait({"LevelSelectGui","MapSelect"});
 // Scroll within the list until the requested card lies inside its viewport.
 auto container=r.guiObject({"LevelSelectGui","MapSelect","Main","Wrapper","Container"});auto card=r.find(container,world);
 auto view=r.guiObject({"LevelSelectGui","MapSelect","Main","Wrapper"});
 bool cardVisible=false;
 for(int i=0;i<35;++i){auto vp=r.read<std::array<float,2>>(view+244),vs=r.read<std::array<float,2>>(view+252),cp=r.read<std::array<float,2>>(card+244),cs=r.read<std::array<float,2>>(card+252);double cy=cp[1]+cs[1]/2;
  if(cy>vp[1]+12&&cy<vp[1]+vs[1]-12){cardVisible=true;break;}auto p=center(view);input.hover(p.x,p.y,window,guard);INPUT wheel{};wheel.type=INPUT_MOUSE;wheel.mi.dwFlags=MOUSEEVENTF_WHEEL;wheel.mi.mouseData=cy<vp[1]?WHEEL_DELTA:static_cast<DWORD>(-WHEEL_DELTA);guard();if(SendInput(1,&wheel,sizeof(wheel))!=1)throw std::runtime_error("Map list scroll failed");pause(150);
 }
 if(!cardVisible)throw std::runtime_error("Requested map did not become visible");clickObject(card);
 wait({"LevelSelectGui","LevelSelect"});
 const auto wanted=act?"Act "+std::to_string(act):std::string("Infinite");Address actButton=0;
 for(auto a:r.children(r.guiObject({"LevelSelectGui","LevelSelect","InfoFrame","LevelButtons"})))try{if((r.className(a)=="ImageButton"||r.className(a)=="TextButton")&&r.guiText(r.find(r.find(a,"Main"),"text"))==wanted){if(actButton)throw std::runtime_error("Ambiguous act");actButton=a;}}catch(const std::exception&){}
 if(!actButton)throw std::runtime_error("Requested act is not available");
 auto actView=r.guiObject({"LevelSelectGui","LevelSelect","InfoFrame","LevelButtons"});bool actVisible=false;
 for(int i=0;i<20;++i){auto vp=r.read<std::array<float,2>>(actView+244),vs=r.read<std::array<float,2>>(actView+252),bp=r.read<std::array<float,2>>(actButton+244),bs=r.read<std::array<float,2>>(actButton+252);double cy=bp[1]+bs[1]/2;if(cy>vp[1]+12&&cy<vp[1]+vs[1]-12){actVisible=true;break;}auto p=center(actView);input.hover(p.x,p.y,window,guard);INPUT wheel{};wheel.type=INPUT_MOUSE;wheel.mi.dwFlags=MOUSEEVENTF_WHEEL;wheel.mi.mouseData=cy<vp[1]?WHEEL_DELTA:static_cast<DWORD>(-WHEEL_DELTA);guard();if(SendInput(1,&wheel,sizeof(wheel))!=1)throw std::runtime_error("Act list scroll failed");pause(150);}
 if(!actVisible)throw std::runtime_error("Act not visible inside list");clickObject(actButton);
 if(act)click({"LevelSelectGui","LevelSelect","InfoFrame","Bottom","Difficulty","Items",difficulty,"Button"});
 click({"LevelSelectGui","LevelSelect","Buttons","Select"});
 click({"LevelSelectGui","Buttons","Start"});
 std::cout<<"Lobby Start sent for "<<world<<", "<<wanted<<"; waiting for level load\n"<<std::flush;
 auto end=Clock::now()+std::chrono::seconds(90);
 while(Clock::now()<end){guard();try{auto state=r.matchState();if(state.place!=lobbyPlace&&state.wave==0){std::cout<<"Level load confirmed\n"<<std::flush;return;}}catch(const std::exception&){}pause(200);}
 throw std::runtime_error("Level load not confirmed after Start");
}
