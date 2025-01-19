#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include <chrono>
#include <map>
#include <memory>
#include "./include/crow_all.h"

using namespace cv;
using namespace std;
using namespace boost::asio;
using namespace std::chrono;

struct CameraConfig
{
  string url;
  int frameRate;
  bool active;
  Mat currentFrame;
  mutex frameMutex;
  condition_variable frameAvailable;
};

class CameraService
{
private:
  map<int, shared_ptr<CameraConfig>> cameras;
  map<int, unique_ptr<VideoCapture>> captures;
  map<int, thread> captureThreads;
  atomic<int> nextCameraId{1};

  void captureFramesFromCamera(int cameraId, shared_ptr<CameraConfig> config)
  {
    auto &cap = captures[cameraId];
    while (config->active)
    {
      Mat frame;
      cap->read(frame);

      if (frame.empty())
      {
        cerr << "Error: Empty frame from camera " << cameraId << endl;
        continue;
      }

      {
        lock_guard<mutex> lock(config->frameMutex);
        frame.copyTo(config->currentFrame);
      }
      config->frameAvailable.notify_all();
      this_thread::sleep_for(milliseconds(1000 / config->frameRate));
    }
  }

public:
  int addCamera(const string &url, int frameRate)
  {
    int id = nextCameraId++;

    auto cap = make_unique<VideoCapture>(url);
    if (!cap->isOpened())
    {
      throw runtime_error("Failed to open camera: " + url);
    }

    auto config = make_shared<CameraConfig>();
    config->url = url;
    config->frameRate = frameRate;
    config->active = true;

    cameras[id] = config;
    captures[id] = move(cap);

    captureThreads[id] = thread(&CameraService::captureFramesFromCamera, this, id, config);
    captureThreads[id].detach();

    return id;
  }

  void removeCamera(int id)
  {
    if (cameras.find(id) != cameras.end())
    {
      cameras[id]->active = false;
      captures.erase(id);
      cameras.erase(id);
    }
  }

  shared_ptr<CameraConfig> getCamera(int id)
  {
    if (cameras.find(id) != cameras.end())
    {
      return cameras[id];
    }
    return nullptr;
  }
};

void handleClient(ip::tcp::socket socket, shared_ptr<CameraConfig> camera, int cameraId)
{
  string clientAddress = socket.remote_endpoint().address().to_string();
  cout << "Client connected to camera " << cameraId << " from " << clientAddress << endl;

  try
  {
    string header = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    socket.send(buffer(header));

    vector<uchar> buf;
    vector<int> params = {IMWRITE_JPEG_QUALITY, 90};

    while (camera->active)
    {
      Mat frame;
      {
        unique_lock<mutex> lock(camera->frameMutex);
        camera->frameAvailable.wait(lock, [&]
                                    { return !camera->currentFrame.empty(); });
        camera->currentFrame.copyTo(frame);
      }

      imencode(".jpg", frame, buf, params);
      string boundary = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + to_string(buf.size()) + "\r\n\r\n";
      socket.send(buffer(boundary));
      socket.send(buffer(reinterpret_cast<const char *>(buf.data()), buf.size()));
      socket.send(buffer("\r\n"));

      this_thread::sleep_for(milliseconds(33));
    }
  }
  catch (exception &e)
  {
    cerr << "Client " << clientAddress << " disconnected from camera " << cameraId << ": " << e.what() << endl;
  }
}

void streamServer(CameraService &service)
{
  try
  {
    io_service io_service;
    ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ip::tcp::v4(), 3000));
    cout << "Stream server started on port 3000" << endl;

    while (true)
    {
      ip::tcp::socket socket(io_service);
      acceptor.accept(socket);

      // Read HTTP request
      char request[1024] = {0};
      size_t len = socket.receive(buffer(request));
      string req(request, len);

      cout << "Received request: " << req << endl;

      // Parse request path
      size_t pathStart = req.find(" ") + 1;
      size_t pathEnd = req.find(" ", pathStart);
      string path = req.substr(pathStart, pathEnd - pathStart);

      cout << "Path: " << path << endl;

      try
      {
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != string::npos)
        {
          int cameraId = stoi(path.substr(lastSlash + 1));
          cout << "Camera ID: " << cameraId << endl;

          auto camera = service.getCamera(cameraId);
          if (camera)
          {
            thread(handleClient, move(socket), camera, cameraId).detach();
          }
          else
          {
            cout << "Camera " << cameraId << " not found" << endl;
          }
        }
      }
      catch (...)
      {
        cerr << "Invalid camera ID in request" << endl;
      }
    }
  }
  catch (exception &e)
  {
    cerr << "Server error: " << e.what() << endl;
  }
}

int main()
{
  crow::SimpleApp app;
  CameraService cameraService;

  CROW_ROUTE(app, "/cameras")
      .methods("POST"_method)([&](const crow::request &req)
                              {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, "Invalid JSON");
        
        try {
            string url = json["url"].s();
            int frameRate = json["frameRate"].i();
            std::cout << "URL: " << url << " Frame Rate: " << frameRate << std::endl;
            int id = cameraService.addCamera(url, frameRate);
            return crow::response(200, "Camera added with ID: " + to_string(id));
        } catch (exception& e) {
            return crow::response(500, e.what());
        } });

  CROW_ROUTE(app, "/cameras/<int>")
      .methods("DELETE"_method)([&](int id)
                                {
        cameraService.removeCamera(id);
        return crow::response(200, "Camera removed"); });

  thread(streamServer, ref(cameraService)).detach();
  app.port(3001).run();
  return 0;
}