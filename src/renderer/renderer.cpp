#include <wulfhilde/renderer/renderer.hpp>

#include "device.hpp"
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>

#include <expected>
#include <memory>
#include <utility>

namespace wulfhilde {

std::expected<Renderer, DeviceError> Renderer::create_headless(RendererSettings settings) {
    auto device = detail::Device::create(settings);
    if (!device) {
        return std::unexpected(std::move(device).error());
    }
    return Renderer(std::move(*device));
}

Renderer::Renderer(std::unique_ptr<detail::Device> device) noexcept : device_(std::move(device)) {}

Renderer::Renderer(Renderer&& other) noexcept = default;

Renderer& Renderer::operator=(Renderer&& other) noexcept = default;

Renderer::~Renderer() = default;

} // namespace wulfhilde
