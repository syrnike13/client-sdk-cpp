# ICE socket retirement

The pinned WebRTC `webrtc-51ef663` bundle retains allocation sequences during
continual ICE gathering. When a failed network is regathered, its pruned ports
are destroyed, but the sequence's shared UDP socket remains open until the whole
session is destroyed. Repeated regathering therefore retains one socket per
failed network per cycle.

The SDK injects a BasicPortAllocator specialization that clears
`PORTALLOCATOR_ENABLE_SHARED_SOCKET` on each newly created session, after the
peer connection has applied its allocator defaults. UDP/STUN/TURN ports then
own their sockets and close them on destruction. This increases the initial
number of sockets where several port types were sharing one; it removes the
retained shared socket. It preserves continual gathering, network eligibility,
UDP/TCP/TURN availability, and the public API. This patch does not claim to
change WebRTC's retained allocation-sequence bookkeeping.

The factory injects the same BasicNetworkManager and BasicPacketSocketFactory
types used by WebRTC's default connection context. Their unique owners remain
in that context; each peer connection retains the factory while its allocator
borrows these dependencies. Failed factory construction is rejected before
using the borrowed pointers.

## Deterministic negative control

The following Windows probe links the actual prebuilt WebRTC archive directly;
it does not construct a Room, encoder, capture source, or media track. Every
network deliberately has no ICE connection. The probe keeps the same four
networks and ready ports while forcing eight failed-network regathers. It
shortens only the test ports' retirement timeout to zero. No machine network
settings or interfaces are changed.

Observed on Windows 11 build 26200, MSVC 14.51, 2026-09-06:

| Shared socket | Initial handles | After eight regathers | Ready ports throughout |
|---|---:|---:|---:|
| enabled (old ownership) | 149 | 181 | 4 |
| disabled (port ownership) | 143 | 143 | 4 |

The first process gained exactly four handles each cycle. The second stayed at
143 in all nine samples. Both completed session/allocator/network teardown.
The archive SHA-256 was `e89f95b37934a419f9d30199794c49fb3ef87942dff3bf413dec282d2f84c55c`.

The separate 375-second SDK-only connected Room control, using the same local
STUN/TURN setup, had maximum forward growth 13 (previous release: 16), and
returned to the same 266 handles after SDK shutdown. These Room counts contain
other SDK resources and are not the deterministic cause proof.

The application capture/encoder/audio/neutral-observer run completed all 30
WGC/DXGI switches: 58.37 decoded fps, p95 frame age 93 ms, zero Room reconnects,
maximum forward handle growth WGC 11 / DXGI 13 against the unchanged limit 16,
maximum duplication hold 23215 us against 50000 us, and final capture generation
count zero. These measurements used the private candidate before the additional
failed-factory guard; release-binary qualification remains separate.

## Reproduce against the pinned archive

Save the following source as `main.cpp` in an external diagnostic directory.
This is a consumer of an already built WebRTC archive; build the SDK itself with
its documented `build.cmd`. Use the SDK's existing `LK_CUSTOM_WEBRTC` bundle
or the exact downloaded `webrtc-51ef663` Windows x64 release bundle.

Save this `CMakeLists.txt` beside the source:

```cmake
cmake_minimum_required(VERSION 3.24)
project(port_retirement_probe LANGUAGES CXX)
if(NOT DEFINED WEBRTC_ROOT)
  message(FATAL_ERROR "Set WEBRTC_ROOT to the pinned prebuilt WebRTC bundle")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
add_executable(port_retirement_probe main.cpp)
target_compile_features(port_retirement_probe PRIVATE cxx_std_20)
target_compile_definitions(port_retirement_probe PRIVATE
  WEBRTC_WIN WEBRTC_LIBRARY_IMPL WIN32_LEAN_AND_MEAN NOMINMAX NDEBUG _HAS_EXCEPTIONS=0)
target_include_directories(port_retirement_probe PRIVATE
  "${WEBRTC_ROOT}/include"
  "${WEBRTC_ROOT}/include/third_party/abseil-cpp")
target_link_libraries(port_retirement_probe PRIVATE
  "${WEBRTC_ROOT}/lib/webrtc.lib"
  msdmo wmcodecdspuuid dmoguids crypt32 iphlpapi ole32 secur32 winmm
  ws2_32 strmiids d3d11 gdi32 dxgi dwmapi shcore)
```

From that external directory:

```powershell
cmake -S . -B build -A x64 -DWEBRTC_ROOT=C:/path/to/pinned-webrtc
cmake --build build --config Release
./build/Release/port_retirement_probe.exe shared
./build/Release/port_retirement_probe.exe owned
```

The number of networks can differ. Compare the per-cycle slope, require a
nonzero and stable ready-port count, and require `PORT_PROBE_DRAINED`. The
port-owned run must not retain one additional socket per network per regather.

```cpp
#include <winsock2.h>
#include <windows.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "api/environment/environment_factory.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/client/basic_port_allocator.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"

int main(int argc, char** argv) {
  const bool shared = argc > 1 && std::string(argv[1]) == "shared";
  WSADATA winsock{};
  if (WSAStartup(MAKEWORD(2, 2), &winsock)) return 2;
  webrtc::ThreadManager::Instance()->WrapCurrentThread();
  auto network = webrtc::Thread::CreateWithSocketServer();
  network->SetName("port-retirement-probe", nullptr);
  if (!network->Start()) return 3;
  const auto env = webrtc::CreateEnvironment();
  std::unique_ptr<webrtc::BasicNetworkManager> manager;
  std::unique_ptr<webrtc::BasicPacketSocketFactory> sockets;
  std::unique_ptr<webrtc::BasicPortAllocator> allocator;
  std::unique_ptr<webrtc::PortAllocatorSession> session;
  network->BlockingCall([&] {
    manager = std::make_unique<webrtc::BasicNetworkManager>(env, network->socketserver());
    sockets = std::make_unique<webrtc::BasicPacketSocketFactory>(network->socketserver());
    allocator = std::make_unique<webrtc::BasicPortAllocator>(env, manager.get(), sockets.get());
    allocator->set_flags(webrtc::PORTALLOCATOR_DISABLE_STUN |
                         webrtc::PORTALLOCATOR_DISABLE_RELAY |
                         webrtc::PORTALLOCATOR_DISABLE_TCP |
                         (shared ? webrtc::PORTALLOCATOR_ENABLE_SHARED_SOCKET : 0));
    allocator->Initialize();
    session = allocator->CreateSession("probe", 1, "probe-ufrag", "probe-password");
    session->StartGettingPorts();
  });
  std::this_thread::sleep_for(std::chrono::seconds(3));
  bool gathered = true;
  for (int cycle = 0; cycle <= 8; ++cycle) {
    auto ports = network->BlockingCall([&] { return session->ReadyPorts().size(); });
    DWORD handles = 0;
    GetProcessHandleCount(GetCurrentProcess(), &handles);
    std::cout << "PORT_SAMPLE {\"shared\":" << (shared ? "true" : "false")
              << ",\"cycle\":" << cycle << ",\"readyPorts\":" << ports
              << ",\"handles\":" << handles << "}" << std::endl;
    if (ports == 0) { gathered = false; break; }
    if (cycle == 8) break;
    network->BlockingCall([&] {
      // Shorten only the test port's normal retirement deadline. No ICE
      // connection exists, so every network is deliberately regathered.
      for (auto* port : session->ReadyPorts())
        static_cast<webrtc::Port*>(port)->set_timeout_delay(0);
      session->RegatherOnFailedNetworks();
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  network->BlockingCall([&] {
    session.reset();
    allocator.reset();
    sockets.reset();
    manager.reset();
  });
  network->Stop();
  network.reset();
  webrtc::ThreadManager::Instance()->UnwrapCurrentThread();
  WSACleanup();
  std::cout << "PORT_PROBE_DRAINED" << std::endl;
  return gathered ? 0 : 4;
}
```

