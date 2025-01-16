# RTSP client

# install dependencies

```bash
sudo apt update
sudo apt install -y libopencv-dev ffmpeg libboost-all-dev g++

```

# build

```bash
g++  src/main.cpp `pkg-config --cflags --libs opencv4` -lboost_system -lpthread && ./a.out
```

# run

```bash
./a.out
```
