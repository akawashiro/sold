FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install -y ninja-build cmake gcc g++ git python3 python3-distutils python3-dev python3-pip
RUN pip3 install pytest numpy torch==1.8.0+cpu -f https://download.pytorch.org/whl/torch_stable.html
COPY . /sold
WORKDIR /sold
RUN rm -rf build
RUN mkdir build && cd build && cmake -DTorch_DIR=$(python3 -c "import site; print (site.getsitepackages()[0])")/torch/share/cmake/Torch -GNinja -DSOLD_PYBIND_TEST=ON -DSOLD_LIBTORCH_TEST=ON ..
RUN cmake --build build
RUN cd build && ctest
