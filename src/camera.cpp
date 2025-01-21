#include "main.hpp"

Camera::Camera() {};

Camera::Camera(const std::string &rtspUrl) : rtspUrl(rtspUrl) {};

Camera *Camera::getInstance(const std::string &rtspUrl)
{
    std::lock_guard<std::mutex> lock(Camera::mutex);
    if (Camera::instances.find(rtspUrl) == Camera::instances.end())
    {
        std::cout << "create new instance: " << rtspUrl << std::endl;
        Camera::instances[rtspUrl] = new Camera(rtspUrl);
    }
    else
    {
        std::cout << "return existing instance: " << rtspUrl << std::endl;
    }
    return Camera::instances[rtspUrl];
}
static void releaseInstance(const std::string &rtspUrl)
{
    std::lock_guard<std::mutex> lock(Camera::mutex);
    if (Camera::instances.find(rtspUrl) != Camera::instances.end())
    {
        std::cout << "releaseInstance: " << rtspUrl << std::endl;
        delete Camera::instances[rtspUrl];
        Camera::instances.erase(rtspUrl);
    }
}

int Camera::captureFrames()
{
    cap.open(rtspUrl);
    if (!cap.isOpened())
    {

        std::cout << "Error opening RTSP stream" << std::endl;
        return -1;
    }
    while (true)
    {
        cv::Mat frame;
        cap >> frame; // Capture a frame
        std::cout << "captureFrames: " << rtspUrl << std::endl;
        if (frame.empty())
        {
            std::cerr << "Error: Received empty frame. Stopping capture." << std::endl;
            break;
        }

        // Lock the mutex and update the shared frame
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            frame.copyTo(sharedFrame);
        }

        // Notify waiting threads that a new frame is available
        frameAvailable.notify_all();

        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
    }
    cap.release();
    return 0;
}

void Camera::handleClient(crow::response res)
{
    try
    {
        // // Get client info
        // string clientAddress = socket.remote_endpoint().address().to_string();
        // cout << "New client connected: " << clientAddress << endl;

        // // HTTP Header
        // string header =
        //     "HTTP/1.1 200 OK\r\n"
        //     "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
        // socket.send(buffer(header));

        // vector<uchar> buf;
        // vector<int> params = {IMWRITE_JPEG_QUALITY, 90};

        // while (true)
        // {
        //     Mat frame;

        //     // Wait for a new frame
        //     {
        //         unique_lock<mutex> lock(frameMutex);
        //         frameAvailable.wait(lock, []
        //                             { return !sharedFrame.empty(); });
        //         sharedFrame.copyTo(frame);
        //     }

        //     // Encode frame as JPEG
        //     imencode(".jpg", frame, buf, params);

        //     // Write frame boundary and data
        //     string boundary = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + to_string(buf.size()) + "\r\n\r\n";
        //     socket.send(buffer(boundary));
        //     socket.send(buffer(reinterpret_cast<const char *>(buf.data()), buf.size()));
        //     socket.send(buffer("\r\n"));

        //     this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        // }
        res.add_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");
        res.is_alive();
        std::vector<uchar> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 90};

        while (true)
        {
            cv::Mat frame;
            {
                std::unique_lock<std::mutex> lock(frameMutex);
                frameAvailable.wait(lock, [this]
                                    { return !sharedFrame.empty(); });
                sharedFrame.copyTo(frame);
            }
            imencode(".jpg", frame, buf, params);
            std::string boundary = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(buf.size()) + "\r\n\r\n";
            res.write(boundary);
            res.write(reinterpret_cast<const char *>(buf.data()));
            res.write("\r\n");
            res.end();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Client connection error: " << e.what() << std::endl;
    }

    // Log client disconnection
    std::cout << "Client disconnected." << std::endl;
}