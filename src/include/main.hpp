
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include <string>
#include "camera.hpp"
#include "client.hpp"

std::string getCodec(const cv::VideoCapture *cap);