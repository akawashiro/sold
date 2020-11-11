FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install -y ninja-build cmake gcc g++ git
COPY . /sold
WORKDIR /sold
RUN rm -rf build
RUN mkdir build && cd build && cmake -GNinja ..
RUN cmake --build build
RUN cd tests && ./run-all-tests.sh
