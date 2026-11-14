#pragma once
#include "WiFi.h"
class WebServer {
  struct Route { std::string path; int method; std::function<void()> handler; };
  std::vector<Route> routes;
  std::function<void()> fallback;
  deviceMock::Request current;
public:
  explicit WebServer(int) {}
  template<class Handler> void on(const char *path, Handler handler) { on(path, HTTP_ANY, handler); }
  template<class Handler> void on(const char *path, int method, Handler handler) { routes.push_back({path, method, handler}); }
  template<class Handler> void onNotFound(Handler handler) { fallback = handler; }
  void begin() {}
  void stop() {}
  void handleClient() {
    if (deviceMock::requests.empty()) return;
    current = deviceMock::requests.front();
    deviceMock::requests.pop_front();
    for (auto &route : routes) {
      if (route.path == current.path && (route.method == HTTP_ANY || route.method == current.method)) {
        route.handler();
        return;
      }
    }
    if (fallback) fallback();
  }
  bool hasArg(const char *name) const { return current.args.count(name); }
  String arg(const char *name) const { return current.args.at(name); }
  void send(int status, const char *, const String &body) { deviceMock::responses.push_back({status, body}); }
  void send_P(int status, const char *type, const char *body) { send(status, type, body); }
  void sendHeader(const char *, const String &, bool) {}
};
