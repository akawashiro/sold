FROM fedora:34
RUN dnf install -y ninja-build cmake gcc g++ git libstdc++ python3 python3-devel python3-pip rsync
RUN pip3 install numpy pytest
COPY . /sold
WORKDIR /sold
RUN rm -rf build
RUN mkdir build && cd build && cmake -GNinja -DSOLD_PYBIND_TEST=ON ..
RUN cmake --build build
RUN cd build && ctest
