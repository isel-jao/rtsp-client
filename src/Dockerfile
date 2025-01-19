FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libopencv-dev \
    libboost-all-dev \
    libasio-dev \
    nodejs \
    npm \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN npm install -g nodemon

WORKDIR /app
COPY . .

EXPOSE 3000 3001

ENTRYPOINT ["bash", "-c", "while true; do g++ main.cpp `pkg-config --cflags --libs opencv4` -lboost_system -lpthread && ./a.out; sleep 2; done"]