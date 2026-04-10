#pragma once

#include "game_state.h"

#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

class VulkanRenderer {
public:
    void initialize(GLFWwindow* window);
    void render(
        const ShipState& shipState,
        std::span<const EffectParticleRenderData> particles,
        std::span<const AsteroidRenderData> asteroids,
        float deltaTimeSeconds
    );
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

    struct BlurPushConstants {
        float texelOffset[2];
    };

    struct StarPushConstants {
        float elapsedTimeSeconds = 0.0f;
    };

    struct ParticleVertex {
        float position[2];
        float color[4];
        float params0[4] = {1.0f, 0.0f, 1.0f, 0.0f};
        float params1[4] = {3.5f, 1.0f, 1.0f, 1.0f};

        static VkVertexInputBindingDescription binding_description();
        static std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions();
    };

    struct AsteroidVertex {
        float position[2];
        float localPosition[2];
        float variation[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float basis[4] = {1.0f, 0.0f, 1.0f, 0.0f};

        static VkVertexInputBindingDescription binding_description();
        static std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions();
    };

    struct StarVertex {
        float position[2];
        float color[4];
        float params[4] = {1.0f, 1.0f, 1.0f, 0.0f};

        static VkVertexInputBindingDescription binding_description();
        static std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions();
    };

    struct OffscreenTarget {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };

    void create_instance();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_swapchain();
    void create_swapchain_image_views();
    void create_render_passes();
    void create_descriptor_set_layouts();
    void create_samplers();
    void create_offscreen_targets();
    void create_pipelines();
    void create_framebuffers();
    void create_descriptor_pool();
    void create_descriptor_sets();
    void create_command_pool();
    void create_starfield();
    void create_star_buffer();
    void create_particle_buffer();
    void create_asteroid_buffer();
    void create_command_buffers();
    void create_sync_objects();

    void cleanup_swapchain();
    void recreate_swapchain();

    void update_descriptor_sets();
    void update_particle_buffer(std::span<const EffectParticleRenderData> particles);
    void update_asteroid_buffer(std::span<const AsteroidRenderData> asteroids);
    void create_offscreen_target(OffscreenTarget& target);
    void destroy_offscreen_target(OffscreenTarget& target);
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void transition_image_to_shader_read(VkCommandBuffer commandBuffer, VkImage image) const;
    void record_command_buffer(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const ShipState& shipState,
        std::span<const EffectParticleRenderData> particles,
        std::span<const AsteroidRenderData> asteroids
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

    OffscreenTarget lightTarget_{};
    OffscreenTarget sceneTarget_{};
    OffscreenTarget brightTarget_{};
    std::array<OffscreenTarget, 2> blurTargets_{};

    VkRenderPass lightRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass sceneRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass postProcessRenderPass_ = VK_NULL_HANDLE;
    VkRenderPass compositeRenderPass_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout shipDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout blurDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout compositeDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet shipDescriptorSet_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 3> blurDescriptorSets_{};
    VkDescriptorSet compositeDescriptorSet_ = VK_NULL_HANDLE;
    VkSampler linearSampler_ = VK_NULL_HANDLE;

    VkPipelineLayout shipPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline shipPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout starPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline starPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout asteroidPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline asteroidPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout particleScenePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline particleScenePipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout particleLightPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline particleLightPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout blurPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline blurPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout compositePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline compositePipeline_ = VK_NULL_HANDLE;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    VkBuffer starBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory starBufferMemory_ = VK_NULL_HANDLE;
    void* starBufferMapped_ = nullptr;
    std::size_t starCount_ = 0;
    std::size_t maxStarCount_ = 240;

    VkBuffer particleBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory particleBufferMemory_ = VK_NULL_HANDLE;
    void* particleBufferMapped_ = nullptr;
    std::size_t maxParticleCount_ = 1024;

    VkBuffer asteroidBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory asteroidBufferMemory_ = VK_NULL_HANDLE;
    void* asteroidBufferMapped_ = nullptr;
    std::size_t asteroidVertexCount_ = 0;
    std::size_t maxAsteroidVertexCount_ = 1024;

    float elapsedTimeSeconds_ = 0.0f;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    std::vector<VkFence> imagesInFlight_;
    uint32_t currentFrame_ = 0;
    bool framebufferResized_ = false;

    static constexpr int kMaxFramesInFlight = 2;
    static constexpr int kBloomPassCount = 4;
};
