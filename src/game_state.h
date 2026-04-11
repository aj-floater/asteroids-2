#pragma once

#include "math.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct InputState {
    bool rotateLeft = false;
    bool rotateRight = false;
    float mouseTurnDelta = 0.0f;
    bool thrustForward = false;
    bool firePressed = false;
};

struct ColorRgb {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

struct ShipState {
    Vec2 position{};
    Vec2 velocity{};
    float headingRadians = 0.0f;
    float angularVelocityRadiansPerSecond = 0.0f;
};

struct AsteroidRenderData {
    static constexpr std::size_t kMaxVertexCount = 12;

    std::array<Vec2, kMaxVertexCount> localVertices{};
    std::size_t vertexCount = 0;
    Vec2 position{};
    float rotationRadians = 0.0f;
    float shadingSeed = 0.0f;
};

enum class AsteroidSizeClass : std::uint32_t {
    Large = 0,
    Medium = 1,
    Small = 2,
};

enum class ParticleShape : std::uint32_t {
    Square = 0,
    Rectangle = 1,
};

enum class ParticleDespawnBehavior : std::uint32_t {
    Wrap = 0,
    DestroyOffscreen = 1,
};

enum class ParticleRenderLayer : std::uint32_t {
    BehindAsteroids = 0,
    Front = 1,
};

struct EffectParticle {
    Vec2 position{};
    Vec2 velocity{};
    ColorRgb startColor{};
    ColorRgb midColor{};
    ColorRgb endColor{};
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 0.0f;
    float size = 1.0f;
    float alpha = 1.0f;
    float rotationRadians = 0.0f;
    float aspectRatio = 1.0f;
    float glowScale = 3.5f;
    float glowIntensity = 1.0f;
    float bloomIntensity = 1.0f;
    float lightIntensity = 1.0f;
    bool fadeAlphaOverLife = true;
    bool scaleDownOverLife = true;
    ParticleShape shape = ParticleShape::Square;
    ParticleDespawnBehavior despawnBehavior = ParticleDespawnBehavior::Wrap;
    ParticleRenderLayer renderLayer = ParticleRenderLayer::Front;
};

struct EffectParticleRenderData {
    Vec2 position{};
    ColorRgb color{};
    float alpha = 1.0f;
    float size = 1.0f;
    float rotationRadians = 0.0f;
    float aspectRatio = 1.0f;
    ParticleShape shape = ParticleShape::Square;
    float glowScale = 3.5f;
    float glowIntensity = 1.0f;
    float bloomIntensity = 1.0f;
    float lightIntensity = 1.0f;
    ParticleRenderLayer renderLayer = ParticleRenderLayer::Front;
};

struct LaserState {
    Vec2 position{};
    Vec2 previousPosition{};
    Vec2 velocity{};
    float headingRadians = 0.0f;
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 0.0f;
};

struct FlameEmitterConfig {
    std::uint32_t randomSeed = 0xA57E1D35u;
    std::size_t maxParticles = 1024;
    float particlesPerSecond = 180.0f;
    Vec2 localSpawnLeftPoint = {-3.0f, 2.0f};
    Vec2 localSpawnCenterPoint = {-2.0f, 0.0f};
    Vec2 localSpawnRightPoint = {-3.0f, -2.0f};
    float spawnCenterBiasExponent = 2.4f;
    float spreadAngleRadians = 0.28f;
    float minParticleSpeed = 28.0f;
    float maxParticleSpeed = 62.0f;
    float minLifetimeSeconds = 0.14f;
    float maxLifetimeSeconds = 0.4f;
    float minParticleSize = 0.12f;
    float maxParticleSize = 0.9f;
    float backgroundParticleChance = 0.38f;
    float glowScale = 3.5f;
    float glowIntensity = 1.0f;
    float bloomIntensity = 1.0f;
    float lightIntensity = 1.0f;
    std::array<float, 2> startWhiteRange = {0.88f, 1.0f};
    std::array<float, 2> midOrangeRedRange = {0.32f, 0.56f};
    std::array<float, 2> midOrangeGreenRange = {0.48f, 0.72f};
    std::array<float, 2> endRedRange = {0.72f, 1.0f};
    std::array<float, 2> endGreenRange = {0.04f, 0.18f};
};

struct LaserConfig {
    Vec2 localSpawnPoint = {4.0f, 0.0f};
    float speed = 170.0f;
    float inheritedVelocityFactor = 0.35f;
    float lifetimeSeconds = 8.0f;
    float length = 0.56f;
    float width = 0.24f;
    float glowScale = 8.0f;
    float glowIntensity = 5.5f;
    float bloomIntensity = 12.0f;
    float lightIntensity = 1.8f;
    ColorRgb color = {0.0f, 0.68f, 0.31f};
};

struct LaserImpactEffectConfig {
    std::size_t particlesPerHit = 7;
    float spreadRadians = 0.7f;
    float minSpeed = 40.0f;
    float maxSpeed = 135.0f;
    float minLifetimeSeconds = 0.05f;
    float maxLifetimeSeconds = 0.18f;
    float minSize = 0.16f;
    float maxSize = 0.42f;
    float widthRatioMin = 0.18f;
    float widthRatioMax = 0.42f;
    float glowScale = 5.5f;
    float glowIntensity = 2.4f;
    float bloomIntensity = 4.5f;
    float lightIntensity = 0.9f;
    ColorRgb startColor = {0.10f, 0.95f, 0.45f};
    ColorRgb midColor = {0.45f, 1.00f, 0.72f};
    ColorRgb endColor = {0.10f, 0.35f, 0.18f};
};

struct AsteroidBurstEffectConfig {
    std::size_t largeShardCount = 12;
    std::size_t mediumShardCount = 8;
    std::size_t smallShardCount = 5;
    std::size_t largeSmokeCount = 6;
    std::size_t mediumSmokeCount = 4;
    std::size_t smallSmokeCount = 3;
    std::size_t largeGlowCount = 3;
    std::size_t mediumGlowCount = 2;
    std::size_t smallGlowCount = 2;
    float minShardSpeed = 10.0f;
    float maxShardSpeed = 48.0f;
    float minShardLifetimeSeconds = 0.12f;
    float maxShardLifetimeSeconds = 0.34f;
    float minShardSizeFactor = 0.16f;
    float maxShardSizeFactor = 0.34f;
    float minSmokeSpeed = 6.0f;
    float maxSmokeSpeed = 22.0f;
    float minSmokeLifetimeSeconds = 0.18f;
    float maxSmokeLifetimeSeconds = 0.46f;
    float minSmokeSizeFactor = 0.22f;
    float maxSmokeSizeFactor = 0.48f;
    float minGlowSpeed = 18.0f;
    float maxGlowSpeed = 40.0f;
    float minGlowLifetimeSeconds = 0.12f;
    float maxGlowLifetimeSeconds = 0.28f;
    float minGlowSizeFactor = 0.18f;
    float maxGlowSizeFactor = 0.32f;
    ColorRgb shardStartColor = {0.32f, 0.34f, 0.36f};
    ColorRgb shardMidColor = {0.22f, 0.23f, 0.25f};
    ColorRgb shardEndColor = {0.06f, 0.06f, 0.07f};
    ColorRgb smokeStartColor = {0.46f, 0.38f, 0.30f};
    ColorRgb smokeMidColor = {0.22f, 0.18f, 0.16f};
    ColorRgb smokeEndColor = {0.04f, 0.04f, 0.04f};
    ColorRgb glowStartColor = {1.0f, 0.95f, 0.84f};
    ColorRgb glowMidColor = {1.0f, 0.58f, 0.18f};
    ColorRgb glowEndColor = {0.48f, 0.08f, 0.02f};
};

struct AsteroidFieldConfig {
    std::uint32_t randomSeed = 0xC41D5EEDu;
    std::size_t asteroidCount = 14;
    float minSize = 5.0f;
    float maxSize = 13.0f;
    float minSpeed = 6.0f;
    float maxSpeed = 19.0f;
    float minAngularSpeedRadiansPerSecond = -0.55f;
    float maxAngularSpeedRadiansPerSecond = 0.55f;
    std::size_t minVertexCount = 7;
    std::size_t maxVertexCount = 11;
    float radialJitter = 0.28f;
    float inwardHeadingSpreadRadians = 0.85f;
    float mediumScale = 0.58f;
    float smallScale = 0.58f;
    float childSpeedMultiplier = 1.35f;
    float childAngularSpeedMultiplier = 1.2f;
    float childSeparationAngleRadians = 0.45f;
    float childHeadingJitterRadians = 0.12f;
    float mediumChildRadialPerturbation = 0.09f;
    float smallChildRadialPerturbation = 0.14f;
    float childShapeBiasScale = 0.4f;
    float minChildShadingSeedJitter = 17.0f;
    float maxChildShadingSeedJitter = 79.0f;
};

struct AsteroidState {
    std::array<Vec2, AsteroidRenderData::kMaxVertexCount> localVertices{};
    std::size_t vertexCount = 0;
    Vec2 position{};
    Vec2 velocity{};
    float rotationRadians = 0.0f;
    float angularVelocityRadiansPerSecond = 0.0f;
    float outerRadius = 1.0f;
    float shadingSeed = 0.0f;
    AsteroidSizeClass sizeClass = AsteroidSizeClass::Large;
};

class GameState {
public:
    static constexpr float kWorldHalfWidth = 100.0f;
    static constexpr float kWorldHalfHeight = 75.0f;

    GameState();

    void update(float deltaTimeSeconds, const InputState& inputState);

    const ShipState& ship() const;
    bool shipColliding() const;
    std::optional<std::size_t> collidingAsteroidIndex() const;
    std::span<const EffectParticleRenderData> particles() const;
    std::span<const AsteroidRenderData> asteroids() const;

private:
    friend struct GameStateTestAccess;

    struct AsteroidBounds {
        float minX = 0.0f;
        float maxX = 0.0f;
        float minY = 0.0f;
        float maxY = 0.0f;
    };

    struct LaserImpactEvent {
        std::size_t asteroidIndex = 0;
        Vec2 impactPosition{};
        Vec2 impactNormal{};
        AsteroidSizeClass asteroidSizeClass = AsteroidSizeClass::Large;
        float distanceAlongLaser = 1.0f;
    };

    float random_range(float minValue, float maxValue);
    std::size_t random_index(std::size_t minValue, std::size_t maxValue);
    ColorRgb lerp_color(const ColorRgb& from, const ColorRgb& to, float t) const;
    ColorRgb particle_color_at_life(const EffectParticle& particle, float normalizedAge) const;
    EffectParticleRenderData render_data_from_effect_particle(const EffectParticle& particle) const;
    EffectParticleRenderData render_data_from_laser(const LaserState& laser) const;
    bool try_emit_effect_particle(const EffectParticle& particle);
    void initialize_asteroids();
    AsteroidState spawn_asteroid();
    std::vector<AsteroidState> split_asteroid(const AsteroidState& asteroid);
    std::optional<AsteroidSizeClass> next_size_class(AsteroidSizeClass sizeClass) const;
    float child_scale_for_size_class(AsteroidSizeClass sizeClass) const;
    float child_radial_perturbation_for_size_class(AsteroidSizeClass sizeClass) const;
    void apply_child_shape_variation(AsteroidState& asteroid);
    std::size_t shard_count_for_size(AsteroidSizeClass sizeClass) const;
    std::size_t smoke_count_for_size(AsteroidSizeClass sizeClass) const;
    std::size_t glow_count_for_size(AsteroidSizeClass sizeClass) const;
    AsteroidBounds asteroid_bounds(const AsteroidState& asteroid, Vec2 positionOffset = {}) const;
    void rebuild_asteroid_render_data();
    void rebuild_particle_render_data();
    void wrap_asteroid(AsteroidState& asteroid) const;
    std::optional<LaserImpactEvent> find_laser_impact(const LaserState& laser) const;
    void emit_laser_impact_particles(const LaserState& laser, const LaserImpactEvent& impactEvent);
    void emit_asteroid_destruction_particles(const AsteroidState& asteroid, const LaserImpactEvent& impactEvent);
    void resolve_laser_asteroid_hits();
    void update_ship_collision_state();
    void emit_thrust_particles(float deltaTimeSeconds);
    void emit_laser_shot();
    void update_lasers(float deltaTimeSeconds);
    void update_asteroids(float deltaTimeSeconds);
    void update_effect_particles(float deltaTimeSeconds);
    void wrap_position(Vec2& position) const;
    bool is_out_of_bounds(const Vec2& position) const;

    ShipState shipState_{};
    FlameEmitterConfig flameConfig_{};
    LaserConfig laserConfig_{};
    LaserImpactEffectConfig laserImpactEffectConfig_{};
    AsteroidBurstEffectConfig asteroidBurstEffectConfig_{};
    AsteroidFieldConfig asteroidConfig_{};
    std::vector<AsteroidState> asteroids_{};
    std::vector<AsteroidRenderData> asteroidRenderData_{};
    std::vector<EffectParticle> effectParticles_{};
    std::vector<LaserState> lasers_{};
    std::vector<EffectParticleRenderData> particleRenderData_{};
    float emissionAccumulator_ = 0.0f;
    std::uint32_t rngState_ = 0;
    bool shipColliding_ = false;
    std::optional<std::size_t> collidingAsteroidIndex_{};
};
