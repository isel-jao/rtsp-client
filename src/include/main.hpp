
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include <string>
#include "crow_all.h"
#include "camera.hpp"

std::string getCodec(const cv::VideoCapture *cap);