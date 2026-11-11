// VulkanMemoryAllocator's implementation, which its header holds, compiled here once. It takes its
// Vulkan functions from volk, which the header must see first.
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
