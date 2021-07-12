#include <iostream>

#include <ATen/ATen.h>
#include <ATen/Functions.h>
#include <ATen/Tensor.h>
#include <ATen/core/dispatch/Dispatcher.h>
#include <c10/core/DispatchKey.h>
#include <c10/util/ArrayRef.h>

#define SLOG() std::cout << __FILE__ << ":" << __LINE__ << " "
#define SSHOW_KV(key, value) " " << key << "=" << (value)
#define SSHOW(x) SSHOW_KV(#x, x)

extern "C" void test() {
    SLOG() << "add test" << std::endl;
    auto x = at::randn({1, 128, 224, 224}, at::kFloat);
    auto y = at::randn({1, 128, 224, 224}, at::kFloat);
    auto z = at::add(x, y);
    SLOG() << SSHOW(z.toString()) << std::endl;

    SLOG() << "convolution test" << std::endl;
    auto w = at::randn({32, 128, 224, 224}, at::kFloat);
    auto r2 = at::convolution(x, w, c10::nullopt, c10::IntArrayRef({1, 1}), c10::IntArrayRef({0}), c10::IntArrayRef({1, 1}), false,
                              c10::IntArrayRef({0}), 1);
    SLOG() << SSHOW(r2.toString()) << std::endl;

    SLOG() << "matmul test" << std::endl;
    auto m1 = at::randn({128, 128}, at::kFloat);
    auto m2 = at::randn({128, 128}, at::kFloat);
    auto m3 = at::matmul(m1, m2);
    SLOG() << SSHOW(m3.toString()) << std::endl;
}
