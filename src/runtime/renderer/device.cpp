#include "device.h"
#include "d3d12/device_dx12.h"

std::unique_ptr<GPUDevice> GPUDevice::Create(CreateFlags flags) {
    return std::make_unique<GPUDevice>();
}