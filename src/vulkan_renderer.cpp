#include "vulkan_renderer.h"

#include "app_identity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef STARSHARD_SHADER_BUILD_DIR
#define STARSHARD_SHADER_BUILD_DIR "shaders"
#endif

#ifndef STARSHARD_SHADER_INSTALL_DIR
#define STARSHARD_SHADER_INSTALL_DIR "shaders"
#endif

namespace {

const std::vector<const char*> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

constexpr std::uint32_t kStarRandomSeed = 0x51A2C4D7u;
constexpr float kInvulnerabilityFlashHz = 8.0f;

VkPipelineColorBlendAttachmentState opaque_blend_attachment() {
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = VK_FALSE;
    return attachment;
}

VkPipelineColorBlendAttachmentState alpha_blend_attachment() {
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = VK_TRUE;
    attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return attachment;
}

VkPipelineColorBlendAttachmentState additive_blend_attachment() {
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = VK_TRUE;
    attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return attachment;
}

float next_random(std::uint32_t& state) {
    state = 1664525u * state + 1013904223u;
    return static_cast<float>(state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

std::vector<std::filesystem::path> shader_search_directories() {
    std::vector<std::filesystem::path> directories;
    directories.reserve(3);

    auto append = [&](const char* value) {
        const std::filesystem::path candidate(value);
        if (std::find(directories.begin(), directories.end(), candidate) == directories.end()) {
            directories.push_back(candidate);
        }
    };

    append(STARSHARD_SHADER_BUILD_DIR);
    append(STARSHARD_SHADER_INSTALL_DIR);
    append("shaders");
    return directories;
}

std::filesystem::path resolve_shader_path(const char* fileName) {
    std::error_code error;
    const std::vector<std::filesystem::path> directories = shader_search_directories();
    for (const auto& directory : directories) {
        const std::filesystem::path candidate = directory / fileName;
        if (std::filesystem::exists(candidate, error)) {
            return candidate;
        }
    }

    std::string message = "Failed to locate shader file ";
    message += fileName;
    message += ". Searched:";
    for (const auto& directory : directories) {
        message += " ";
        message += directory.string();
    }
    throw std::runtime_error(message);
}

}

bool VulkanRenderer::QueueFamilyIndices::is_complete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
}

VkVertexInputBindingDescription VulkanRenderer::ParticleVertex::binding_description() {
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(ParticleVertex);
    description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return description;
}

std::array<VkVertexInputAttributeDescription, 4> VulkanRenderer::ParticleVertex::attribute_descriptions() {
    std::array<VkVertexInputAttributeDescription, 4> descriptions{};

    descriptions[0].binding = 0;
    descriptions[0].location = 0;
    descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    descriptions[0].offset = offsetof(ParticleVertex, position);

    descriptions[1].binding = 0;
    descriptions[1].location = 1;
    descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[1].offset = offsetof(ParticleVertex, color);

    descriptions[2].binding = 0;
    descriptions[2].location = 2;
    descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[2].offset = offsetof(ParticleVertex, params0);

    descriptions[3].binding = 0;
    descriptions[3].location = 3;
    descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[3].offset = offsetof(ParticleVertex, params1);

    return descriptions;
}

VkVertexInputBindingDescription VulkanRenderer::AsteroidVertex::binding_description() {
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(AsteroidVertex);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return description;
}

std::array<VkVertexInputAttributeDescription, 4> VulkanRenderer::AsteroidVertex::attribute_descriptions() {
    std::array<VkVertexInputAttributeDescription, 4> descriptions{};
    descriptions[0].binding = 0;
    descriptions[0].location = 0;
    descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    descriptions[0].offset = offsetof(AsteroidVertex, position);

    descriptions[1].binding = 0;
    descriptions[1].location = 1;
    descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    descriptions[1].offset = offsetof(AsteroidVertex, localPosition);

    descriptions[2].binding = 0;
    descriptions[2].location = 2;
    descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[2].offset = offsetof(AsteroidVertex, variation);

    descriptions[3].binding = 0;
    descriptions[3].location = 3;
    descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[3].offset = offsetof(AsteroidVertex, basis);
    return descriptions;
}

VkVertexInputBindingDescription VulkanRenderer::StarVertex::binding_description() {
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(StarVertex);
    description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return description;
}

std::array<VkVertexInputAttributeDescription, 3> VulkanRenderer::StarVertex::attribute_descriptions() {
    std::array<VkVertexInputAttributeDescription, 3> descriptions{};

    descriptions[0].binding = 0;
    descriptions[0].location = 0;
    descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    descriptions[0].offset = offsetof(StarVertex, position);

    descriptions[1].binding = 0;
    descriptions[1].location = 1;
    descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[1].offset = offsetof(StarVertex, color);

    descriptions[2].binding = 0;
    descriptions[2].location = 2;
    descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[2].offset = offsetof(StarVertex, params);

    return descriptions;
}

VkVertexInputBindingDescription VulkanRenderer::HudVertex::binding_description() {
    VkVertexInputBindingDescription description{};
    description.binding = 0;
    description.stride = sizeof(HudVertex);
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return description;
}

std::array<VkVertexInputAttributeDescription, 3> VulkanRenderer::HudVertex::attribute_descriptions() {
    std::array<VkVertexInputAttributeDescription, 3> descriptions{};

    descriptions[0].binding = 0;
    descriptions[0].location = 0;
    descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    descriptions[0].offset = offsetof(HudVertex, position);

    descriptions[1].binding = 0;
    descriptions[1].location = 1;
    descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[1].offset = offsetof(HudVertex, color);

    descriptions[2].binding = 0;
    descriptions[2].location = 2;
    descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    descriptions[2].offset = offsetof(HudVertex, params);

    return descriptions;
}

void VulkanRenderer::initialize(GLFWwindow* window) {
    window_ = window;
    create_instance();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_swapchain_image_views();
    create_render_passes();
    create_descriptor_set_layouts();
    create_samplers();
    create_offscreen_targets();
    create_pipelines();
    create_framebuffers();
    create_descriptor_pool();
    create_descriptor_sets();
    create_command_pool();
    create_star_buffer();
    create_starfield();
    create_particle_buffer();
    create_asteroid_buffer();
    create_hud_buffer();
    create_menu_buffer();
    create_command_buffers();
    create_sync_objects();
    update_descriptor_sets();
}

void VulkanRenderer::render(
    const ShipState& shipState,
    std::span<const EffectParticleRenderData> particles,
    std::span<const AsteroidRenderData> asteroids,
    const HudState& hudState,
    float deltaTimeSeconds,
    RenderMode renderMode,
    const MenuOverlayState& menuOverlay
) {
    elapsedTimeSeconds_ += deltaTimeSeconds;
    update_particle_buffer(particles);
    update_asteroid_buffer(asteroids);
    update_hud_buffer(hudState);
    update_menu_buffer(menuOverlay);

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight_[imageIndex] = inFlightFences_[currentFrame_];

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    record_command_buffer(commandBuffers_[currentFrame_], imageIndex, shipState, particles, asteroids, hudState, renderMode);

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    VkSwapchainKHR swapchains[] = {swapchain_};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreate_swapchain();
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

void VulkanRenderer::handle_resize() {
    framebufferResized_ = true;
}

void VulkanRenderer::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    cleanup_swapchain();

    if (starBufferMapped_ != nullptr) {
        vkUnmapMemory(device_, starBufferMemory_);
        starBufferMapped_ = nullptr;
    }

    if (starBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, starBuffer_, nullptr);
        starBuffer_ = VK_NULL_HANDLE;
    }

    if (starBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, starBufferMemory_, nullptr);
        starBufferMemory_ = VK_NULL_HANDLE;
    }

    if (particleBufferMapped_ != nullptr) {
        vkUnmapMemory(device_, particleBufferMemory_);
        particleBufferMapped_ = nullptr;
    }

    if (particleBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, particleBuffer_, nullptr);
        particleBuffer_ = VK_NULL_HANDLE;
    }

    if (particleBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, particleBufferMemory_, nullptr);
        particleBufferMemory_ = VK_NULL_HANDLE;
    }

    if (asteroidBufferMapped_ != nullptr) {
        vkUnmapMemory(device_, asteroidBufferMemory_);
        asteroidBufferMapped_ = nullptr;
    }

    if (asteroidBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, asteroidBuffer_, nullptr);
        asteroidBuffer_ = VK_NULL_HANDLE;
    }

    if (asteroidBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, asteroidBufferMemory_, nullptr);
        asteroidBufferMemory_ = VK_NULL_HANDLE;
    }

    if (hudBufferMapped_ != nullptr) {
        vkUnmapMemory(device_, hudBufferMemory_);
        hudBufferMapped_ = nullptr;
    }

    if (hudBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, hudBuffer_, nullptr);
        hudBuffer_ = VK_NULL_HANDLE;
    }

    if (hudBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, hudBufferMemory_, nullptr);
        hudBufferMemory_ = VK_NULL_HANDLE;
    }

    if (menuBufferMapped_ != nullptr) {
        vkUnmapMemory(device_, menuBufferMemory_);
        menuBufferMapped_ = nullptr;
    }

    if (menuBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, menuBuffer_, nullptr);
        menuBuffer_ = VK_NULL_HANDLE;
    }

    if (menuBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, menuBufferMemory_, nullptr);
        menuBufferMemory_ = VK_NULL_HANDLE;
    }

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }

    if (shipDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, shipDescriptorSetLayout_, nullptr);
        shipDescriptorSetLayout_ = VK_NULL_HANDLE;
    }

    if (blurDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, blurDescriptorSetLayout_, nullptr);
        blurDescriptorSetLayout_ = VK_NULL_HANDLE;
    }

    if (compositeDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, compositeDescriptorSetLayout_, nullptr);
        compositeDescriptorSetLayout_ = VK_NULL_HANDLE;
    }

    if (linearSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, linearSampler_, nullptr);
        linearSampler_ = VK_NULL_HANDLE;
    }

    if (device_ != VK_NULL_HANDLE) {
        for (size_t index = 0; index < imageAvailableSemaphores_.size(); ++index) {
            vkDestroySemaphore(device_, renderFinishedSemaphores_[index], nullptr);
            vkDestroySemaphore(device_, imageAvailableSemaphores_[index], nullptr);
            vkDestroyFence(device_, inFlightFences_[index], nullptr);
        }
    }
    imageAvailableSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    inFlightFences_.clear();
    imagesInFlight_.clear();

    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    commandBuffers_.clear();

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    physicalDevice_ = VK_NULL_HANDLE;
    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_ = VK_NULL_HANDLE;
    window_ = nullptr;
    currentFrame_ = 0;
    framebufferResized_ = false;
}

void VulkanRenderer::create_instance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = AppIdentity::kDisplayName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "None";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (extensions == nullptr || extensionCount == 0) {
        throw std::runtime_error("GLFW did not provide Vulkan instance extensions.");
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance.");
    }
}

void VulkanRenderer::create_surface() {
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface.");
    }
}

void VulkanRenderer::pick_physical_device() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        if (is_device_suitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable Vulkan device.");
    }
}

void VulkanRenderer::create_logical_device() {
    const QueueFamilyIndices indices = find_queue_families(physicalDevice_);
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());

    for (uint32_t familyIndex : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = familyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kRequiredDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kRequiredDeviceExtensions.data();

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }

    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

void VulkanRenderer::create_swapchain() {
    const SwapchainSupportDetails supportDetails = query_swapchain_support(physicalDevice_);
    const VkSurfaceFormatKHR surfaceFormat = choose_surface_format(supportDetails.formats);
    const VkPresentModeKHR presentMode = choose_present_mode(supportDetails.presentModes);
    const VkExtent2D extent = choose_swap_extent(supportDetails.capabilities);

    uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
    if (supportDetails.capabilities.maxImageCount > 0 && imageCount > supportDetails.capabilities.maxImageCount) {
        imageCount = supportDetails.capabilities.maxImageCount;
    }

    const QueueFamilyIndices indices = find_queue_families(physicalDevice_);
    const uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = supportDetails.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain.");
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
    imagesInFlight_.assign(swapchainImages_.size(), VK_NULL_HANDLE);
}

void VulkanRenderer::create_swapchain_image_views() {
    swapchainImageViews_.resize(swapchainImages_.size());

    for (size_t index = 0; index < swapchainImages_.size(); ++index) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages_[index];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat_;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[index]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain image view.");
        }
    }
}

void VulkanRenderer::create_render_passes() {
    auto make_single_attachment = [&](VkImageLayout finalLayout) {
        VkAttachmentDescription attachment{};
        attachment.format = swapchainImageFormat_;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = finalLayout;
        return attachment;
    };

    {
        const VkAttachmentDescription attachment = make_single_attachment(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkAttachmentReference attachmentRef{};
        attachmentRef.attachment = 0;
        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &attachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &lightRenderPass_) != VK_SUCCESS ||
            vkCreateRenderPass(device_, &renderPassInfo, nullptr, &postProcessRenderPass_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create single-attachment render pass.");
        }
    }

    {
        std::array<VkAttachmentDescription, 2> attachments = {
            make_single_attachment(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
            make_single_attachment(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        };
        std::array<VkAttachmentReference, 2> attachmentRefs{};
        attachmentRefs[0].attachment = 0;
        attachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentRefs[1].attachment = 1;
        attachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(attachmentRefs.size());
        subpass.pColorAttachments = attachmentRefs.data();

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &sceneRenderPass_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create scene render pass.");
        }
    }

    {
        const VkAttachmentDescription attachment = make_single_attachment(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        VkAttachmentReference attachmentRef{};
        attachmentRef.attachment = 0;
        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &attachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &attachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &compositeRenderPass_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create composite render pass.");
        }
    }
}

void VulkanRenderer::create_descriptor_set_layouts() {
    VkDescriptorSetLayoutBinding shipBinding{};
    shipBinding.binding = 0;
    shipBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shipBinding.descriptorCount = 1;
    shipBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo shipLayoutInfo{};
    shipLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    shipLayoutInfo.bindingCount = 1;
    shipLayoutInfo.pBindings = &shipBinding;

    VkDescriptorSetLayoutBinding blurBinding = shipBinding;
    VkDescriptorSetLayoutCreateInfo blurLayoutInfo = shipLayoutInfo;
    blurLayoutInfo.pBindings = &blurBinding;

    std::array<VkDescriptorSetLayoutBinding, 2> compositeBindings{};
    compositeBindings[0].binding = 0;
    compositeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    compositeBindings[0].descriptorCount = 1;
    compositeBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    compositeBindings[1] = compositeBindings[0];
    compositeBindings[1].binding = 1;

    VkDescriptorSetLayoutCreateInfo compositeLayoutInfo{};
    compositeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    compositeLayoutInfo.bindingCount = static_cast<uint32_t>(compositeBindings.size());
    compositeLayoutInfo.pBindings = compositeBindings.data();

    if (vkCreateDescriptorSetLayout(device_, &shipLayoutInfo, nullptr, &shipDescriptorSetLayout_) != VK_SUCCESS ||
        vkCreateDescriptorSetLayout(device_, &blurLayoutInfo, nullptr, &blurDescriptorSetLayout_) != VK_SUCCESS ||
        vkCreateDescriptorSetLayout(device_, &compositeLayoutInfo, nullptr, &compositeDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layouts.");
    }
}

void VulkanRenderer::create_samplers() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device_, &samplerInfo, nullptr, &linearSampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler.");
    }
}

void VulkanRenderer::create_offscreen_target(OffscreenTarget& target) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapchainExtent_.width;
    imageInfo.extent.height = swapchainExtent_.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = swapchainImageFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &imageInfo, nullptr, &target.image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen image.");
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device_, target.image, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &target.memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate offscreen image memory.");
    }

    vkBindImageMemory(device_, target.image, target.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = target.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swapchainImageFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_, &viewInfo, nullptr, &target.view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen image view.");
    }
}

void VulkanRenderer::destroy_offscreen_target(OffscreenTarget& target) {
    if (target.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, target.framebuffer, nullptr);
        target.framebuffer = VK_NULL_HANDLE;
    }
    if (target.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, target.view, nullptr);
        target.view = VK_NULL_HANDLE;
    }
    if (target.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, target.image, nullptr);
        target.image = VK_NULL_HANDLE;
    }
    if (target.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, target.memory, nullptr);
        target.memory = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::create_offscreen_targets() {
    create_offscreen_target(lightTarget_);
    create_offscreen_target(asteroidLightTarget_);
    create_offscreen_target(sceneTarget_);
    create_offscreen_target(brightTarget_);
    create_offscreen_target(blurTargets_[0]);
    create_offscreen_target(blurTargets_[1]);
}

void VulkanRenderer::create_pipelines() {
    const auto read_shader = [&](const char* fileName) {
        return read_binary_file(resolve_shader_path(fileName).string());
    };

    const std::vector<char> starVertShaderCode = read_shader("star.vert.spv");
    const std::vector<char> starFragShaderCode = read_shader("star.frag.spv");
    const std::vector<char> asteroidVertShaderCode = read_shader("asteroid.vert.spv");
    const std::vector<char> asteroidFragShaderCode = read_shader("asteroid.frag.spv");
    const std::vector<char> shipVertShaderCode = read_shader("ship.vert.spv");
    const std::vector<char> shipFragShaderCode = read_shader("ship.frag.spv");
    const std::vector<char> particleVertShaderCode = read_shader("particle.vert.spv");
    const std::vector<char> particleSceneFragShaderCode = read_shader("particle.frag.spv");
    const std::vector<char> particleLightVertShaderCode = read_shader("particle_light.vert.spv");
    const std::vector<char> particleLightFragShaderCode = read_shader("particle_light.frag.spv");
    const std::vector<char> fullscreenVertShaderCode = read_shader("fullscreen.vert.spv");
    const std::vector<char> blurFragShaderCode = read_shader("blur.frag.spv");
    const std::vector<char> compositeFragShaderCode = read_shader("composite.frag.spv");
    const std::vector<char> hudVertShaderCode = read_shader("hud.vert.spv");
    const std::vector<char> hudFragShaderCode = read_shader("hud.frag.spv");
    const std::vector<char> menuHudFragShaderCode = read_shader("menu_hud.frag.spv");

    const VkShaderModule starVertModule = create_shader_module(starVertShaderCode);
    const VkShaderModule starFragModule = create_shader_module(starFragShaderCode);
    const VkShaderModule asteroidVertModule = create_shader_module(asteroidVertShaderCode);
    const VkShaderModule asteroidFragModule = create_shader_module(asteroidFragShaderCode);
    const VkShaderModule shipVertModule = create_shader_module(shipVertShaderCode);
    const VkShaderModule shipFragModule = create_shader_module(shipFragShaderCode);
    const VkShaderModule particleVertModule = create_shader_module(particleVertShaderCode);
    const VkShaderModule particleSceneFragModule = create_shader_module(particleSceneFragShaderCode);
    const VkShaderModule particleLightVertModule = create_shader_module(particleLightVertShaderCode);
    const VkShaderModule particleLightFragModule = create_shader_module(particleLightFragShaderCode);
    const VkShaderModule fullscreenVertModule = create_shader_module(fullscreenVertShaderCode);
    const VkShaderModule blurFragModule = create_shader_module(blurFragShaderCode);
    const VkShaderModule compositeFragModule = create_shader_module(compositeFragShaderCode);
    const VkShaderModule hudVertModule = create_shader_module(hudVertShaderCode);
    const VkShaderModule hudFragModule = create_shader_module(hudFragShaderCode);
    const VkShaderModule menuHudFragModule = create_shader_module(menuHudFragShaderCode);

    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    {
        const VkVertexInputBindingDescription bindingDescription = StarVertex::binding_description();
        const auto attributeDescriptions = StarVertex::attribute_descriptions();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, starVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, starFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        const std::array<VkPipelineColorBlendAttachmentState, 2> attachments = {
            alpha_blend_attachment(),
            opaque_blend_attachment(),
        };
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = static_cast<uint32_t>(attachments.size());
        colorBlending.pAttachments = attachments.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.size = sizeof(StarPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &starPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create star pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = starPipelineLayout_;
        pipelineInfo.renderPass = sceneRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &starPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create star graphics pipeline.");
        }
    }

    {
        const VkVertexInputBindingDescription bindingDescription = AsteroidVertex::binding_description();
        const auto attributeDescriptions = AsteroidVertex::attribute_descriptions();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, asteroidVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, asteroidFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const std::array<VkPipelineColorBlendAttachmentState, 2> attachments = {
            opaque_blend_attachment(),
            opaque_blend_attachment(),
        };
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = static_cast<uint32_t>(attachments.size());
        colorBlending.pAttachments = attachments.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.size = sizeof(AsteroidPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &shipDescriptorSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &asteroidPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create asteroid pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = asteroidPipelineLayout_;
        pipelineInfo.renderPass = sceneRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &asteroidPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create asteroid graphics pipeline.");
        }
    }

    {
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shipVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, shipFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const std::array<VkPipelineColorBlendAttachmentState, 2> attachments = {
            opaque_blend_attachment(),
            additive_blend_attachment(),
        };
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = static_cast<uint32_t>(attachments.size());
        colorBlending.pAttachments = attachments.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.size = sizeof(ShipPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &shipDescriptorSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &shipPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ship pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = shipPipelineLayout_;
        pipelineInfo.renderPass = sceneRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &shipPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ship graphics pipeline.");
        }
    }

    {
        const VkVertexInputBindingDescription bindingDescription = ParticleVertex::binding_description();
        const auto attributeDescriptions = ParticleVertex::attribute_descriptions();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, particleVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, particleSceneFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        const std::array<VkPipelineColorBlendAttachmentState, 2> attachments = {
            alpha_blend_attachment(),
            additive_blend_attachment(),
        };
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = static_cast<uint32_t>(attachments.size());
        colorBlending.pAttachments = attachments.data();

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &particleScenePipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create particle scene pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = particleScenePipelineLayout_;
        pipelineInfo.renderPass = sceneRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &particleScenePipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create particle scene graphics pipeline.");
        }
    }

    {
        const VkVertexInputBindingDescription bindingDescription = ParticleVertex::binding_description();
        const auto attributeDescriptions = ParticleVertex::attribute_descriptions();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, particleLightVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, particleLightFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        const VkPipelineColorBlendAttachmentState attachment = additive_blend_attachment();
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &attachment;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &particleLightPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create particle light pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = particleLightPipelineLayout_;
        pipelineInfo.renderPass = lightRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &particleLightPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create particle light graphics pipeline.");
        }
    }

    {
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, fullscreenVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, blurFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const VkPipelineColorBlendAttachmentState attachment = opaque_blend_attachment();
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &attachment;

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.size = sizeof(BlurPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &blurDescriptorSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &blurPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blur pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = blurPipelineLayout_;
        pipelineInfo.renderPass = postProcessRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &blurPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blur graphics pipeline.");
        }
    }

    {
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, fullscreenVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, compositeFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const VkPipelineColorBlendAttachmentState attachment = opaque_blend_attachment();
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &attachment;

        VkPushConstantRange compositePushConstantRange{};
        compositePushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        compositePushConstantRange.offset = 0;
        compositePushConstantRange.size = sizeof(CompositePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &compositeDescriptorSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &compositePushConstantRange;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &compositePipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create composite pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = compositePipelineLayout_;
        pipelineInfo.renderPass = compositeRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &compositePipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create composite graphics pipeline.");
        }
    }

    {
        const VkVertexInputBindingDescription bindingDescription = HudVertex::binding_description();
        const auto attributeDescriptions = HudVertex::attribute_descriptions();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, hudVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, hudFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const std::array<VkPipelineColorBlendAttachmentState, 2> attachments = {
            alpha_blend_attachment(),
            additive_blend_attachment(),
        };
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = static_cast<uint32_t>(attachments.size());
        colorBlending.pAttachments = attachments.data();

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &hudPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create HUD pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = hudPipelineLayout_;
        pipelineInfo.renderPass = sceneRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &hudPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create HUD graphics pipeline.");
        }
    }

    {
        const VkVertexInputBindingDescription bindingDescription = HudVertex::binding_description();
        const auto attributeDescriptions = HudVertex::attribute_descriptions();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, hudVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, menuHudFragModule, "main", nullptr},
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        const VkPipelineColorBlendAttachmentState attachment = alpha_blend_attachment();
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &attachment;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &menuHudPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create menu HUD pipeline layout.");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = menuHudPipelineLayout_;
        pipelineInfo.renderPass = compositeRenderPass_;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &menuHudPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create menu HUD graphics pipeline.");
        }
    }

    vkDestroyShaderModule(device_, menuHudFragModule, nullptr);
    vkDestroyShaderModule(device_, hudFragModule, nullptr);
    vkDestroyShaderModule(device_, hudVertModule, nullptr);
    vkDestroyShaderModule(device_, starFragModule, nullptr);
    vkDestroyShaderModule(device_, starVertModule, nullptr);
    vkDestroyShaderModule(device_, asteroidFragModule, nullptr);
    vkDestroyShaderModule(device_, asteroidVertModule, nullptr);
    vkDestroyShaderModule(device_, compositeFragModule, nullptr);
    vkDestroyShaderModule(device_, blurFragModule, nullptr);
    vkDestroyShaderModule(device_, fullscreenVertModule, nullptr);
    vkDestroyShaderModule(device_, particleLightFragModule, nullptr);
    vkDestroyShaderModule(device_, particleLightVertModule, nullptr);
    vkDestroyShaderModule(device_, particleSceneFragModule, nullptr);
    vkDestroyShaderModule(device_, particleVertModule, nullptr);
    vkDestroyShaderModule(device_, shipFragModule, nullptr);
    vkDestroyShaderModule(device_, shipVertModule, nullptr);
}

void VulkanRenderer::create_framebuffers() {
    {
        VkImageView attachments[] = {lightTarget_.view};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = lightRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &lightTarget_.framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create light framebuffer.");
        }
    }

    {
        VkImageView attachments[] = {asteroidLightTarget_.view};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = lightRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &asteroidLightTarget_.framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create asteroid light framebuffer.");
        }
    }

    {
        VkImageView attachments[] = {sceneTarget_.view, brightTarget_.view};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = sceneRenderPass_;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &sceneTarget_.framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create scene framebuffer.");
        }
    }

    for (size_t index = 0; index < blurTargets_.size(); ++index) {
        VkImageView attachments[] = {blurTargets_[index].view};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = postProcessRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &blurTargets_[index].framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blur framebuffer.");
        }
    }

    swapchainFramebuffers_.resize(swapchainImageViews_.size());
    for (size_t index = 0; index < swapchainImageViews_.size(); ++index) {
        VkImageView attachments[] = {swapchainImageViews_[index]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = compositeRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &swapchainFramebuffers_[index]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain framebuffer.");
        }
    }
}

void VulkanRenderer::create_descriptor_pool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 8;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 7;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

void VulkanRenderer::create_descriptor_sets() {
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &shipDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &shipDescriptorSet_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate ship descriptor set.");
        }
    }

    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &shipDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &asteroidDescriptorSet_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate asteroid descriptor set.");
        }
    }

    {
        std::array<VkDescriptorSetLayout, 3> layouts = {
            blurDescriptorSetLayout_,
            blurDescriptorSetLayout_,
            blurDescriptorSetLayout_,
        };
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device_, &allocInfo, blurDescriptorSets_.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate blur descriptor sets.");
        }
    }

    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &blurDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &sceneBlurInputDescriptorSet_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate scene blur input descriptor set.");
        }
    }

    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &compositeDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &compositeDescriptorSet_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate composite descriptor set.");
        }
    }
}

void VulkanRenderer::update_descriptor_sets() {
    auto make_image_info = [&](VkImageView view) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = view;
        imageInfo.sampler = linearSampler_;
        return imageInfo;
    };

    VkDescriptorImageInfo shipImageInfo = make_image_info(lightTarget_.view);
    VkWriteDescriptorSet shipWrite{};
    shipWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shipWrite.dstSet = shipDescriptorSet_;
    shipWrite.dstBinding = 0;
    shipWrite.descriptorCount = 1;
    shipWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shipWrite.pImageInfo = &shipImageInfo;
    vkUpdateDescriptorSets(device_, 1, &shipWrite, 0, nullptr);

    VkDescriptorImageInfo asteroidImageInfo = make_image_info(asteroidLightTarget_.view);
    VkWriteDescriptorSet asteroidWrite{};
    asteroidWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    asteroidWrite.dstSet = asteroidDescriptorSet_;
    asteroidWrite.dstBinding = 0;
    asteroidWrite.descriptorCount = 1;
    asteroidWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    asteroidWrite.pImageInfo = &asteroidImageInfo;
    vkUpdateDescriptorSets(device_, 1, &asteroidWrite, 0, nullptr);

    VkDescriptorImageInfo sceneBlurImageInfo = make_image_info(sceneTarget_.view);
    VkWriteDescriptorSet sceneBlurWrite{};
    sceneBlurWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sceneBlurWrite.dstSet = sceneBlurInputDescriptorSet_;
    sceneBlurWrite.dstBinding = 0;
    sceneBlurWrite.descriptorCount = 1;
    sceneBlurWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sceneBlurWrite.pImageInfo = &sceneBlurImageInfo;
    vkUpdateDescriptorSets(device_, 1, &sceneBlurWrite, 0, nullptr);

    const std::array<VkImageView, 3> blurViews = {brightTarget_.view, blurTargets_[0].view, blurTargets_[1].view};
    for (size_t index = 0; index < blurDescriptorSets_.size(); ++index) {
        VkDescriptorImageInfo blurImageInfo = make_image_info(blurViews[index]);
        VkWriteDescriptorSet blurWrite{};
        blurWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blurWrite.dstSet = blurDescriptorSets_[index];
        blurWrite.dstBinding = 0;
        blurWrite.descriptorCount = 1;
        blurWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blurWrite.pImageInfo = &blurImageInfo;
        vkUpdateDescriptorSets(device_, 1, &blurWrite, 0, nullptr);
    }

    std::array<VkDescriptorImageInfo, 2> compositeImages = {
        make_image_info(sceneTarget_.view),
        make_image_info(blurTargets_[1].view),
    };
    std::array<VkWriteDescriptorSet, 2> compositeWrites{};
    for (uint32_t index = 0; index < compositeWrites.size(); ++index) {
        compositeWrites[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        compositeWrites[index].dstSet = compositeDescriptorSet_;
        compositeWrites[index].dstBinding = index;
        compositeWrites[index].descriptorCount = 1;
        compositeWrites[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        compositeWrites[index].pImageInfo = &compositeImages[index];
    }
    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(compositeWrites.size()), compositeWrites.data(), 0, nullptr);
}

void VulkanRenderer::create_command_pool() {
    const QueueFamilyIndices queueFamilyIndices = find_queue_families(physicalDevice_);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool.");
    }
}

void VulkanRenderer::create_starfield() {
    const FixedAspectViewportLayout viewportLayout =
        compute_fixed_aspect_viewport_layout(
            static_cast<int>(swapchainExtent_.width),
            static_cast<int>(swapchainExtent_.height)
        );
    const Vec2 backgroundHalfExtents = compute_background_half_extents(
        viewportLayout,
        GameState::kWorldHalfWidth,
        GameState::kWorldHalfHeight
    );
    const float defaultArea =
        (GameState::kWorldHalfWidth * 2.0f) *
        (GameState::kWorldHalfHeight * 2.0f);
    const float backgroundArea =
        (backgroundHalfExtents.x * 2.0f) *
        (backgroundHalfExtents.y * 2.0f);
    const std::size_t desiredStarCount = static_cast<std::size_t>(std::lround(
        240.0f * (backgroundArea / defaultArea)
    ));

    std::vector<StarVertex> upload;
    upload.reserve(maxStarCount_);

    std::uint32_t rng = kStarRandomSeed;
    const std::size_t starCountTarget = std::clamp<std::size_t>(desiredStarCount, 1, maxStarCount_);
    for (std::size_t index = 0; index < starCountTarget; ++index) {
        const float x =
            -backgroundHalfExtents.x + next_random(rng) * backgroundHalfExtents.x * 2.0f;
        const float y =
            -backgroundHalfExtents.y + next_random(rng) * backgroundHalfExtents.y * 2.0f;
        const float size = 0.12f + next_random(rng) * 0.42f;
        const float baseBrightness = 0.35f + next_random(rng) * 0.5f;
        const float twinkleAmplitude = 0.04f + next_random(rng) * 0.14f;
        const float twinkleSpeed = 0.5f + next_random(rng) * 1.7f;
        const float twinklePhase = next_random(rng) * 2.0f * std::numbers::pi_v<float>;
        const float spectralRoll = next_random(rng);
        const float tintRoll = next_random(rng);

        const ColorRgb neutral = {1.0f, 1.0f, 1.0f};
        const ColorRgb cool = {0.78f, 0.86f, 1.0f};
        const ColorRgb warm = {1.0f, 0.9f, 0.8f};

        ColorRgb targetColor = neutral;
        if (spectralRoll < 0.16f) {
            targetColor = warm;
        } else if (spectralRoll > 0.84f) {
            targetColor = cool;
        }

        const float tintStrength =
            (0.12f + tintRoll * 0.24f) *
            std::clamp(0.45f + baseBrightness * 0.5f, 0.0f, 1.0f);
        const ColorRgb color = {
            neutral.r + (targetColor.r - neutral.r) * tintStrength,
            neutral.g + (targetColor.g - neutral.g) * tintStrength,
            neutral.b + (targetColor.b - neutral.b) * tintStrength,
        };

        upload.push_back({
            .position = {x, y},
            .color = {color.r, color.g, color.b, twinklePhase},
            .params = {size, baseBrightness, twinkleAmplitude, twinkleSpeed},
        });
    }

    starCount_ = upload.size();
    if (!upload.empty()) {
        std::memcpy(starBufferMapped_, upload.data(), upload.size() * sizeof(StarVertex));
    }
}

void VulkanRenderer::create_star_buffer() {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(maxStarCount_ * sizeof(StarVertex));
    create_buffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        starBuffer_,
        starBufferMemory_
    );

    if (vkMapMemory(device_, starBufferMemory_, 0, bufferSize, 0, &starBufferMapped_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map star vertex buffer.");
    }
}

void VulkanRenderer::create_particle_buffer() {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(maxParticleCount_ * sizeof(ParticleVertex));
    create_buffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        particleBuffer_,
        particleBufferMemory_
    );

    if (vkMapMemory(device_, particleBufferMemory_, 0, bufferSize, 0, &particleBufferMapped_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map particle vertex buffer.");
    }
}

void VulkanRenderer::create_asteroid_buffer() {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(maxAsteroidVertexCount_ * sizeof(AsteroidVertex));
    create_buffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        asteroidBuffer_,
        asteroidBufferMemory_
    );

    if (vkMapMemory(device_, asteroidBufferMemory_, 0, bufferSize, 0, &asteroidBufferMapped_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map asteroid vertex buffer.");
    }
}

void VulkanRenderer::create_hud_buffer() {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(kMaxHudVertices * sizeof(HudVertex));
    create_buffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        hudBuffer_,
        hudBufferMemory_
    );

    if (vkMapMemory(device_, hudBufferMemory_, 0, bufferSize, 0, &hudBufferMapped_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map HUD vertex buffer.");
    }
}

void VulkanRenderer::update_hud_buffer(const HudState& hudState) {
    constexpr bool segs[10][7] = {
        {true,  true,  true,  false, true,  true,  true},
        {false, false, true,  false, false, true,  false},
        {true,  false, true,  true,  true,  false, true},
        {true,  false, true,  true,  false, true,  true},
        {false, true,  true,  true,  false, true,  false},
        {true,  true,  false, true,  false, true,  true},
        {true,  true,  false, true,  true,  true,  true},
        {true,  false, true,  false, false, true,  false},
        {true,  true,  true,  true,  true,  true,  true},
        {true,  true,  true,  true,  false, true,  true},
    };

    struct SegmentRect {
        float x0, y0, x1, y1;
    };

    constexpr float sw = 0.15f;
    constexpr float dw = 1.0f;
    constexpr float dh = 1.8f;
    constexpr float hh = dh * 0.5f;
    constexpr float kHudEdgePaddingX = 0.08f;
    constexpr float kHudEdgePaddingY = 0.08f;
    constexpr float kHudLivesShipScale = 0.0056f;
    constexpr float kHudLivesShipSpacing = 0.046f;
    constexpr std::array<Vec2, 6> kShipSilhouettePoints = {{
        {4.0f, 0.0f},
        {-4.0f, 4.0f},
        {-2.0f, 0.0f},
        {4.0f, 0.0f},
        {-2.0f, 0.0f},
        {-4.0f, -4.0f},
    }};

    constexpr SegmentRect segmentRects[7] = {
        {sw, dh - sw, dw - sw, dh},
        {0.0f, hh, sw, dh - sw},
        {dw - sw, hh, dw, dh - sw},
        {sw, hh - sw * 0.5f, dw - sw, hh + sw * 0.5f},
        {0.0f, sw, sw, hh},
        {dw - sw, sw, dw, hh},
        {sw, 0.0f, dw - sw, sw},
    };

    std::vector<HudVertex> vertices;
    vertices.reserve(kMaxHudVertices);

    auto mix_color = [](const ColorRgb& from, const ColorRgb& to, float t) {
        return ColorRgb{
            from.r + (to.r - from.r) * t,
            from.g + (to.g - from.g) * t,
            from.b + (to.b - from.b) * t,
        };
    };

    auto make_vertex = [&](const Vec2& position, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        HudVertex vertex{};
        vertex.position[0] = position.x;
        vertex.position[1] = position.y;
        vertex.color[0] = color.r;
        vertex.color[1] = color.g;
        vertex.color[2] = color.b;
        vertex.color[3] = alpha;
        vertex.params[0] = emissiveStrength;
        vertex.params[1] = sceneAlphaScale;
        return vertex;
    };

    auto add_triangle = [&](const Vec2& a, const Vec2& b, const Vec2& c, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        HudVertex v0 = make_vertex(a, color, alpha, emissiveStrength, sceneAlphaScale);
        HudVertex v1 = make_vertex(b, color, alpha, emissiveStrength, sceneAlphaScale);
        HudVertex v2 = make_vertex(c, color, alpha, emissiveStrength, sceneAlphaScale);
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
    };

    auto add_rect = [&](float x0, float y0, float x1, float y1, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        const HudVertex v0 = make_vertex({x0, y0}, color, alpha, emissiveStrength, sceneAlphaScale);
        const HudVertex v1 = make_vertex({x1, y0}, color, alpha, emissiveStrength, sceneAlphaScale);
        const HudVertex v2 = make_vertex({x1, y1}, color, alpha, emissiveStrength, sceneAlphaScale);
        const HudVertex v3 = make_vertex({x0, y1}, color, alpha, emissiveStrength, sceneAlphaScale);
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v0);
        vertices.push_back(v2);
        vertices.push_back(v3);
    };

    auto add_ship = [&](float centerX, float centerY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        for (std::size_t vertexIndex = 0; vertexIndex < kShipSilhouettePoints.size(); vertexIndex += 3) {
            const Vec2 a = {
                centerX + kShipSilhouettePoints[vertexIndex + 0].x * scale,
                centerY + kShipSilhouettePoints[vertexIndex + 0].y * scale,
            };
            const Vec2 b = {
                centerX + kShipSilhouettePoints[vertexIndex + 1].x * scale,
                centerY + kShipSilhouettePoints[vertexIndex + 1].y * scale,
            };
            const Vec2 c = {
                centerX + kShipSilhouettePoints[vertexIndex + 2].x * scale,
                centerY + kShipSilhouettePoints[vertexIndex + 2].y * scale,
            };
            add_triangle(a, b, c, color, alpha, emissiveStrength, sceneAlphaScale);
        }
    };

    auto add_digit = [&](std::uint32_t digit, float originX, float originY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        if (digit > 9) {
            digit = 0;
        }

        for (int s = 0; s < 7; ++s) {
            if (segs[digit][s]) {
                const SegmentRect& rect = segmentRects[s];
                add_rect(
                    originX + rect.x0 * scale,
                    originY + rect.y0 * scale,
                    originX + rect.x1 * scale,
                    originY + rect.y1 * scale,
                    color,
                    alpha,
                    emissiveStrength,
                    sceneAlphaScale
                );
            }
        }
    };

    auto add_number = [&](std::uint32_t number, float rightX, float topY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        const float digitWidth = dw * scale;
        const float spacing = 0.3f * scale;

        std::array<std::uint32_t, 10> digits{};
        int digitCount = 0;
        if (number == 0) {
            digits[0] = 0;
            digitCount = 1;
        } else {
            std::uint32_t remaining = number;
            while (remaining > 0 && digitCount < 10) {
                digits[digitCount++] = remaining % 10;
                remaining /= 10;
            }
        }

        float x = rightX - digitWidth;
        const float bottomY = topY - dh * scale;
        for (int i = 0; i < digitCount; ++i) {
            add_digit(digits[i], x, bottomY, scale, color, alpha, emissiveStrength, sceneAlphaScale);
            x -= digitWidth + spacing;
        }
    };

    auto add_plus_glyph = [&](float originX, float originY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) -> float {
        const float plusWidth = 0.72f * scale;
        const float strokeWidth = sw * scale * 0.88f;
        const float centerX = originX + plusWidth * 0.5f;
        const float centerY = originY + hh * scale;
        add_rect(
            centerX - strokeWidth * 0.5f,
            originY + dh * scale * 0.2f,
            centerX + strokeWidth * 0.5f,
            originY + dh * scale * 0.8f,
            color,
            alpha,
            emissiveStrength,
            sceneAlphaScale
        );
        add_rect(
            originX,
            centerY - strokeWidth * 0.5f,
            originX + plusWidth,
            centerY + strokeWidth * 0.5f,
            color,
            alpha,
            emissiveStrength,
            sceneAlphaScale
        );
        return plusWidth;
    };

    auto count_digits = [](std::uint32_t number) {
        int digitCount = 0;
        if (number == 0) {
            return 1;
        }
        while (number > 0 && digitCount < 10) {
            number /= 10;
            ++digitCount;
        }
        return digitCount;
    };

    auto measure_number_width = [&](std::uint32_t number, float scale) {
        const float digitWidth = dw * scale;
        const float spacing = 0.3f * scale;
        const int digitCount = count_digits(number);
        return digitWidth * static_cast<float>(digitCount) + spacing * static_cast<float>(std::max(0, digitCount - 1));
    };

    auto add_number_left_aligned = [&](std::uint32_t number, float leftX, float topY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        const float digitWidth = dw * scale;
        const float spacing = 0.3f * scale;

        std::array<std::uint32_t, 10> digits{};
        int digitCount = 0;
        if (number == 0) {
            digits[0] = 0;
            digitCount = 1;
        } else {
            std::uint32_t remaining = number;
            while (remaining > 0 && digitCount < 10) {
                digits[digitCount++] = remaining % 10;
                remaining /= 10;
            }
        }

        float x = leftX;
        const float bottomY = topY - dh * scale;
        for (int index = digitCount - 1; index >= 0; --index) {
            add_digit(digits[index], x, bottomY, scale, color, alpha, emissiveStrength, sceneAlphaScale);
            x += digitWidth + spacing;
        }
    };

    auto add_number_centered = [&](std::uint32_t number, float centerX, float topY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        add_number_left_aligned(
            number,
            centerX - measure_number_width(number, scale) * 0.5f,
            topY,
            scale,
            color,
            alpha,
            emissiveStrength,
            sceneAlphaScale
        );
    };

    auto add_score_delta_popup = [&](std::uint32_t deltaValue, float rightX, float topY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        const float plusWidth = 0.72f * scale;
        const float plusGap = 0.18f * scale;
        const float popupWidth = plusWidth + plusGap + measure_number_width(deltaValue, scale);
        const float popupLeftX = rightX - popupWidth;
        add_plus_glyph(
            popupLeftX,
            topY - dh * scale,
            scale,
            color,
            alpha,
            emissiveStrength,
            sceneAlphaScale
        );
        add_number_left_aligned(
            deltaValue,
            popupLeftX + plusWidth + plusGap,
            topY,
            scale,
            color,
            alpha,
            emissiveStrength,
            sceneAlphaScale
        );
    };

    auto add_level_label = [&](float centerX, float centerY, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        constexpr float stroke = 0.18f;
        constexpr char text[] = "LEVEL";
        const float spacing = 0.34f * scale;
        const float totalWidth = 5.0f * scale + 4.0f * spacing;
        float x = centerX - totalWidth * 0.5f;
        const float y = centerY - 0.9f * scale;

        auto lr = [&](float x0, float y0, float x1, float y1) {
            add_rect(
                x + x0 * scale,
                y + y0 * scale,
                x + x1 * scale,
                y + y1 * scale,
                color,
                alpha,
                emissiveStrength,
                sceneAlphaScale
            );
        };

        for (const char* p = text; *p != '\0'; ++p) {
            const char ch = *p;
            switch (ch) {
            case 'L':
                lr(0.0f, 0.0f, stroke, 1.8f);
                lr(stroke, 0.0f, 1.0f, stroke);
                break;
            case 'E':
                lr(0.0f, 0.0f, stroke, 1.8f);
                lr(stroke, 1.8f - stroke, 1.0f, 1.8f);
                lr(stroke, 0.9f - stroke * 0.5f, 0.8f, 0.9f + stroke * 0.5f);
                lr(stroke, 0.0f, 1.0f, stroke);
                break;
            case 'V':
                lr(0.0f, 0.9f, stroke, 1.8f);
                lr(1.0f - stroke, 0.9f, 1.0f, 1.8f);
                lr(stroke, 0.3f, stroke * 2.0f, 0.9f + stroke);
                lr(1.0f - stroke * 2.0f, 0.3f, 1.0f - stroke, 0.9f + stroke);
                lr(stroke * 2.0f, 0.0f, 1.0f - stroke * 2.0f, stroke);
                break;
            default:
                break;
            }
            x += scale + spacing;
        }
    };

    auto add_game_over_menu_text = [&](const char* text, float centerX, float centerY, float scale, const ColorRgb& textColor, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        constexpr float stroke = 0.18f;
        const float spacing = kGameOverMenuButtonLetterSpacing;
        const float textWidth = measure_game_over_menu_text(text, scale, spacing);
        float x = centerX - textWidth * 0.5f;
        const float y = centerY - (1.8f * scale) * 0.5f;
        auto lr = [&](float xStart, float yStart, float xEnd, float yEnd) {
            add_rect(
                x + xStart * scale,
                y + yStart * scale,
                x + xEnd * scale,
                y + yEnd * scale,
                textColor,
                alpha,
                emissiveStrength,
                sceneAlphaScale
            );
        };

        for (const char* p = text; *p != '\0'; ++p) {
            float letterWidth = game_over_menu_letter_width(*p);
            switch (*p) {
            case 'R':
                lr(0.0f, 0.0f, stroke, 1.8f);
                lr(stroke, 1.8f - stroke, 1.0f - stroke, 1.8f);
                lr(1.0f - stroke, 0.9f, 1.0f, 1.8f - stroke);
                lr(stroke, 0.9f - stroke * 0.5f, 1.0f - stroke, 0.9f + stroke * 0.5f);
                lr(0.5f, 0.0f, 0.5f + stroke, 0.9f - stroke * 0.5f);
                lr(0.5f + stroke, 0.0f, 1.0f, stroke);
                break;
            case 'E':
                lr(0.0f, 0.0f, stroke, 1.8f);
                lr(stroke, 1.8f - stroke, 1.0f, 1.8f);
                lr(stroke, 0.9f - stroke * 0.5f, 0.8f, 0.9f + stroke * 0.5f);
                lr(stroke, 0.0f, 1.0f, stroke);
                break;
            case 'S':
                lr(stroke, 1.8f - stroke, 1.0f, 1.8f);
                lr(0.0f, 0.9f + stroke * 0.5f, stroke, 1.8f - stroke);
                lr(stroke, 0.9f - stroke * 0.5f, 1.0f - stroke, 0.9f + stroke * 0.5f);
                lr(1.0f - stroke, stroke, 1.0f, 0.9f - stroke * 0.5f);
                lr(0.0f, 0.0f, 1.0f - stroke, stroke);
                break;
            case 'T':
                lr(0.0f, 1.8f - stroke, 1.0f, 1.8f);
                lr(0.5f - stroke * 0.5f, 0.0f, 0.5f + stroke * 0.5f, 1.8f - stroke);
                break;
            case 'A':
                lr(0.0f, 0.0f, stroke, 1.8f);
                lr(1.0f - stroke, 0.0f, 1.0f, 1.8f);
                lr(stroke, 1.8f - stroke, 1.0f - stroke, 1.8f);
                lr(stroke, 0.9f - stroke * 0.5f, 1.0f - stroke, 0.9f + stroke * 0.5f);
                break;
            case 'M':
                lr(0.0f, 0.0f, stroke, 1.8f);
                lr(1.2f - stroke, 0.0f, 1.2f, 1.8f);
                lr(stroke, 1.8f - stroke, 0.6f, 1.8f);
                lr(0.6f, 1.8f - stroke, 1.2f - stroke, 1.8f);
                lr(0.6f - stroke * 0.5f, 0.9f, 0.6f + stroke * 0.5f, 1.8f - stroke);
                break;
            case 'I':
                lr(0.35f - stroke * 0.5f, 0.0f, 0.35f + stroke * 0.5f, 1.8f);
                lr(0.0f, 1.8f - stroke, 0.7f, 1.8f);
                lr(0.0f, 0.0f, 0.7f, stroke);
                break;
            case 'N':
                lr(0.0f, 0.0f, stroke, 1.8f);
                lr(1.0f - stroke, 0.0f, 1.0f, 1.8f);
                lr(stroke, 1.8f - stroke, 1.0f - stroke, 1.8f);
                break;
            case 'U':
                lr(0.0f, stroke, stroke, 1.8f);
                lr(1.0f - stroke, stroke, 1.0f, 1.8f);
                lr(stroke, 0.0f, 1.0f - stroke, stroke);
                break;
            case ' ':
                break;
            default:
                break;
            }
            x += letterWidth * scale + spacing;
        }
    };

    auto add_game_over_selection_brackets = [&](float centerX, float centerY, float textWidth, float scale, const ColorRgb& color, float alpha, float emissiveStrength, float sceneAlphaScale = 1.0f) {
        const float bracketGap = scale * 0.85f;
        const float bracketThickness = scale * 0.20f;
        const float bracketStub = scale * 0.50f;
        const float y0 = centerY - (1.8f * scale) * 0.5f - scale * 0.12f;
        const float y1 = centerY + (1.8f * scale) * 0.5f + scale * 0.12f;

        const float leftInner = centerX - textWidth * 0.5f - bracketGap;
        const float leftOuter = leftInner - bracketThickness;
        const float rightOuter = centerX + textWidth * 0.5f + bracketGap + bracketThickness;
        const float rightInner = rightOuter - bracketThickness;

        add_rect(leftOuter, y0, leftInner, y1, color, alpha, emissiveStrength, sceneAlphaScale);
        add_rect(leftOuter, y1 - bracketThickness, leftOuter + bracketStub, y1, color, alpha, emissiveStrength, sceneAlphaScale);
        add_rect(leftOuter, y0, leftOuter + bracketStub, y0 + bracketThickness, color, alpha, emissiveStrength, sceneAlphaScale);

        add_rect(rightInner, y0, rightOuter, y1, color, alpha, emissiveStrength, sceneAlphaScale);
        add_rect(rightOuter - bracketStub, y1 - bracketThickness, rightOuter, y1, color, alpha, emissiveStrength, sceneAlphaScale);
        add_rect(rightOuter - bracketStub, y0, rightOuter, y0 + bracketThickness, color, alpha, emissiveStrength, sceneAlphaScale);
    };

    {
        const ScoreFeedbackTuning& scoreFeedback = kScoreFeedbackTuning;
        const float flashEnergy = std::max(hudState.scoreFlashEnergy, 0.0f);
        const float flashMix = 1.0f - std::exp(-flashEnergy * scoreFeedback.scoreTintResponse);
        const float milestoneEnergy = std::max(hudState.scoreMilestoneFlashEnergy, 0.0f);
        const float milestoneMix = 1.0f - std::exp(-milestoneEnergy * scoreFeedback.milestoneTintResponse);
        const float scoreScale = 0.026f;
        const float topY = 1.0f - kHudEdgePaddingY;
        const float rightX = 1.0f - kHudEdgePaddingX;
        const ColorRgb scoreBaseColor = scoreFeedback.baseColor;
        const ColorRgb scoreFlashColor = mix_color(scoreBaseColor, hudState.laserColor, flashMix);
        const ColorRgb scoreCoreColor = mix_color(scoreFlashColor, hudState.scoreMilestoneColor, milestoneMix);
        const float scoreEmission =
            scoreFeedback.baseEmission +
            flashEnergy * scoreFeedback.scoreEmissionPerEnergy +
            milestoneEnergy * scoreFeedback.milestoneEmissionPerEnergy;
        const float glowEnergy =
            flashEnergy * scoreFeedback.scoreGlowEnergyWeight +
            milestoneEnergy * scoreFeedback.milestoneGlowEnergyWeight;
        const float glowMix = 1.0f - std::exp(-glowEnergy);

        if (glowMix > 0.001f) {
            add_number(
                hudState.score,
                rightX,
                topY,
                scoreScale * scoreFeedback.outerGlow.scaleMultiplier,
                scoreCoreColor,
                scoreFeedback.outerGlow.bloomAlpha * glowMix,
                scoreEmission * scoreFeedback.outerGlow.emissionScale * glowMix,
                scoreFeedback.outerGlow.sceneAlphaScale
            );
            add_number(
                hudState.score,
                rightX,
                topY,
                scoreScale * scoreFeedback.innerGlow.scaleMultiplier,
                scoreCoreColor,
                scoreFeedback.innerGlow.bloomAlpha * glowMix,
                scoreEmission * scoreFeedback.innerGlow.emissionScale * glowMix,
                scoreFeedback.innerGlow.sceneAlphaScale
            );
        }

        add_number(
            hudState.score,
            rightX,
            topY,
            scoreScale,
            scoreCoreColor,
            0.98f,
            scoreEmission
        );

        if (hudState.recentScorePopupValue > 0 && hudState.recentScorePopupTimer > 0.0f) {
            const ScorePopupTuning& popup = scoreFeedback.popup;
            const float normalizedRemaining = std::clamp(
                hudState.recentScorePopupTimer / popup.lifetimeSeconds,
                0.0f,
                1.0f
            );
            const float popupAlpha = popup.baseAlpha * (1.0f - std::pow(1.0f - normalizedRemaining, popup.fadeExponent));
            const float popupGlowMix = 1.0f - std::exp(-popupAlpha * popup.glowResponse);
            const float riseProgress = 1.0f - normalizedRemaining;
            const float popupTopY =
                topY - dh * scoreScale - popup.verticalGap + popup.riseDistance * riseProgress;
            const float popupEmission = popup.baseEmission * (0.65f + popupAlpha * 0.35f);

            if (popupGlowMix > 0.001f) {
                add_score_delta_popup(
                    hudState.recentScorePopupValue,
                    rightX,
                    popupTopY,
                    popup.scale * popup.outerGlow.scaleMultiplier,
                    hudState.laserColor,
                    popup.outerGlow.bloomAlpha * popupGlowMix,
                    popupEmission * popup.outerGlow.emissionScale * popupGlowMix,
                    popup.outerGlow.sceneAlphaScale
                );
                add_score_delta_popup(
                    hudState.recentScorePopupValue,
                    rightX,
                    popupTopY,
                    popup.scale * popup.innerGlow.scaleMultiplier,
                    hudState.laserColor,
                    popup.innerGlow.bloomAlpha * popupGlowMix,
                    popupEmission * popup.innerGlow.emissionScale * popupGlowMix,
                    popup.innerGlow.sceneAlphaScale
                );
            }

            add_score_delta_popup(
                hudState.recentScorePopupValue,
                rightX,
                popupTopY,
                popup.scale,
                hudState.laserColor,
                popupAlpha,
                popupEmission
            );
        }
    }

    {
        const std::uint32_t displayLives = (hudState.lives > 0) ? hudState.lives - 1 : 0;
        const float shipScale = kHudLivesShipScale;
        const float startX = -1.0f + kHudEdgePaddingX + 4.0f * shipScale;
        const float shipY = 1.0f - kHudEdgePaddingY - 4.0f * shipScale;
        const float shipSpacing = kHudLivesShipSpacing;
        const ColorRgb shipColor{0.541f, 0.082f, 0.220f};
        const float livesEmission = 0.9f;

        for (std::uint32_t i = 0; i < displayLives && i < 10; ++i) {
            const float centerX = startX + static_cast<float>(i) * shipSpacing;
            add_ship(centerX, shipY, shipScale, shipColor, 0.96f, livesEmission);
        }
    }

    if (hudState.waveAnnouncementTimer > 0.0f && hudState.phase != GamePhase::GameOver) {
        const float normalizedRemaining = std::clamp(
            hudState.waveAnnouncementTimer / GameState::kWaveAnnouncementSeconds,
            0.0f,
            1.0f
        );
        const float alpha = 0.94f * std::min(1.0f, normalizedRemaining * 3.0f);
        const float labelScale = 0.032f;
        const float numberScale = 0.050f;
        const ColorRgb levelColor{0.82f, 0.84f, 0.92f};
        const float numberTopY = -0.03f + dh * numberScale * 0.5f;

        add_level_label(0.0f, 0.10f, labelScale, levelColor, alpha, 0.0f);
        add_number_centered(hudState.wave, 0.0f, numberTopY, numberScale, levelColor, alpha, 0.0f);
    }

    if (hudState.phase == GamePhase::GameOver) {
        const float scale = 0.048f;
        const float letterSpacing = 1.3f * scale;
        const float spaceWidth = 0.8f * scale;
        const float lw = 0.18f;
        const ColorRgb gameOverColor{0.95f, 0.18f, 0.16f};
        const float gameOverEmission = 2.1f;
        const float totalWidth = 8.0f * letterSpacing + spaceWidth;
        float cx = -totalWidth * 0.5f;
        const float cy = -0.04f;

        auto letter_rect = [&](float lx0, float ly0, float lx1, float ly1) {
            add_rect(
                cx + lx0 * scale,
                cy + ly0 * scale,
                cx + lx1 * scale,
                cy + ly1 * scale,
                gameOverColor,
                0.98f,
                gameOverEmission
            );
        };

        letter_rect(lw, 1.8f - lw, 1.0f, 1.8f);
        letter_rect(0.0f, lw, lw, 1.8f);
        letter_rect(lw, 0.0f, 1.0f, lw);
        letter_rect(1.0f - lw, 0.0f, 1.0f, 0.9f + lw * 0.5f);
        letter_rect(0.5f, 0.9f - lw * 0.5f, 1.0f, 0.9f + lw * 0.5f);
        cx += letterSpacing;

        letter_rect(lw, 1.8f - lw, 1.0f - lw, 1.8f);
        letter_rect(0.0f, 0.0f, lw, 1.8f);
        letter_rect(1.0f - lw, 0.0f, 1.0f, 1.8f);
        letter_rect(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
        cx += letterSpacing;

        letter_rect(0.0f, 0.0f, lw, 1.8f);
        letter_rect(1.0f - lw, 0.0f, 1.0f, 1.8f);
        letter_rect(lw, 1.8f - lw, 1.0f - lw, 1.8f);
        letter_rect(0.5f - lw * 0.5f, 0.9f, 0.5f + lw * 0.5f, 1.8f - lw);
        cx += letterSpacing;

        letter_rect(0.0f, 0.0f, lw, 1.8f);
        letter_rect(lw, 1.8f - lw, 1.0f, 1.8f);
        letter_rect(lw, 0.9f - lw * 0.5f, 0.8f, 0.9f + lw * 0.5f);
        letter_rect(lw, 0.0f, 1.0f, lw);
        cx += letterSpacing;

        cx += spaceWidth;

        letter_rect(lw, 1.8f - lw, 1.0f - lw, 1.8f);
        letter_rect(0.0f, lw, lw, 1.8f - lw);
        letter_rect(1.0f - lw, lw, 1.0f, 1.8f - lw);
        letter_rect(lw, 0.0f, 1.0f - lw, lw);
        cx += letterSpacing;

        letter_rect(0.0f, 0.9f, lw, 1.8f);
        letter_rect(0.0f, 0.0f, lw * 1.5f, 0.9f);
        letter_rect(1.0f - lw, 0.9f, 1.0f, 1.8f);
        letter_rect(1.0f - lw * 1.5f, 0.0f, 1.0f, 0.9f);
        letter_rect(lw * 0.5f, 0.0f, 1.0f - lw * 0.5f, lw);
        cx += letterSpacing;

        letter_rect(0.0f, 0.0f, lw, 1.8f);
        letter_rect(lw, 1.8f - lw, 1.0f, 1.8f);
        letter_rect(lw, 0.9f - lw * 0.5f, 0.8f, 0.9f + lw * 0.5f);
        letter_rect(lw, 0.0f, 1.0f, lw);
        cx += letterSpacing;

        letter_rect(0.0f, 0.0f, lw, 1.8f);
        letter_rect(lw, 1.8f - lw, 1.0f - lw, 1.8f);
        letter_rect(1.0f - lw, 0.9f, 1.0f, 1.8f - lw);
        letter_rect(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
        letter_rect(1.0f - lw, 0.0f, 1.0f, 0.9f - lw * 0.5f);

        if (hudState.restartPromptVisible) {
            constexpr ColorRgb selectedColor{0.96f, 0.77f, 0.18f};
            constexpr ColorRgb idleColor{0.55f, 0.56f, 0.62f};
            for (std::size_t buttonIndex = 0; buttonIndex < kGameOverMenuButtons.size(); ++buttonIndex) {
                const bool selected = hudState.gameOverMenuSelectedIndex == buttonIndex;
                const ColorRgb color = selected ? selectedColor : idleColor;
                const float emission = selected ? 3.5f : 1.2f;
                const float textWidth = measure_game_over_menu_text(
                    kGameOverMenuButtons[buttonIndex].label,
                    kGameOverMenuButtonScale,
                    kGameOverMenuButtonLetterSpacing
                );
                if (selected) {
                    add_game_over_selection_brackets(
                        0.0f,
                        kGameOverMenuButtons[buttonIndex].centerY,
                        textWidth,
                        kGameOverMenuButtonScale,
                        color,
                        0.98f,
                        emission
                    );
                }
                add_game_over_menu_text(
                    kGameOverMenuButtons[buttonIndex].label,
                    0.0f,
                    kGameOverMenuButtons[buttonIndex].centerY,
                    kGameOverMenuButtonScale,
                    color,
                    0.98f,
                    emission
                );
            }
        }
    }

    hudVertexCount_ = std::min(vertices.size(), kMaxHudVertices);
    if (hudVertexCount_ > 0) {
        std::memcpy(hudBufferMapped_, vertices.data(), hudVertexCount_ * sizeof(HudVertex));
    }
}

void VulkanRenderer::create_menu_buffer() {
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(kMaxMenuVertices * sizeof(HudVertex));
    create_buffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        menuBuffer_,
        menuBufferMemory_
    );

    if (vkMapMemory(device_, menuBufferMemory_, 0, bufferSize, 0, &menuBufferMapped_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map menu vertex buffer.");
    }
}

void VulkanRenderer::update_menu_buffer(const MenuOverlayState& menuOverlay) {
    std::vector<HudVertex> vertices;
    vertices.reserve(kMaxMenuVertices);

    auto make_vertex = [](const Vec2& position, const ColorRgb& color, float alpha, float emissiveStrength) {
        HudVertex vertex{};
        vertex.position[0] = position.x;
        vertex.position[1] = position.y;
        vertex.color[0] = color.r;
        vertex.color[1] = color.g;
        vertex.color[2] = color.b;
        vertex.color[3] = alpha;
        vertex.params[0] = emissiveStrength;
        return vertex;
    };

    auto add_rect = [&](float x0, float y0, float x1, float y1, const ColorRgb& color, float alpha, float emissiveStrength) {
        const HudVertex v0 = make_vertex({x0, y0}, color, alpha, emissiveStrength);
        const HudVertex v1 = make_vertex({x1, y0}, color, alpha, emissiveStrength);
        const HudVertex v2 = make_vertex({x1, y1}, color, alpha, emissiveStrength);
        const HudVertex v3 = make_vertex({x0, y1}, color, alpha, emissiveStrength);
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v0);
        vertices.push_back(v2);
        vertices.push_back(v3);
    };

    struct LetterDef {
        char ch;
        float width;
    };

    constexpr float lw = 0.18f;

    auto draw_letter = [&](char ch, float cx, float cy, float scale, const ColorRgb& color, float alpha, float emissiveStrength) -> float {
        auto lr = [&](float lx0, float ly0, float lx1, float ly1) {
            add_rect(
                cx + lx0 * scale, cy + ly0 * scale,
                cx + lx1 * scale, cy + ly1 * scale,
                color, alpha, emissiveStrength
            );
        };

        float letterWidth = 1.0f;

        // 7-segment helpers for digits (reused per digit case)
        auto seg_T  = [&]{ lr(lw, 1.8f - lw, 1.0f - lw, 1.8f); };
        auto seg_TL = [&]{ lr(0.0f, 0.9f + lw * 0.5f, lw, 1.8f - lw); };
        auto seg_TR = [&]{ lr(1.0f - lw, 0.9f + lw * 0.5f, 1.0f, 1.8f - lw); };
        auto seg_M  = [&]{ lr(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f); };
        auto seg_BL = [&]{ lr(0.0f, lw, lw, 0.9f - lw * 0.5f); };
        auto seg_BR = [&]{ lr(1.0f - lw, lw, 1.0f, 0.9f - lw * 0.5f); };
        auto seg_B  = [&]{ lr(lw, 0.0f, 1.0f - lw, lw); };

        switch (ch) {
            case 'A':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(1.0f - lw, 0.0f, 1.0f, 1.8f);
                lr(lw, 1.8f - lw, 1.0f - lw, 1.8f);
                lr(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
                break;
            case 'B':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 1.8f - lw, 0.8f, 1.8f);
                lr(0.8f, 0.9f + lw * 0.5f, 0.8f + lw, 1.8f - lw);
                lr(lw, 0.9f - lw * 0.5f, 0.8f + lw, 0.9f + lw * 0.5f);
                lr(0.8f, lw, 0.8f + lw, 0.9f - lw * 0.5f);
                lr(lw, 0.0f, 0.8f, lw);
                letterWidth = 0.8f + lw;
                break;
            case 'C':
                lr(lw, 1.8f - lw, 1.0f, 1.8f);
                lr(0.0f, lw, lw, 1.8f - lw);
                lr(lw, 0.0f, 1.0f, lw);
                break;
            case 'D':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 1.8f - lw, 0.75f, 1.8f);
                lr(lw, 0.0f, 0.75f, lw);
                lr(0.75f, lw, 0.75f + lw, 0.9f - lw * 0.25f);
                lr(0.75f, 0.9f + lw * 0.25f, 0.75f + lw, 1.8f - lw);
                letterWidth = 0.75f + lw;
                break;
            case 'E':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 1.8f - lw, 1.0f, 1.8f);
                lr(lw, 0.9f - lw * 0.5f, 0.8f, 0.9f + lw * 0.5f);
                lr(lw, 0.0f, 1.0f, lw);
                break;
            case 'F':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 1.8f - lw, 1.0f, 1.8f);
                lr(lw, 0.9f - lw * 0.5f, 0.8f, 0.9f + lw * 0.5f);
                break;
            case 'G':
                lr(lw, 1.8f - lw, 1.0f, 1.8f);
                lr(0.0f, lw, lw, 1.8f - lw);
                lr(lw, 0.0f, 1.0f - lw, lw);
                lr(1.0f - lw, 0.0f, 1.0f, 0.9f + lw * 0.5f);
                lr(0.5f, 0.9f - lw * 0.5f, 1.0f, 0.9f + lw * 0.5f);
                break;
            case 'H':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(1.0f - lw, 0.0f, 1.0f, 1.8f);
                lr(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
                break;
            case 'I':
                lr(0.5f - lw * 0.5f, 0.0f, 0.5f + lw * 0.5f, 1.8f);
                lr(0.0f, 1.8f - lw, 1.0f, 1.8f);
                lr(0.0f, 0.0f, 1.0f, lw);
                break;
            case 'J':
                lr(1.0f - lw, lw, 1.0f, 1.8f);
                lr(0.0f, 0.0f, 1.0f - lw, lw);
                lr(0.0f, lw, lw, 0.5f);
                break;
            case 'K':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 0.9f - lw * 0.5f, 0.55f, 0.9f + lw * 0.5f);
                lr(0.55f, 0.9f + lw * 0.5f, 0.55f + lw, 1.8f - lw);
                lr(0.55f + lw, 1.8f - lw, 1.0f, 1.8f);
                lr(0.55f, lw, 0.55f + lw, 0.9f - lw * 0.5f);
                lr(0.55f + lw, 0.0f, 1.0f, lw);
                break;
            case 'L':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 0.0f, 1.0f, lw);
                break;
            case 'M':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(1.2f - lw, 0.0f, 1.2f, 1.8f);
                lr(lw, 1.8f - lw, 0.6f, 1.8f);
                lr(0.6f, 1.8f - lw, 1.2f - lw, 1.8f);
                lr(0.6f - lw * 0.5f, 0.9f, 0.6f + lw * 0.5f, 1.8f - lw);
                letterWidth = 1.2f;
                break;
            case 'N':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(1.0f - lw, 0.0f, 1.0f, 1.8f);
                lr(lw, 1.8f - lw, 1.0f - lw, 1.8f);
                break;
            case 'O':
                lr(lw, 1.8f - lw, 1.0f - lw, 1.8f);
                lr(0.0f, lw, lw, 1.8f - lw);
                lr(1.0f - lw, lw, 1.0f, 1.8f - lw);
                lr(lw, 0.0f, 1.0f - lw, lw);
                break;
            case 'P':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 1.8f - lw, 1.0f - lw, 1.8f);
                lr(1.0f - lw, 0.9f, 1.0f, 1.8f - lw);
                lr(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
                break;
            case 'Q':
                lr(lw, 1.8f - lw, 1.0f - lw, 1.8f);
                lr(0.0f, lw, lw, 1.8f - lw);
                lr(1.0f - lw, lw, 1.0f, 1.8f - lw);
                lr(lw, 0.0f, 1.0f - lw, lw);
                lr(0.6f, 0.0f, 1.1f, lw);
                letterWidth = 1.1f;
                break;
            case 'R':
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(lw, 1.8f - lw, 1.0f - lw, 1.8f);
                lr(1.0f - lw, 0.9f, 1.0f, 1.8f - lw);
                lr(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
                lr(0.5f, 0.0f, 0.5f + lw, 0.9f - lw * 0.5f);
                lr(0.5f + lw, 0.0f, 1.0f, lw);
                break;
            case 'S':
                lr(lw, 1.8f - lw, 1.0f, 1.8f);
                lr(0.0f, 0.9f + lw * 0.5f, lw, 1.8f - lw);
                lr(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
                lr(1.0f - lw, lw, 1.0f, 0.9f - lw * 0.5f);
                lr(0.0f, 0.0f, 1.0f - lw, lw);
                break;
            case 'T':
                lr(0.0f, 1.8f - lw, 1.0f, 1.8f);
                lr(0.5f - lw * 0.5f, 0.0f, 0.5f + lw * 0.5f, 1.8f - lw);
                break;
            case 'U':
                lr(0.0f, lw, lw, 1.8f);
                lr(1.0f - lw, lw, 1.0f, 1.8f);
                lr(lw, 0.0f, 1.0f - lw, lw);
                break;
            case 'V':
                // Wide-top, narrow-bottom stepped shape
                lr(0.0f, 0.9f, lw, 1.8f);                          // upper-left stem
                lr(1.0f - lw, 0.9f, 1.0f, 1.8f);                   // upper-right stem
                lr(lw, 0.3f, lw * 2.0f, 0.9f + lw);                // lower-left inner
                lr(1.0f - lw * 2.0f, 0.3f, 1.0f - lw, 0.9f + lw); // lower-right inner
                lr(lw * 2.0f, 0.0f, 1.0f - lw * 2.0f, lw);        // bottom bar
                break;
            case 'W':
                // Upside-down M
                lr(0.0f, 0.0f, lw, 1.8f);
                lr(1.2f - lw, 0.0f, 1.2f, 1.8f);
                lr(lw, 0.0f, 0.6f, lw);
                lr(0.6f, 0.0f, 1.2f - lw, lw);
                lr(0.6f - lw * 0.5f, lw, 0.6f + lw * 0.5f, 0.9f);
                letterWidth = 1.2f;
                break;
            case 'X':
                // Four corner stubs + inner steps + center
                lr(0.0f, 1.8f - lw * 2.5f, lw, 1.8f);
                lr(1.0f - lw, 1.8f - lw * 2.5f, 1.0f, 1.8f);
                lr(0.0f, 0.0f, lw, lw * 2.5f);
                lr(1.0f - lw, 0.0f, 1.0f, lw * 2.5f);
                lr(lw, 1.2f, lw * 2.0f, 1.8f - lw * 2.5f);
                lr(1.0f - lw * 2.0f, 1.2f, 1.0f - lw, 1.8f - lw * 2.5f);
                lr(lw, lw * 2.5f, lw * 2.0f, 0.6f);
                lr(1.0f - lw * 2.0f, lw * 2.5f, 1.0f - lw, 0.6f);
                lr(lw * 2.0f, 0.6f, 1.0f - lw * 2.0f, 1.2f);
                break;
            case 'Y':
                lr(0.0f, 0.9f, lw, 1.8f);
                lr(1.0f - lw, 0.9f, 1.0f, 1.8f);
                lr(lw, 0.9f - lw * 0.5f, 1.0f - lw, 0.9f + lw * 0.5f);
                lr(0.5f - lw * 0.5f, 0.0f, 0.5f + lw * 0.5f, 0.9f - lw * 0.5f);
                break;
            case 'Z':
                lr(0.0f, 1.8f - lw, 1.0f, 1.8f);
                lr(0.0f, 0.0f, 1.0f, lw);
                lr(0.0f, 1.1f, lw, 1.8f - lw);
                lr(0.0f, 1.1f - lw, 1.0f, 1.1f);
                lr(1.0f - lw, lw, 1.0f, 0.7f);
                lr(0.0f, 0.7f - lw, 1.0f, 0.7f);
                break;
            case '0': seg_T(); seg_TL(); seg_TR(); seg_BL(); seg_BR(); seg_B(); break;
            case '1':
                lr(0.35f, 0.0f, 0.65f, 1.8f);
                letterWidth = 0.65f;
                break;
            case '2': seg_T(); seg_TR(); seg_M(); seg_BL(); seg_B(); break;
            case '3': seg_T(); seg_TR(); seg_M(); seg_BR(); seg_B(); break;
            case '4': seg_TL(); seg_M(); seg_TR(); seg_BR(); break;
            case '5': seg_T(); seg_TL(); seg_M(); seg_BR(); seg_B(); break;
            case '6': seg_T(); seg_TL(); seg_M(); seg_BL(); seg_BR(); seg_B(); break;
            case '7': seg_T(); seg_TR(); seg_BR(); break;
            case '8': seg_T(); seg_TL(); seg_TR(); seg_M(); seg_BL(); seg_BR(); seg_B(); break;
            case '9': seg_T(); seg_TL(); seg_TR(); seg_M(); seg_BR(); seg_B(); break;
            case ':':
                lr(0.2f, 0.35f, 0.8f, 0.65f);
                lr(0.2f, 1.15f, 0.8f, 1.45f);
                letterWidth = 0.7f;
                break;
            case '+':
                lr(0.28f, 0.78f, 0.44f, 1.02f);
                lr(0.16f, 0.90f, 0.56f, 1.14f);
                lr(0.28f, 1.02f, 0.44f, 1.26f);
                letterWidth = 0.72f;
                break;
            case '-':
                lr(0.12f, 0.90f, 0.60f, 1.14f);
                letterWidth = 0.72f;
                break;
            case '/':
                lr(0.50f, 0.0f, 0.72f, 0.24f);
                lr(0.38f, 0.36f, 0.60f, 0.60f);
                lr(0.26f, 0.72f, 0.48f, 0.96f);
                lr(0.14f, 1.08f, 0.36f, 1.32f);
                lr(0.02f, 1.44f, 0.24f, 1.68f);
                letterWidth = 0.72f;
                break;
            case ' ':
                letterWidth = 0.5f;
                break;
            default:
                break;
        }
        return letterWidth * scale;
    };

    auto measure_text = [&](const char* text, float scale, float spacing) -> float {
        float width = 0.0f;
        for (const char* p = text; *p != '\0'; ++p) {
            if (p != text) width += spacing;
            // Use same letter widths as draw_letter
            float letterWidth = 1.0f;
            switch (*p) {
                case 'B': letterWidth = 0.8f + lw; break;
                case 'D': letterWidth = 0.75f + lw; break;
                case 'M': letterWidth = 1.2f; break;
                case 'Q': letterWidth = 1.1f; break;
                case 'W': letterWidth = 1.2f; break;
                case '1': letterWidth = 0.65f; break;
                case ':': letterWidth = 0.7f; break;
                case '+': letterWidth = 0.72f; break;
                case '-': letterWidth = 0.72f; break;
                case '/': letterWidth = 0.72f; break;
                case ' ': letterWidth = 0.5f; break;
                default: break;
            }
            width += letterWidth * scale;
        }
        return width;
    };

    auto draw_text = [&](const char* text, float centerX, float centerY, float scale, float spacing, const ColorRgb& color, float alpha, float emissiveStrength) {
        const float totalWidth = measure_text(text, scale, spacing);
        float x = centerX - totalWidth * 0.5f;
        for (const char* p = text; *p != '\0'; ++p) {
            float w = draw_letter(*p, x, centerY, scale, color, alpha, emissiveStrength);
            x += w + spacing;
        }
    };

    auto draw_selection_brackets = [&](float centerX, float baselineY, float textWidth, float scale, const ColorRgb& color, float alpha, float emissiveStrength) {
        constexpr float kGlyphHeight = 1.8f;

        const float bracketGap       = scale * 0.85f;
        const float bracketThickness = scale * 0.20f;
        const float bracketStub      = scale * 0.50f;
        const float y0               = baselineY - scale * 0.12f;
        const float y1               = baselineY + kGlyphHeight * scale + scale * 0.12f;

        const float leftInner  = centerX - textWidth * 0.5f - bracketGap;
        const float leftOuter  = leftInner - bracketThickness;
        const float rightOuter = centerX + textWidth * 0.5f + bracketGap + bracketThickness;
        const float rightInner = rightOuter - bracketThickness;

        add_rect(leftOuter, y0, leftInner, y1, color, alpha, emissiveStrength);
        add_rect(leftOuter, y1 - bracketThickness, leftOuter + bracketStub, y1, color, alpha, emissiveStrength);
        add_rect(leftOuter, y0, leftOuter + bracketStub, y0 + bracketThickness, color, alpha, emissiveStrength);

        add_rect(rightInner, y0, rightOuter, y1, color, alpha, emissiveStrength);
        add_rect(rightOuter - bracketStub, y1 - bracketThickness, rightOuter, y1, color, alpha, emissiveStrength);
        add_rect(rightOuter - bracketStub, y0, rightOuter, y0 + bracketThickness, color, alpha, emissiveStrength);
    };

    // -----------------------------------------------------------------------
    // Title
    // -----------------------------------------------------------------------
    const MenuLayout::OverlayLayout layout = MenuLayout::compute_overlay_layout(
        menuOverlay.lines,
        menuOverlay.placement
    );

    if (menuOverlay.title != nullptr) {
        constexpr ColorRgb titleColor{0.86f, 0.88f, 0.96f};
        constexpr float titleScale   = MenuLayout::kTitleScale;
        const     float titleSpacing = 0.3f * titleScale;
        draw_text(menuOverlay.title, 0.0f, layout.titleY, titleScale, titleSpacing, titleColor, 0.98f, 3.0f);
    }

    // -----------------------------------------------------------------------
    // Lines
    // -----------------------------------------------------------------------
    // selectedIndex counts only Selectable lines.
    std::size_t selectablesSeen = 0;
    float y = layout.linesStartY;

    for (const MenuLine& line : menuOverlay.lines) {
        // Extra gap before this line (e.g. visual section break)
        y -= line.extraGapBefore;

        switch (line.type) {
        case MenuLineType::Selectable: {
            const bool isSelected = (selectablesSeen == menuOverlay.selectedIndex);
            ++selectablesSeen;
            ColorRgb color;
            float emission;
            if (isSelected) {
                color    = line.accented ? ColorRgb{1.0f, 0.88f, 0.30f} : ColorRgb{0.96f, 0.77f, 0.18f};
                emission = 3.5f;
            } else if (line.accented) {
                color    = ColorRgb{0.70f, 0.74f, 0.84f};
                emission = 1.05f;
            } else {
                color    = ColorRgb{0.55f, 0.56f, 0.62f};
                emission = 1.2f;
            }
            if (line.label != nullptr) {
                constexpr float scale   = MenuLayout::kItemScale;
                const     float spacing = 0.3f * scale;
                const     float textWidth = measure_text(line.label, scale, spacing);
                if (isSelected) {
                    draw_selection_brackets(0.0f, y, textWidth, scale, color, 0.98f, emission);
                }
                draw_text(line.label, 0.0f, y, scale, spacing, color, 0.98f, emission);
            }
            y -= MenuLayout::kSelectableStep;
            break;
        }
        case MenuLineType::Info: {
            if (line.label != nullptr) {
                constexpr ColorRgb color{0.55f, 0.56f, 0.62f};
                constexpr float scale   = MenuLayout::kInfoScale;
                const     float spacing = 0.3f * scale;
                draw_text(line.label, 0.0f, y, scale, spacing, color, 0.75f, 0.9f);
            }
            y -= MenuLayout::kInfoStep;
            break;
        }
        case MenuLineType::Stat: {
            // Label left-aligned at -0.38, value right-aligned at +0.38
            constexpr float scale    = MenuLayout::kStatLabelScale;
            const     float spacing  = 0.3f * scale;
            constexpr ColorRgb labelColor{0.50f, 0.52f, 0.58f};
            constexpr ColorRgb valueColor{0.80f, 0.82f, 0.90f};
            if (line.label != nullptr) {
                const float tw = measure_text(line.label, scale, spacing);
                draw_text(line.label, -0.38f + tw * 0.5f, y, scale, spacing, labelColor, 0.85f, 0.9f);
            }
            if (line.value != nullptr) {
                const float tw = measure_text(line.value, scale, spacing);
                draw_text(line.value, 0.38f - tw * 0.5f, y, scale, spacing, valueColor, 0.95f, 1.8f);
            }
            y -= MenuLayout::kStatStep;
            break;
        }
        }
    }

    // -----------------------------------------------------------------------
    // Bottom status line
    // -----------------------------------------------------------------------
    if (menuOverlay.bottomStatus != nullptr) {
        constexpr ColorRgb color{0.46f, 0.48f, 0.54f};
        constexpr float scale   = MenuLayout::kBottomStatusScale;
        const     float spacing = 0.3f * scale;
        draw_text(menuOverlay.bottomStatus, 0.0f, MenuLayout::kBottomStatusY, scale, spacing, color, 0.7f, 0.8f);
    }

    menuVertexCount_ = std::min(vertices.size(), kMaxMenuVertices);
    if (menuVertexCount_ > 0) {
        std::memcpy(menuBufferMapped_, vertices.data(), menuVertexCount_ * sizeof(HudVertex));
    }
}

void VulkanRenderer::create_command_buffers() {
    commandBuffers_.resize(kMaxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers.");
    }
}

void VulkanRenderer::create_sync_objects() {
    imageAvailableSemaphores_.resize(kMaxFramesInFlight);
    renderFinishedSemaphores_.resize(kMaxFramesInFlight);
    inFlightFences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int index = 0; index < kMaxFramesInFlight; ++index) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[index]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[index]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[index]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects.");
        }
    }
}

void VulkanRenderer::cleanup_swapchain() {
    for (VkFramebuffer framebuffer : swapchainFramebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    swapchainFramebuffers_.clear();

    destroy_offscreen_target(lightTarget_);
    destroy_offscreen_target(asteroidLightTarget_);
    destroy_offscreen_target(sceneTarget_);
    destroy_offscreen_target(brightTarget_);
    for (OffscreenTarget& target : blurTargets_) {
        destroy_offscreen_target(target);
    }

    if (menuHudPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, menuHudPipeline_, nullptr);
        menuHudPipeline_ = VK_NULL_HANDLE;
    }
    if (menuHudPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, menuHudPipelineLayout_, nullptr);
        menuHudPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (hudPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, hudPipeline_, nullptr);
        hudPipeline_ = VK_NULL_HANDLE;
    }
    if (hudPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, hudPipelineLayout_, nullptr);
        hudPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (compositePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, compositePipeline_, nullptr);
        compositePipeline_ = VK_NULL_HANDLE;
    }
    if (compositePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, compositePipelineLayout_, nullptr);
        compositePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (blurPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, blurPipeline_, nullptr);
        blurPipeline_ = VK_NULL_HANDLE;
    }
    if (blurPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, blurPipelineLayout_, nullptr);
        blurPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (particleLightPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, particleLightPipeline_, nullptr);
        particleLightPipeline_ = VK_NULL_HANDLE;
    }
    if (particleLightPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, particleLightPipelineLayout_, nullptr);
        particleLightPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (particleScenePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, particleScenePipeline_, nullptr);
        particleScenePipeline_ = VK_NULL_HANDLE;
    }
    if (particleScenePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, particleScenePipelineLayout_, nullptr);
        particleScenePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (starPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, starPipeline_, nullptr);
        starPipeline_ = VK_NULL_HANDLE;
    }
    if (starPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, starPipelineLayout_, nullptr);
        starPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (asteroidPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, asteroidPipeline_, nullptr);
        asteroidPipeline_ = VK_NULL_HANDLE;
    }
    if (asteroidPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, asteroidPipelineLayout_, nullptr);
        asteroidPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (shipPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, shipPipeline_, nullptr);
        shipPipeline_ = VK_NULL_HANDLE;
    }
    if (shipPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, shipPipelineLayout_, nullptr);
        shipPipelineLayout_ = VK_NULL_HANDLE;
    }

    if (lightRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, lightRenderPass_, nullptr);
        lightRenderPass_ = VK_NULL_HANDLE;
    }
    if (sceneRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, sceneRenderPass_, nullptr);
        sceneRenderPass_ = VK_NULL_HANDLE;
    }
    if (postProcessRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, postProcessRenderPass_, nullptr);
        postProcessRenderPass_ = VK_NULL_HANDLE;
    }
    if (compositeRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, compositeRenderPass_, nullptr);
        compositeRenderPass_ = VK_NULL_HANDLE;
    }

    for (VkImageView imageView : swapchainImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    imagesInFlight_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    swapchainExtent_ = {};
}

void VulkanRenderer::recreate_swapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);

    cleanup_swapchain();
    create_swapchain();
    create_swapchain_image_views();
    create_render_passes();
    create_offscreen_targets();
    create_pipelines();
    create_framebuffers();
    update_descriptor_sets();
    create_starfield();
}

void VulkanRenderer::update_particle_buffer(std::span<const EffectParticleRenderData> particles) {
    if (particles.size() > maxParticleCount_) {
        throw std::runtime_error("Particle count exceeds renderer buffer capacity.");
    }

    auto make_vertex = [](const EffectParticleRenderData& particle) {
        return ParticleVertex{
            .position = {particle.position.x, particle.position.y},
            .color = {particle.color.r, particle.color.g, particle.color.b, particle.alpha},
            .params0 = {
                particle.size,
                particle.rotationRadians,
                particle.aspectRatio,
                static_cast<float>(particle.shape == ParticleShape::Rectangle ? 1.0f : 0.0f),
            },
            .params1 = {
                particle.glowScale,
                particle.glowIntensity,
                particle.bloomIntensity,
                particle.lightIntensity,
            },
        };
    };

    std::vector<ParticleVertex> upload;
    upload.reserve(particles.size());
    backgroundParticleCount_ = 0;

    for (const EffectParticleRenderData& particle : particles) {
        if (particle.renderLayer == ParticleRenderLayer::BehindAsteroids) {
            upload.push_back(make_vertex(particle));
            ++backgroundParticleCount_;
        }
    }

    for (const EffectParticleRenderData& particle : particles) {
        if (particle.renderLayer == ParticleRenderLayer::Front) {
            upload.push_back(make_vertex(particle));
        }
    }

    if (!upload.empty()) {
        std::memcpy(particleBufferMapped_, upload.data(), upload.size() * sizeof(ParticleVertex));
    }
}

void VulkanRenderer::update_asteroid_buffer(std::span<const AsteroidRenderData> asteroids) {
    std::vector<AsteroidVertex> upload;
    upload.reserve(asteroids.size() * AsteroidRenderData::kMaxVertexCount * 3);

    for (const AsteroidRenderData& asteroid : asteroids) {
        if (asteroid.vertexCount < 3) {
            continue;
        }

        const float cosine = std::cos(asteroid.rotationRadians);
        const float sine = std::sin(asteroid.rotationRadians);
        const auto rotate_and_translate = [&](const Vec2& point) {
            return Vec2{
                asteroid.position.x + point.x * cosine - point.y * sine,
                asteroid.position.y + point.x * sine + point.y * cosine,
            };
        };

        const Vec2 center = asteroid.position;
        const float seed = asteroid.shadingSeed;
        const float seedPhase = seed * 0.0137f;
        const float noiseScale = 0.16f + 0.06f * (0.5f + 0.5f * std::sin(seedPhase));
        const float facetDepth = 0.55f + 0.35f * (0.5f + 0.5f * std::sin(seedPhase * 1.7f + 1.1f));
        const float tintShift = 0.5f + 0.5f * std::sin(seedPhase * 2.3f + 2.4f);
        float maxRadius = 0.001f;
        for (std::size_t index = 0; index < asteroid.vertexCount; ++index) {
            maxRadius = std::max(maxRadius, length(asteroid.localVertices[index]));
        }
        const std::array<float, 4> basis = {
            cosine,
            sine,
            1.0f / maxRadius,
            0.0f,
        };
        for (std::size_t index = 0; index < asteroid.vertexCount; ++index) {
            const Vec2 localA = asteroid.localVertices[index];
            const Vec2 localB = asteroid.localVertices[(index + 1) % asteroid.vertexCount];
            const Vec2 vertexA = rotate_and_translate(asteroid.localVertices[index]);
            const Vec2 vertexB = rotate_and_translate(asteroid.localVertices[(index + 1) % asteroid.vertexCount]);
            upload.push_back({
                .position = {center.x, center.y},
                .localPosition = {0.0f, 0.0f},
                .variation = {seed, noiseScale, facetDepth, tintShift},
                .basis = {basis[0], basis[1], basis[2], basis[3]},
            });
            upload.push_back({
                .position = {vertexA.x, vertexA.y},
                .localPosition = {localA.x, localA.y},
                .variation = {seed, noiseScale, facetDepth, tintShift},
                .basis = {basis[0], basis[1], basis[2], basis[3]},
            });
            upload.push_back({
                .position = {vertexB.x, vertexB.y},
                .localPosition = {localB.x, localB.y},
                .variation = {seed, noiseScale, facetDepth, tintShift},
                .basis = {basis[0], basis[1], basis[2], basis[3]},
            });
        }
    }

    if (upload.size() > maxAsteroidVertexCount_) {
        throw std::runtime_error("Asteroid vertex count exceeds renderer buffer capacity.");
    }

    asteroidVertexCount_ = upload.size();
    if (!upload.empty()) {
        std::memcpy(asteroidBufferMapped_, upload.data(), upload.size() * sizeof(AsteroidVertex));
    }
}

void VulkanRenderer::create_buffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory
) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan buffer.");
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(memoryRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Vulkan buffer memory.");
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
}

uint32_t VulkanRenderer::find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);

    for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        const bool typeMatches = (typeFilter & (1u << index)) != 0;
        const bool propertiesMatch = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
        if (typeMatches && propertiesMatch) {
            return index;
        }
    }

    throw std::runtime_error("Failed to find a suitable Vulkan memory type.");
}

void VulkanRenderer::transition_image_to_shader_read(VkCommandBuffer commandBuffer, VkImage image) const {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}

void VulkanRenderer::record_command_buffer(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    const ShipState& shipState,
    std::span<const EffectParticleRenderData> particles,
    std::span<const AsteroidRenderData> asteroids,
    const HudState& hudState,
    RenderMode renderMode
) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer.");
    }

    const FixedAspectViewportLayout viewportLayout =
        compute_fixed_aspect_viewport_layout(
            static_cast<int>(swapchainExtent_.width),
            static_cast<int>(swapchainExtent_.height)
        );
    const Vec2 backgroundHalfExtents = compute_background_half_extents(
        viewportLayout,
        GameState::kWorldHalfWidth,
        GameState::kWorldHalfHeight
    );

    VkViewport fullViewport{};
    fullViewport.x = 0.0f;
    fullViewport.y = 0.0f;
    fullViewport.width = static_cast<float>(swapchainExtent_.width);
    fullViewport.height = static_cast<float>(swapchainExtent_.height);
    fullViewport.minDepth = 0.0f;
    fullViewport.maxDepth = 1.0f;

    VkRect2D fullScissor{};
    fullScissor.offset = {0, 0};
    fullScissor.extent = swapchainExtent_;

    VkViewport playableViewport{};
    playableViewport.x = static_cast<float>(viewportLayout.playableOffsetX);
    playableViewport.y = static_cast<float>(viewportLayout.playableOffsetY);
    playableViewport.width = static_cast<float>(viewportLayout.playableWidth);
    playableViewport.height = static_cast<float>(viewportLayout.playableHeight);
    playableViewport.minDepth = 0.0f;
    playableViewport.maxDepth = 1.0f;

    VkRect2D playableScissor{};
    playableScissor.offset = {
        viewportLayout.playableOffsetX,
        viewportLayout.playableOffsetY,
    };
    playableScissor.extent = {
        static_cast<std::uint32_t>(viewportLayout.playableWidth),
        static_cast<std::uint32_t>(viewportLayout.playableHeight),
    };

    const bool skipLightPasses = (renderMode == RenderMode::StartMenu);

    {
        VkClearValue clearColor{};
        clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = lightRenderPass_;
        renderPassInfo.framebuffer = lightTarget_.framebuffer;
        renderPassInfo.renderArea = fullScissor;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(commandBuffer, 0, 1, &playableViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &playableScissor);

        if (!skipLightPasses && !particles.empty()) {
            VkBuffer vertexBuffers[] = {particleBuffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particleLightPipeline_);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdDraw(commandBuffer, 6, static_cast<uint32_t>(particles.size()), 0, 0);
        }
        vkCmdEndRenderPass(commandBuffer);
        transition_image_to_shader_read(commandBuffer, lightTarget_.image);
    }

    {
        VkClearValue clearColor{};
        clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = lightRenderPass_;
        renderPassInfo.framebuffer = asteroidLightTarget_.framebuffer;
        renderPassInfo.renderArea = fullScissor;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(commandBuffer, 0, 1, &playableViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &playableScissor);

        if (!skipLightPasses) {
            const std::size_t foregroundParticleCount = particles.size() - backgroundParticleCount_;
            if (foregroundParticleCount > 0) {
                VkBuffer vertexBuffers[] = {particleBuffer_};
                VkDeviceSize offsets[] = {0};
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particleLightPipeline_);
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdDraw(
                    commandBuffer,
                    6,
                    static_cast<uint32_t>(foregroundParticleCount),
                    0,
                    static_cast<uint32_t>(backgroundParticleCount_)
                );
            }
        }
        vkCmdEndRenderPass(commandBuffer);
        transition_image_to_shader_read(commandBuffer, asteroidLightTarget_.image);
    }

    {
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = sceneRenderPass_;
        renderPassInfo.framebuffer = sceneTarget_.framebuffer;
        renderPassInfo.renderArea = fullScissor;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(commandBuffer, 0, 1, &fullViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &fullScissor);

        if (starCount_ > 0) {
            VkBuffer vertexBuffers[] = {starBuffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, starPipeline_);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            StarPushConstants starPushConstants{};
            starPushConstants.elapsedTimeSeconds = elapsedTimeSeconds_;
            starPushConstants.backgroundHalfWidth = backgroundHalfExtents.x;
            starPushConstants.backgroundHalfHeight = backgroundHalfExtents.y;
            vkCmdPushConstants(
                commandBuffer,
                starPipelineLayout_,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(StarPushConstants),
                &starPushConstants
            );

            vkCmdDraw(commandBuffer, 4, static_cast<uint32_t>(starCount_), 0, 0);
        }

        vkCmdSetViewport(commandBuffer, 0, 1, &playableViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &playableScissor);

        if (renderMode != RenderMode::StartMenu && !particles.empty() && backgroundParticleCount_ > 0) {
            VkBuffer vertexBuffers[] = {particleBuffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particleScenePipeline_);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdDraw(commandBuffer, 6, static_cast<uint32_t>(backgroundParticleCount_), 0, 0);
        }

        if (!asteroids.empty() && asteroidVertexCount_ > 0) {
            VkBuffer vertexBuffers[] = {asteroidBuffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, asteroidPipeline_);
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                asteroidPipelineLayout_,
                0,
                1,
                &asteroidDescriptorSet_,
                0,
                nullptr
            );
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            AsteroidPushConstants pushConstants{};
            pushConstants.worldHalfExtents[0] = GameState::kWorldHalfWidth;
            pushConstants.worldHalfExtents[1] = GameState::kWorldHalfHeight;
            pushConstants.playableUvRect[0] = viewportLayout.playableUvMinX;
            pushConstants.playableUvRect[1] = viewportLayout.playableUvMinY;
            pushConstants.playableUvRect[2] = viewportLayout.playableUvMaxX;
            pushConstants.playableUvRect[3] = viewportLayout.playableUvMaxY;
            vkCmdPushConstants(
                commandBuffer,
                asteroidPipelineLayout_,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(AsteroidPushConstants),
                &pushConstants
            );

            vkCmdDraw(commandBuffer, static_cast<uint32_t>(asteroidVertexCount_), 1, 0, 0);
        }

        if (renderMode != RenderMode::StartMenu && !particles.empty()) {
            VkBuffer vertexBuffers[] = {particleBuffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particleScenePipeline_);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            const std::size_t foregroundParticleCount = particles.size() - backgroundParticleCount_;
            if (foregroundParticleCount > 0) {
                vkCmdDraw(
                    commandBuffer,
                    6,
                    static_cast<uint32_t>(foregroundParticleCount),
                    0,
                    static_cast<uint32_t>(backgroundParticleCount_)
                );
            }
        }

        {
            bool drawShip = hudState.shipVisible && renderMode != RenderMode::StartMenu;
            if (drawShip && hudState.shipFlashing) {
                drawShip = std::fmod(elapsedTimeSeconds_ * kInvulnerabilityFlashHz, 1.0f) < 0.5f;
            }
            if (drawShip) {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shipPipeline_);
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    shipPipelineLayout_,
                    0,
                    1,
                    &shipDescriptorSet_,
                    0,
                    nullptr
                );

                ShipPushConstants pushConstants{};
                pushConstants.shipPosition[0] = shipState.position.x;
                pushConstants.shipPosition[1] = shipState.position.y;
                pushConstants.shipHeading = shipState.headingRadians;
                pushConstants.worldHalfExtents[0] = GameState::kWorldHalfWidth;
                pushConstants.worldHalfExtents[1] = GameState::kWorldHalfHeight;

                vkCmdPushConstants(
                    commandBuffer,
                    shipPipelineLayout_,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(ShipPushConstants),
                    &pushConstants
                );

                vkCmdDraw(commandBuffer, 6, 1, 0, 0);
            }
        }

        if (renderMode != RenderMode::StartMenu && hudVertexCount_ > 0) {
            VkBuffer vertexBuffers[] = {hudBuffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipeline_);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdDraw(commandBuffer, static_cast<uint32_t>(hudVertexCount_), 1, 0, 0);
        }
        vkCmdEndRenderPass(commandBuffer);
        transition_image_to_shader_read(commandBuffer, sceneTarget_.image);
        transition_image_to_shader_read(commandBuffer, brightTarget_.image);
    }

    const VkDescriptorSet blurFirstPassInput = (renderMode == RenderMode::Paused)
        ? sceneBlurInputDescriptorSet_
        : blurDescriptorSets_[0];

    for (int passIndex = 0; passIndex < kBloomPassCount; ++passIndex) {
        const bool horizontal = (passIndex % 2) == 0;
        OffscreenTarget& outputTarget = horizontal ? blurTargets_[0] : blurTargets_[1];
        const VkDescriptorSet inputSet = (passIndex == 0)
            ? blurFirstPassInput
            : (horizontal ? blurDescriptorSets_[2] : blurDescriptorSets_[1]);

        VkClearValue clearColor{};
        clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = postProcessRenderPass_;
        renderPassInfo.framebuffer = outputTarget.framebuffer;
        renderPassInfo.renderArea = fullScissor;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(commandBuffer, 0, 1, &fullViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &fullScissor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipeline_);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipelineLayout_, 0, 1, &inputSet, 0, nullptr);

        BlurPushConstants blurPushConstants{};
        blurPushConstants.texelOffset[0] = horizontal ? 1.0f / static_cast<float>(swapchainExtent_.width) : 0.0f;
        blurPushConstants.texelOffset[1] = horizontal ? 0.0f : 1.0f / static_cast<float>(swapchainExtent_.height);
        vkCmdPushConstants(
            commandBuffer,
            blurPipelineLayout_,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(BlurPushConstants),
            &blurPushConstants
        );

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
        transition_image_to_shader_read(commandBuffer, outputTarget.image);
    }

    {
        VkClearValue clearColor{};
        clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = compositeRenderPass_;
        renderPassInfo.framebuffer = swapchainFramebuffers_[imageIndex];
        renderPassInfo.renderArea = fullScissor;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(commandBuffer, 0, 1, &fullViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &fullScissor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            compositePipelineLayout_,
            0,
            1,
            &compositeDescriptorSet_,
            0,
            nullptr
        );
        CompositePushConstants compositePC{};
        compositePC.playableUvMinX = viewportLayout.playableUvMinX;
        compositePC.playableUvMinY = viewportLayout.playableUvMinY;
        compositePC.playableUvMaxX = viewportLayout.playableUvMaxX;
        compositePC.playableUvMaxY = viewportLayout.playableUvMaxY;
        if (renderMode == RenderMode::Paused) {
            compositePC.sceneMix = 0.0f;
            compositePC.bloomStrength = 1.0f;
            compositePC.dimFactor = 0.45f;
        }
        vkCmdPushConstants(
            commandBuffer,
            compositePipelineLayout_,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(CompositePushConstants),
            &compositePC
        );
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        if (menuVertexCount_ > 0) {
            VkBuffer vertexBuffers[] = {menuBuffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdSetViewport(commandBuffer, 0, 1, &playableViewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &playableScissor);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, menuHudPipeline_);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdDraw(commandBuffer, static_cast<uint32_t>(menuVertexCount_), 1, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer.");
    }
}

VulkanRenderer::QueueFamilyIndices VulkanRenderer::find_queue_families(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    uint32_t index = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphicsFamily = index;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = index;
        }

        if (indices.is_complete()) {
            break;
        }

        ++index;
    }

    return indices;
}

VulkanRenderer::SwapchainSupportDetails VulkanRenderer::query_swapchain_support(VkPhysicalDevice device) const {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanRenderer::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return formats.at(0);
}

VkPresentModeKHR VulkanRenderer::choose_present_mode(const std::vector<VkPresentModeKHR>& presentModes) const {
    for (VkPresentModeKHR presentMode : presentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

    VkExtent2D actualExtent{};
    actualExtent.width = static_cast<uint32_t>(framebufferWidth);
    actualExtent.height = static_cast<uint32_t>(framebufferHeight);
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

bool VulkanRenderer::is_device_suitable(VkPhysicalDevice device) const {
    const QueueFamilyIndices indices = find_queue_families(device);
    if (!indices.is_complete()) {
        return false;
    }

    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(kRequiredDeviceExtensions.begin(), kRequiredDeviceExtensions.end());
    for (const VkExtensionProperties& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    if (!requiredExtensions.empty()) {
        return false;
    }

    const SwapchainSupportDetails swapchainSupport = query_swapchain_support(device);
    return !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
}

VkShaderModule VulkanRenderer::create_shader_module(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }

    return shaderModule;
}

std::vector<char> VulkanRenderer::read_binary_file(const std::string& path) const {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    const std::streamsize fileSize = file.tellg();
    std::vector<char> buffer(static_cast<size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}
