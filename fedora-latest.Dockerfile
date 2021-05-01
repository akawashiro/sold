FROM fedora:34
RUN dnf install -y ninja-build cmake gcc g++ git libstdc++
COPY . /sold
WORKDIR /sold
RUN rm -rf build
RUN mkdir build && cd build && cmake -GNinja ..
RUN cmake --build build
RUN cd tests && ./run-all-tests.sh
