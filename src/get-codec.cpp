#include "main.hpp"

std::string getCodec(const cv::VideoCapture *cap)
{
    // Get codec information
    int fourcc = static_cast<int>(cap->get(cv::CAP_PROP_FOURCC));

    // Convert FOURCC to readable format
    char fourcc_str[5];
    for (int i = 0; i < 4; i++)
    {
        fourcc_str[i] = fourcc & 0xFF;
        fourcc >>= 8;
    }
    fourcc_str[4] = '\0';
    return fourcc_str;
}