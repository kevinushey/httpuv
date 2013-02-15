#include "eventloop.hpp"
#include <stdio.h>
#include <map>
#include <string>
#include <uv.h>
#include "http.hpp"

// TODO: Re. R_ignore_SIGPIPE... there must be a better way!?

#ifndef _WIN32
extern int R_ignore_SIGPIPE;
#else
static int R_ignore_SIGPIPE;
#endif

std::string normalizeHeaderName(const std::string& name) {
  std::string result = name;
  for (std::string::iterator it = result.begin();
    it != result.end();
    it++) {
    if (*it == '-')
      *it = '_';
    else if (*it >= 'a' && *it <= 'z')
      *it = *it + ('A' - 'a');
  }
  return result;
}

class RWebApplication : public WebApplication {
private:
  Rcpp::Function _onRequest;
  Rcpp::Function _onWSOpen;
  Rcpp::Function _onWSMessage;
  Rcpp::Function _onWSClose;

public:
  RWebApplication(Rcpp::Function onRequest, Rcpp::Function onWSOpen,
                  Rcpp::Function onWSMessage, Rcpp::Function onWSClose) :
    _onRequest(onRequest), _onWSOpen(onWSOpen), _onWSMessage(onWSMessage),
    _onWSClose(onWSClose) {

  }

  virtual ~RWebApplication() {
  }

  virtual HttpResponse* getResponse(HttpRequest* pRequest) {
    using namespace Rcpp;

    std::string url = pRequest->url();
    size_t qsIndex = url.find('?');
    std::string path, queryString;
    if (qsIndex == std::string::npos)
      path = url;
    else {
      path = url.substr(0, qsIndex);
      queryString = url.substr(qsIndex);
    }

    Environment env = Rcpp::Function("new.env")();
    env["REQUEST_METHOD"] = pRequest->method();
    env["SCRIPT_NAME"] = std::string("");
    env["PATH_INFO"] = path;
    env["QUERY_STRING"] = queryString;

    env["rook.version"] = "0.0";
    env["rook.url_scheme"] = "http";

    std::vector<char> body = pRequest->body();
    RawVector input = RawVector(body.size());
    std::copy(body.begin(), body.end(), input.begin());
    env["rook.input"] = input;

    std::map<std::string, std::string, compare_ci> headers = pRequest->headers();
    for (std::map<std::string, std::string>::iterator it = headers.begin();
      it != headers.end();
      it++) {
      env["HTTP_" + normalizeHeaderName(it->first)] = it->second;
    }

    R_ignore_SIGPIPE = 0;
    RawVector responseBytes((_onRequest)(env));
    R_ignore_SIGPIPE = 1;
    std::vector<char> resp(responseBytes.size());
    resp.assign(responseBytes.begin(), responseBytes.end());
    return new HttpResponse(pRequest, 200, "OK", resp);
  }

  void onWSOpen(WebSocketConnection* pConn) {
    R_ignore_SIGPIPE = 0;
    _onWSOpen((intptr_t)pConn);
    R_ignore_SIGPIPE = 1;
  }

  void onWSMessage(WebSocketConnection* pConn, bool binary, const char* data, size_t len) {
    R_ignore_SIGPIPE = 0;
    if (binary)
      _onWSMessage((intptr_t)pConn, binary, std::vector<char>(data, data + len));
    else
      _onWSMessage((intptr_t)pConn, binary, std::string(data, len));
    R_ignore_SIGPIPE = 1;
  }
  
  void onWSClose(WebSocketConnection* pConn) {
    R_ignore_SIGPIPE = 0;
    _onWSClose((intptr_t)pConn);
    R_ignore_SIGPIPE = 1;
  }

};

// [[Rcpp::export]]
void sendWSMessage(intptr_t conn, bool binary, Rcpp::RObject message) {
  R_ignore_SIGPIPE = 1;
  WebSocketConnection* wsc = reinterpret_cast<WebSocketConnection*>(conn);
  if (binary) {
    Rcpp::RawVector raw = Rcpp::as<Rcpp::RawVector>(message);
    wsc->sendWSMessage(binary, reinterpret_cast<char*>(&raw[0]), raw.size());
  } else {
    std::string str = Rcpp::as<std::string>(message);
    wsc->sendWSMessage(binary, str.c_str(), str.size());
  }
  R_ignore_SIGPIPE = 0;
}

// [[Rcpp::export]]
void closeWS(intptr_t conn) {
  R_ignore_SIGPIPE = 1;
  std::cerr << "GOT HERE\n";
  WebSocketConnection* wsc = reinterpret_cast<WebSocketConnection*>(conn);
  wsc->closeWS();
  R_ignore_SIGPIPE = 0;
}

// [[Rcpp::export]]
intptr_t makeServer(const std::string& host, int port,
                    Rcpp::Function onRequest, Rcpp::Function onWSOpen,
                    Rcpp::Function onWSMessage, Rcpp::Function onWSClose) {

  using namespace Rcpp;
  // Deleted when owning pHandler is deleted
  // TODO: When is this deleted??
  RWebApplication* pHandler = 
    new RWebApplication(onRequest, onWSOpen, onWSMessage, onWSClose);
  uv_tcp_t* pServer = createServer(
    uv_default_loop(), host.c_str(), port, (WebApplication*)pHandler);

  std::cerr << "makeServer " << (intptr_t)pServer << "\n";
  return (intptr_t)pServer;
}

// [[Rcpp::export]]
void destroyServer(intptr_t handle) {
  std::cerr << "destroyServer " << handle << "\n";
  freeServer((uv_tcp_t*)handle);
}

// [[Rcpp::export]]
bool runNB() {
  void (*origHandler)(int);
  R_ignore_SIGPIPE = 1;
  bool result = runNonBlocking(uv_default_loop());
  R_ignore_SIGPIPE = 0;
  return result;
}