#pragma once

#include "game_state.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

class VulkanRenderer {
public:
    void initialize(GLFWwindow* window);
    void render(const ShipState& shipState, std::span<const FlameParticleRenderData> flameParticles);
    void handle_resize();
    void shutdown();

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool is_complete() const;
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct ShipPushConstants {
        float shipPosition[2];
        float shipHeading = 0.0f;
        float padding = 0.0f;
        float worldHalfExtents[2];
    };

    struct ParticleVertex {
        float position[2];
        float color[4];
        float size = 1.0f;

        static VkVertexInputBindingDescription binding_description();
        static std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions();
    };

    void create_instance();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_swapchain();
    void create_image_views();
    void create_render_pass();
    void create_ship_pipeline();
    void create_particle_pipeline();
    void create_framebuffers();
    void create_command_pool();
    void create_particle_buffer();
    void create_command_buffers();
    void create_sync_objects();

    void cleanup_swapchain();
    void recreate_swapchain();

    void update_particle_buffer(std::span<const FlameParticleRenderData> flameParticles);
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void record_command_buffer(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const ShipState& shipState,
        std::span<const FlameParticleRenderData> flameParticles
    );

    QueueFamilyIndices find_queue_families(VkPhysicalDevice device) const;
    SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& presentModes) const;
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    bool is_device_suitable(VkPhysicalDevice device) const;

    VkShaderModule create_shader_module(const std::vector<char>& code) const;
    std::vector<char> read_binary_file(const std::string& path) const;

    GLFWwindow* window_ = nullptr;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout shipPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline shipPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout particlePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline particlePipeline_ = VK_NULL_HANDLE;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    VkBuffer particleBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory particleBufferMemory_ = VK_NULL_HANDLE;
    void* particleBufferMapped_ = nullptr;
    std::size_t maxParticleCount_ = 512;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    std::vector<VkFence> imagesInFlight_;
    uint32_t currentFrame_ = 0;
    bool framebufferResized_ = false;

    static constexpr int kMaxFramesInFlight = 2;
};
