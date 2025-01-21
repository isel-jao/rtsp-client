#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "main.hpp"

class Camera
{
private:
    const std::string rtspUrl;
    cv::Mat sharedFrame;
    std::mutex frameMutex;
    std::condition_variable frameAvailable;
    const int frameRate = 30;
    cv::VideoCapture cap;

private:
    Camera();
    Camera(const std::string &rtspUrl);
    Camera(const Camera &) = delete;
    Camera &operator=(const Camera &) = delete;

public:
    static std::map<std::string, Camera *> instances;
    static std::mutex mutex;
    static Camera *getInstance(const std::string &rtspUrl);
    int captureFrames();
    void handleClient(crow::response res);
};

#endif