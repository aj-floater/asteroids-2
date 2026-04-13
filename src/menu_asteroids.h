#pragma once

#include "game_state.h"

#include <cstdint>
#include <span>
#include <vector>

class MenuAsteroidField {
public:
    MenuAsteroidField();

    void update(float deltaTimeSeconds);
    std::span<const AsteroidRenderData> render_data() const;

private:
    struct MenuAsteroid {
        std::array<Vec2, AsteroidRenderData::kMaxVertexCount> localVertices{};
        std::size_t vertexCount = 0;
        Vec2 position{};
        Vec2 velocity{};
        float rotationRadians = 0.0f;
        float angularVelocity = 0.0f;
        float outerRadius = 1.0f;
        float shadingSeed = 0.0f;
    };

    float random_range(float min, float max);
    std::size_t random_index(std::size_t min, std::size_t max);

    std::vector<MenuAsteroid> asteroids_;
    std::vector<AsteroidRenderData> renderData_;
    std::uint32_t rng_ = 0;
};
