#pragma once
#include <atomic>
#include <mutex>
#include <functional>

inline HWND gameWindow(DWORD pid){
 struct Search{DWORD pid;HWND found=nullptr;} s{pid};
 EnumWindows([](HWND w,LPARAM p)->BOOL{auto& s=*reinterpret_cast<Search*>(p);DWORD id=0;GetWindowThreadProcessId(w,&id);if(id==s.pid&&IsWindowVisible(w)&&!GetWindow(w,GW_OWNER)){s.found=w;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&s));
 if(!s.found)throw std::runtime_error("Visible Roblox window not found");return s.found;
}
inline bool gameplayKey(DWORD k){return (k>='1'&&k<='6')||k=='W'||k=='A'||k=='S'||k=='D'||k=='Q'||k=='E'||k=='R'||k=='F'||k==VK_SPACE||k==VK_LSHIFT||k==VK_RSHIFT||k==VK_UP||k==VK_DOWN||k==VK_LEFT||k==VK_RIGHT;}
inline LONG normalizedMouseAxis(LONG pixel,LONG origin,int extent){
 if(extent<=0||pixel<origin||static_cast<long long>(pixel)-origin>=extent)throw std::runtime_error("Mouse target outside virtual desktop");
 return static_cast<LONG>(((static_cast<long long>(pixel)-origin)*65536+32768)/extent);
}
inline INPUT gameplayKeyInput(WORD key,bool down){
 INPUT input{};input.type=INPUT_KEYBOARD;auto scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC_EX);
 if(!scan)throw std::runtime_error("Keyboard scan code unavailable");input.ki.wScan=static_cast<WORD>(scan&0xff);input.ki.dwFlags=KEYEVENTF_SCANCODE|(down?0:KEYEVENTF_KEYUP);
 if((scan&0xff00)==0xe000||(scan&0xff00)==0xe100)input.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;return input;
}
class InputCapture {
 inline static InputCapture* instance=nullptr;
 HWND window;std::mutex mutex;std::vector<json> pending;std::jthread worker;std::atomic<int> started{0};Clock::time_point epoch;
 std::set<DWORD> held;bool foreground=false;long long lastMove=-100;std::atomic<bool> stopPressed{false};
 long long ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-epoch).count();}
 void add(json e){e["elapsedMs"]=ms();std::lock_guard lock(mutex);pending.push_back(std::move(e));}
 static LRESULT CALLBACK keyboard(int code,WPARAM message,LPARAM ptr){
  auto* self=instance;if(code==HC_ACTION&&self){auto& k=*reinterpret_cast<KBDLLHOOKSTRUCT*>(ptr);if(k.vkCode==VK_F8&&!(k.flags&LLKHF_INJECTED)&&(message==WM_KEYDOWN||message==WM_SYSKEYDOWN))self->stopPressed=true;}
  if(code==HC_ACTION&&self&&!self->stopPressed&&GetForegroundWindow()==self->window){auto& k=*reinterpret_cast<KBDLLHOOKSTRUCT*>(ptr);
   if(!(k.flags&LLKHF_INJECTED)&&gameplayKey(k.vkCode)){bool down=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;if(down){if(self->held.insert(k.vkCode).second)self->add({{"type","key"},{"vk",k.vkCode},{"down",true}});}else if(self->held.erase(k.vkCode))self->add({{"type","key"},{"vk",k.vkCode},{"down",false}});}
  }return CallNextHookEx(nullptr,code,message,ptr);
 }
 static LRESULT CALLBACK mouse(int code,WPARAM message,LPARAM ptr){
  auto* self=instance;if(code==HC_ACTION&&self&&!self->stopPressed&&GetForegroundWindow()==self->window){auto& m=*reinterpret_cast<MSLLHOOKSTRUCT*>(ptr);POINT p=m.pt;RECT rect{};GetClientRect(self->window,&rect);ScreenToClient(self->window,&p);
   if(!(m.flags&LLMHF_INJECTED)&&PtInRect(&rect,p)){
    if(message==WM_MOUSEMOVE&&self->ms()-self->lastMove<25)return CallNextHookEx(nullptr,code,message,ptr);
    self->lastMove=self->ms();self->add({{"type","mouse"},{"message",message},{"x",p.x},{"y",p.y},{"data",static_cast<short>(HIWORD(m.mouseData))}});
   }
  }return CallNextHookEx(nullptr,code,message,ptr);
 }
 public:
 InputCapture(HWND w,Clock::time_point start):window(w),epoch(start){
  instance=this;worker=std::jthread([this](std::stop_token stop){
   HHOOK k=SetWindowsHookExW(WH_KEYBOARD_LL,keyboard,GetModuleHandleW(nullptr),0),m=SetWindowsHookExW(WH_MOUSE_LL,mouse,GetModuleHandleW(nullptr),0);
   started=(k&&m)?1:-1;
   while(!stop.stop_requested()&&started==1){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    bool active=GetForegroundWindow()==window;
    if(foreground&&!active){add({{"type","release_all"},{"reason","focus_lost"}});held.clear();}
    if(!foreground&&active){add({{"type","focus_resumed"}});}
    foreground=active;std::this_thread::sleep_for(std::chrono::milliseconds(2));
   }
   if(k)UnhookWindowsHookEx(k);if(m)UnhookWindowsHookEx(m);
  });while(started==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if(started<0){worker.join();instance=nullptr;throw std::runtime_error("Input capture hooks failed");}
 }
 ~InputCapture(){finish();}
 bool stopRequested()const{std::error_code ec;return stopPressed.load()||fs::exists("control/STOP",ec);}
 void finish(){if(worker.joinable()){worker.request_stop();worker.join();}instance=nullptr;}
 std::vector<json> drain(){std::lock_guard lock(mutex);std::vector<json> out;out.swap(pending);return out;}
};
class InputSender {
 std::set<WORD> held;std::set<DWORD> mouseHeld;
 bool refreshHover(HWND window,POINT target,const std::function<bool()>& guard){
  // A nonzero relative event refreshes game hover after absolute positioning.
  // Do not change Windows pointer speed/acceleration. Verify the actual endpoint.
  RECT client{};if(!GetClientRect(window,&client))throw std::runtime_error("Cannot read client rectangle");
  POINT local=target;ScreenToClient(window,&local);int direction=local.x<client.right/2?1:-1;
  for(int delta:{direction,-direction}){
   if(GetForegroundWindow()!=window)throw std::runtime_error("Hover cancelled: Roblox lost focus");
   if(guard&&!guard())return false;
   INPUT movement{};movement.type=INPUT_MOUSE;movement.mi.dx=delta;movement.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_MOVE_NOCOALESCE;
   if(SendInput(1,&movement,sizeof(movement))!=1)throw std::runtime_error("Relative hover movement rejected");
   for(int tick=0;tick<5;++tick){std::this_thread::sleep_for(std::chrono::milliseconds(10));if(guard&&!guard())return false;}
  }
  POINT actual{};if(!GetCursorPos(&actual)||std::abs(actual.x-target.x)>2||std::abs(actual.y-target.y)>2)throw std::runtime_error("Hover movement displaced cursor; click cancelled");
  return true;
 }
 public:
 enum class CursorMode { Position, Keep };
 ~InputSender(){release();}
 void release(){for(auto k:held){try{auto i=gameplayKeyInput(k,false);SendInput(1,&i,sizeof(i));}catch(...) {}}held.clear();for(auto flag:mouseHeld){INPUT i{};i.type=INPUT_MOUSE;i.mi.dwFlags=flag;SendInput(1,&i,sizeof(i));}mouseHeld.clear();}
 bool hover(int x,int y,HWND window,const std::function<bool()>& guard){
  if(guard&&!guard())return false;
  send({{"type","mouse"},{"message",WM_MOUSEMOVE},{"x",x},{"y",y},{"data",0}},window);
  POINT target{x,y};if(!ClientToScreen(window,&target))throw std::runtime_error("Client coordinate conversion failed");
  return refreshHover(window,target,guard);
 }
 bool send(const json& e,HWND window,const std::function<bool()>& beforeInput={},CursorMode cursorMode=CursorMode::Position){
  if(GetForegroundWindow()!=window)throw std::runtime_error("Replay stopped: Roblox lost focus");
  INPUT i{};
  if(e.at("type")=="key"){WORD k=e.at("vk");if(!gameplayKey(k))throw std::runtime_error("Unsupported key");i=gameplayKeyInput(k,e.at("down").get<bool>());}
  else if(e.at("type")=="mouse"){
   int x=e.at("x"),y=e.at("y");RECT r{};GetClientRect(window,&r);if(x<0||y<0||x>=r.right||y>=r.bottom)throw std::runtime_error("Mouse position outside client");
   POINT p{x,y};if(!ClientToScreen(window,&p))throw std::runtime_error("Client coordinate conversion failed");
   UINT msg=e.at("message");POINT current{};
   bool releasing=msg==WM_LBUTTONUP||msg==WM_RBUTTONUP||msg==WM_MBUTTONUP;
   bool releaseAtCurrentPosition=releasing&&GetCursorPos(&current)&&current.x==p.x&&current.y==p.y;
   if(cursorMode==CursorMode::Position&&!releaseAtCurrentPosition){
    INPUT movement{};movement.type=INPUT_MOUSE;
    movement.mi.dx=normalizedMouseAxis(p.x,GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_CXVIRTUALSCREEN));
    movement.mi.dy=normalizedMouseAxis(p.y,GetSystemMetrics(SM_YVIRTUALSCREEN),GetSystemMetrics(SM_CYVIRTUALSCREEN));
    movement.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|MOUSEEVENTF_MOVE_NOCOALESCE;
    if(SendInput(1,&movement,sizeof(movement))!=1)throw std::runtime_error("Mouse movement event rejected");
   }
   i.type=INPUT_MOUSE;
   if(msg==WM_LBUTTONDOWN||msg==WM_RBUTTONDOWN||msg==WM_MBUTTONDOWN||msg==WM_MOUSEWHEEL){
    if(cursorMode==CursorMode::Position&&mouseHeld.empty()&&!refreshHover(window,p,beforeInput))return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    POINT actual{};if(!GetCursorPos(&actual)||std::abs(actual.x-p.x)>2||std::abs(actual.y-p.y)>2)throw std::runtime_error("Cursor did not remain at target; click cancelled");
    if(GetForegroundWindow()!=window||GetAncestor(WindowFromPoint(actual),GA_ROOT)!=window)throw std::runtime_error("Another window covers the click target");
   }
   switch(msg){case WM_MOUSEMOVE:return true;case WM_LBUTTONDOWN:i.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;break;case WM_LBUTTONUP:i.mi.dwFlags=MOUSEEVENTF_LEFTUP;break;case WM_RBUTTONDOWN:i.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;break;case WM_RBUTTONUP:i.mi.dwFlags=MOUSEEVENTF_RIGHTUP;break;case WM_MBUTTONDOWN:i.mi.dwFlags=MOUSEEVENTF_MIDDLEDOWN;break;case WM_MBUTTONUP:i.mi.dwFlags=MOUSEEVENTF_MIDDLEUP;break;case WM_MOUSEWHEEL:i.mi.dwFlags=MOUSEEVENTF_WHEEL;i.mi.mouseData=static_cast<DWORD>(e.at("data").get<int>());break;default:throw std::runtime_error("Unsupported mouse event");}
  }else throw std::runtime_error("Unsupported input event");
  if(GetForegroundWindow()!=window)throw std::runtime_error("Focus changed before input; cancelled");
  if(beforeInput&&!beforeInput())return false;
  if(SendInput(1,&i,sizeof(i))!=1)throw std::runtime_error("Input rejected");
  if(i.type==INPUT_KEYBOARD){WORD key=e.at("vk");if(i.ki.dwFlags&KEYEVENTF_KEYUP)held.erase(key);else held.insert(key);}
  else switch(i.mi.dwFlags){
   case MOUSEEVENTF_LEFTDOWN:mouseHeld.insert(MOUSEEVENTF_LEFTUP);break;case MOUSEEVENTF_LEFTUP:mouseHeld.erase(MOUSEEVENTF_LEFTUP);break;
   case MOUSEEVENTF_RIGHTDOWN:mouseHeld.insert(MOUSEEVENTF_RIGHTUP);break;case MOUSEEVENTF_RIGHTUP:mouseHeld.erase(MOUSEEVENTF_RIGHTUP);break;
   case MOUSEEVENTF_MIDDLEDOWN:mouseHeld.insert(MOUSEEVENTF_MIDDLEUP);break;case MOUSEEVENTF_MIDDLEUP:mouseHeld.erase(MOUSEEVENTF_MIDDLEUP);break;
  }
  return true;
 }
};
