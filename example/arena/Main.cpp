// Third-person showcase of the renderer: a robot you drive around an arena.
//     kronk3d_arena [--backend software|opengl] [--aa none|fxaa|ssaa] [--time day|sunset|night]
//                   [--bounds] [--screenshot file.png [--seconds S]]
//                   [--scale 0.05-1] [--screenshot file.png [--seconds S]]
//                   [--flat] [--screenshot file.png [--seconds S]]
//
// ZQSD / WASD: move (relative to the camera)    Shift: run     Space: jump
// Mouse / arrows: camera     T: time of day    F: fog    V: anti-aliasing    H: HUD    B: collision boxes
// Escape: quit
//
// With --screenshot the character is driven by a short script, the frame is saved and the program quits
// (the software backend then needs no window at all).
#include "Hud.hpp"
#include "Level.hpp"
#include "Player.hpp"
#include <SFML/Window.hpp>
#include <kronk3d/Kronk3d.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#ifndef K3_EXAMPLE_ASSETS_DIR
    #define K3_EXAMPLE_ASSETS_DIR "example/assets"
#endif

namespace
{

    using k3::Math::Color;
    using k3::Math::Vector3f;

    constexpr float MOUSE_SENSITIVITY = 0.0028f;
    constexpr float KEY_TURN_SPEED = 1.8f;
    constexpr float CAMERA_DISTANCE = 6.5f;

    struct Options
    {
        k3::Backend      backend = k3::Backend::OpenGL;
        k3::AntiAliasing antiAliasing = k3::AntiAliasing::FXAA;
        std::size_t      timeOfDay = 0;
        bool             bounds = false;        // Draw the collision boxes (debug lines)
        float            renderScale = 1.f;
        bool             flatRobot = false;
        std::string      screenshot{};
        float            seconds = 3.f;
    };

    // Sun, ambient light, sky and fog for a time of day. The fog takes the sky's horizon color so that
    // distant walls melt into it.
    struct TimeOfDay
    {
        const char* name;
        Vector3f    sunDirection;
        Color       sunColor;
        float       sunIntensity;
        Color       ambient, ambientGround, zenith, horizon, ground;      // Ambient light: from above, from below
        float       fogDensity;
    };

    const std::array<TimeOfDay, 3> TIMES = {{
        {"day", {-0.45f, -0.8f, -0.4f}, {1.f, 0.95f, 0.85f, 1.f}, 1.8f, {0.22f, 0.27f, 0.36f, 1.f}, {0.1f, 0.09f, 0.07f, 1.f}, {0.08f, 0.22f, 0.65f, 1.f}, {0.55f, 0.68f, 0.85f, 1.f}, {0.18f, 0.2f, 0.2f, 1.f}, 0.018f},
        {"sunset", {-0.9f, -0.22f, -0.35f}, {1.f, 0.5f, 0.2f, 1.f}, 2.2f, {0.2f, 0.14f, 0.2f, 1.f}, {0.14f, 0.08f, 0.05f, 1.f}, {0.06f, 0.08f, 0.3f, 1.f}, {0.95f, 0.4f, 0.2f, 1.f}, {0.15f, 0.1f, 0.1f, 1.f}, 0.026f},
        {"night", {0.4f, -0.7f, 0.5f}, {0.5f, 0.6f, 1.f, 1.f}, 0.35f, {0.025f, 0.035f, 0.08f, 1.f}, {0.008f, 0.008f, 0.012f, 1.f}, {0.002f, 0.005f, 0.025f, 1.f}, {0.025f, 0.04f, 0.09f, 1.f}, {0.01f, 0.012f, 0.02f, 1.f}, 0.03f},
    }};

    const char* toString(k3::AntiAliasing mode)
    {
        switch (mode) {
            case k3::AntiAliasing::FXAA: return "FXAA";
            case k3::AntiAliasing::SSAA: return "SSAA";
            case k3::AntiAliasing::None: break;
        }
        return "no AA";
    }

    std::optional<Options> parseOptions(int argc, char** argv)
    {
        Options options;

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            const bool hasValue = i + 1 < argc;

            if (arg == "--backend" && hasValue) {
                const std::string_view value = argv[++i];
                if (value == "software")
                    options.backend = k3::Backend::Software;
                else if (value == "opengl")
                    options.backend = k3::Backend::OpenGL;
                else
                    return std::nullopt;
            } else if (arg == "--aa" && hasValue) {
                const std::string_view value = argv[++i];
                if (value == "none")
                    options.antiAliasing = k3::AntiAliasing::None;
                else if (value == "fxaa")
                    options.antiAliasing = k3::AntiAliasing::FXAA;
                else if (value == "ssaa")
                    options.antiAliasing = k3::AntiAliasing::SSAA;
                else
                    return std::nullopt;
            } else if (arg == "--time" && hasValue) {
                const std::string_view value = argv[++i];
                const auto found = std::find_if(TIMES.begin(), TIMES.end(), [&](const TimeOfDay& t) { return value == t.name; });

                if (found == TIMES.end())
                    return std::nullopt;
                options.timeOfDay = static_cast<std::size_t>(found - TIMES.begin());
            } else if (arg == "--bounds") {
                options.bounds = true;
            } else if (arg == "--scale" && hasValue) {
                options.renderScale = static_cast<float>(std::atof(argv[++i]));
            } else if (arg == "--flat") {
                options.flatRobot = true;
            } else if (arg == "--screenshot" && hasValue) {
                options.screenshot = argv[++i];
            } else if (arg == "--seconds" && hasValue) {
                options.seconds = std::max(0.1f, static_cast<float>(std::atof(argv[++i])));
            } else {
                return std::nullopt;
            }
        }
        return options;
    }

    // Everything that moves or changes: the character, the camera, the environment.
    class World
    {
        public:
            // The shadow maps are the heavy part on the CPU: the software backend gets smaller ones.
            World(Level level, Player player, std::size_t timeOfDay, bool lighterShadows)
                : m_level(std::move(level)), m_player(std::move(player)), m_lighterShadows(lighterShadows)
            {
                // A soft round sprite for the embers, tinted per particle.
                k3::Image glow(32, 32, Color::Transparent);

                for (std::uint32_t y = 0; y < 32; ++y) {
                    for (std::uint32_t x = 0; x < 32; ++x) {
                        const float dx = (static_cast<float>(x) - 15.5f) / 16.f, dy = (static_cast<float>(y) - 15.5f) / 16.f;
                        const float falloff = std::max(0.f, 1.f - std::sqrt(dx * dx + dy * dy));
                        std::uint8_t* texel = glow.texel(x, y);

                        texel[0] = texel[1] = texel[2] = 255;
                        texel[3] = static_cast<std::uint8_t>(255.f * falloff * falloff);
                    }
                }
                m_emberMaterial = std::make_shared<k3::Material>();
                m_emberMaterial->unlit = true;
                m_emberMaterial->castShadows = false;
                m_emberMaterial->alphaMode = k3::AlphaMode::Blend;
                m_emberMaterial->diffuseMap = std::make_shared<k3::Texture>(std::move(glow));

                setTimeOfDay(timeOfDay);
                m_player.update(0.f, {}, 0.f, m_level);
            }

            void update(float dt, const Input& input, float yawTurn, float pitchTurn)
            {
                m_clock += dt;
                m_yaw += yawTurn;
                m_pitch = std::clamp(m_pitch + pitchTurn, -1.25f, 0.35f);
                m_player.update(dt, input, m_yaw, m_level);
            }

            void nextTimeOfDay() { setTimeOfDay((m_time + 1) % TIMES.size()); }
            void toggleFog() { m_fog = !m_fog; }
            void toggleBounds() { m_bounds = !m_bounds; }
            void toggleFlatRobot() { m_player.toggleFlatShading(); }

            [[nodiscard]] float cameraYaw() const noexcept { return m_yaw; }
            [[nodiscard]] const Player& player() const noexcept { return m_player; }
            [[nodiscard]] const Level& level() const noexcept { return m_level; }
            [[nodiscard]] const TimeOfDay& timeOfDay() const noexcept { return TIMES[m_time]; }
            [[nodiscard]] bool fog() const noexcept { return m_fog; }
            [[nodiscard]] bool bounds() const noexcept { return m_bounds; }

            void draw(k3::IRasterizer& rasterizer)
            {
                const k3::Camera view = camera();

                rasterizer.beginFrame(view, environment());
                m_level.draw(rasterizer);
                m_player.draw(rasterizer);

                // The collision boxes (yellow, the glass one cyan) and the character's (green): debug lines.
                if (m_bounds) {
                    for (const Box& box : m_level.solids())
                        rasterizer.drawBounds({box.min, box.max}, box.glass ? Color{0.3f, 1.f, 1.f, 1.f} : Color{1.f, 0.9f, 0.2f, 1.f});

                    const Vector3f feet = m_player.position();

                    rasterizer.drawBounds({{feet.x - Player::RADIUS, feet.y, feet.z - Player::RADIUS}, {feet.x + Player::RADIUS, feet.y + Player::HEIGHT, feet.z + Player::RADIUS}}, Color{0.2f, 1.f, 0.3f, 1.f}, k3::Math::Matrix4::identity(), false);
                }
                drawEmbers(rasterizer, view);
            }

        private:
            // Third person: behind the character, pulled in when a block is in the way.
            [[nodiscard]] k3::Camera camera() const
            {
                k3::Camera camera;
                const Vector3f target = m_player.position() + Vector3f{0.f, 1.35f, 0.f};

                camera.yaw = m_yaw;
                camera.pitch = m_pitch;
                camera.nearPlane = 0.1f;
                camera.farPlane = 220.f;

                const Vector3f back = -1.f * camera.forward();
                float distance = CAMERA_DISTANCE;

                for (const Box& box : m_level.solids()) {
                    // Slab test of the ray target -> camera against the block, grown by the camera's margin.
                    constexpr float MARGIN = 0.3f;
                    float entry = 0.f, exit = distance;
                    const float origin[3] = {target.x, target.y, target.z}, direction[3] = {back.x, back.y, back.z};
                    const float low[3] = {box.min.x - MARGIN, box.min.y - MARGIN, box.min.z - MARGIN};
                    const float high[3] = {box.max.x + MARGIN, box.max.y + MARGIN, box.max.z + MARGIN};
                    bool hit = true;

                    for (int axis = 0; axis < 3 && hit; ++axis) {
                        if (std::abs(direction[axis]) < 1e-6f) {
                            hit = origin[axis] >= low[axis] && origin[axis] <= high[axis];
                            continue;
                        }
                        float t0 = (low[axis] - origin[axis]) / direction[axis], t1 = (high[axis] - origin[axis]) / direction[axis];

                        if (t0 > t1)
                            std::swap(t0, t1);
                        entry = std::max(entry, t0);
                        exit = std::min(exit, t1);
                        hit = entry <= exit;
                    }
                    if (hit && entry > 0.f)
                        distance = std::max(1.2f, entry);
                }
                camera.position = target + back * distance;
                camera.position.y = std::max(camera.position.y, 0.35f);
                return camera;
            }

            // Embers rising from every torch: a function of time only (no state), all drawn in one call.
            void drawEmbers(k3::IRasterizer& rasterizer, const k3::Camera& view)
            {
                constexpr int PER_TORCH = 28;
                constexpr float LIFE = 2.2f;

                m_embers.clear();
                for (std::size_t torch = 0; torch < m_level.torches().size(); ++torch) {
                    const Vector3f origin = m_level.torches()[torch].position - Vector3f{0.f, 0.15f, 0.f};

                    for (int i = 0; i < PER_TORCH; ++i) {
                        const float seed = static_cast<float>(torch * 97 + static_cast<std::size_t>(i) * 31);
                        const float age = std::fmod(m_clock + LIFE * static_cast<float>(i) / PER_TORCH + std::fmod(seed * 0.37f, 1.f), LIFE) / LIFE;    // 0 new, 1 gone
                        const float angle = seed * 2.399f;                                                                                         // Golden angle: spread evenly
                        const float drift = 0.35f * age;

                        m_embers.add({
                            .position = origin + Vector3f{std::cos(angle) * drift, 0.2f + 2.2f * age, std::sin(angle) * drift},
                            .size = {0.22f * (1.f - 0.6f * age), 0.22f * (1.f - 0.6f * age)},
                            .color = {1.f, 0.55f - 0.3f * age, 0.15f, 0.9f * (1.f - age)},
                            .rotation = angle,
                        });
                    }
                }
                m_embers.draw(rasterizer, view, m_emberMaterial);
            }

            [[nodiscard]] k3::Environment environment() const
            {
                const TimeOfDay& time = TIMES[m_time];
                k3::Environment environment;

                environment.ambient = time.ambient;
                environment.ambientGround = time.ambientGround;
                environment.skybox = m_sky;
                environment.clearColor = time.horizon;
                environment.lights = {k3::Light::directional(time.sunDirection, time.sunColor, time.sunIntensity)};
                environment.lights[0].castShadows = true;
                environment.lights.insert(environment.lights.end(), m_level.torches().begin(), m_level.torches().end());
                environment.shadows.resolution = m_lighterShadows ? 1024 : 2048;
                environment.shadows.cascades = m_lighterShadows ? 3 : 4;
                environment.shadows.maxDistance = 90.f;

                if (m_fog) {
                    environment.fog.color = time.horizon;
                    environment.fog.density = time.fogDensity;
                    environment.fog.heightFalloff = 0.12f;      // Thicker near the ground
                    environment.fog.baseHeight = 0.f;
                }
                return environment;
            }

            void setTimeOfDay(std::size_t index)
            {
                m_time = index;
                m_sky = k3::Skybox::gradient(TIMES[index].zenith, TIMES[index].horizon, TIMES[index].ground);
            }

            Level        m_level;
            Player       m_player;
            k3::Skybox   m_sky;
            std::size_t  m_time = 0;
            k3::BillboardBatch                 m_embers;
            std::shared_ptr<k3::Material>      m_emberMaterial;
            float                              m_clock = 0.f;        // Seconds since the start
            bool         m_lighterShadows;
            bool         m_fog = true;
            bool         m_bounds = false;
            float        m_yaw = 0.4f;
            float        m_pitch = -0.4f;
    };

    bool pressed(sf::Keyboard::Key key)
    {
        return sf::Keyboard::isKeyPressed(key);
    }

    Input readInput()
    {
        Input input;

        input.forward = (pressed(sf::Keyboard::W) || pressed(sf::Keyboard::Z) || pressed(sf::Keyboard::Up) ? 1.f : 0.f) - (pressed(sf::Keyboard::S) ? 1.f : 0.f);
        input.right = (pressed(sf::Keyboard::D) ? 1.f : 0.f) - (pressed(sf::Keyboard::A) || pressed(sf::Keyboard::Q) ? 1.f : 0.f);
        input.run = pressed(sf::Keyboard::LShift) || pressed(sf::Keyboard::RShift);
        input.jump = pressed(sf::Keyboard::Space);
        return input;
    }

    // The scripted run used for screenshots: forward, a slalom, a jump now and then.
    Input scriptedInput(float t)
    {
        Input input;

        input.forward = 1.f;
        input.right = 0.5f * std::sin(t * 1.3f);
        input.run = t > 1.f;
        input.jump = std::fmod(t, 1.8f) < 0.04f && t > 0.5f;
        return input;
    }

    // Renders the scripted run offscreen and saves the last frame.
    int takeScreenshot(const Options& options, World& world, k3::IRasterizer& rasterizer)
    {
        const Hud hud;
        constexpr float DT = 1.f / 60.f;
        int frame = 0;
        k3::FrameStats previous;        // The HUD shows the last finished frame: the culling is known once it ends

        for (float t = 0.f; t < options.seconds; t += DT, ++frame) {
            world.update(DT, scriptedInput(t), 0.004f, 0.f);
            world.draw(rasterizer);
            hud.draw(rasterizer, world.level(), world.player().position(), world.cameraYaw(), 60, previous);
            rasterizer.endFrame();
            previous = rasterizer.stats();
        }

        const bool saved = rasterizer.readPixels().savePNG(options.screenshot);

        std::cout << (saved ? "Saved " : "Cannot save ") << options.screenshot << " (" << frame << " frames, clip " << world.player().clip() << ", "
                  << rasterizer.stats().culledDraws << " of " << rasterizer.stats().drawCalls << " draws culled)" << std::endl;
        return saved ? 0 : 1;
    }

}

int main(int argc, char** argv)
{
    const auto options = parseOptions(argc, argv);

    if (!options) {
        std::cerr << "usage: " << argv[0] << " [--backend software|opengl] [--aa none|fxaa|ssaa] [--time day|sunset|night] [--bounds] [--screenshot file.png [--seconds S]]" << std::endl;
        std::cerr << "usage: " << argv[0] << " [--backend software|opengl] [--aa none|fxaa|ssaa] [--time day|sunset|night] [--scale 0.05-1] [--screenshot file.png [--seconds S]]" << std::endl;
        std::cerr << "usage: " << argv[0] << " [--backend software|opengl] [--aa none|fxaa|ssaa] [--time day|sunset|night] [--flat] [--screenshot file.png [--seconds S]]" << std::endl;
        return 2;
    }

    const std::filesystem::path assets = K3_EXAMPLE_ASSETS_DIR;
    auto player = Player::create(assets);

    if (!player)
        return 1;

    World world(Level(assets), std::move(*player), options->timeOfDay, options->backend == k3::Backend::Software);
    if (options->bounds)
        world.toggleBounds();
    if (options->flatRobot)
        world.toggleFlatRobot();
    const bool windowless = !options->screenshot.empty() && options->backend == k3::Backend::Software;
    constexpr unsigned WIDTH = 1280, HEIGHT = 720;

    // The window is only needed to show the frame, or to own the OpenGL context.
    std::unique_ptr<sf::Window> window;
    if (!windowless) {
        window = std::make_unique<sf::Window>(sf::VideoMode(WIDTH, HEIGHT), "kronk3d arena", sf::Style::Default, sf::ContextSettings(24, 0, 0, 3, 3, sf::ContextSettings::Core));
        window->setVerticalSyncEnabled(true);       // The GL backend would otherwise render as fast as it can
        window->setActive(true);
    }

    k3::GLLoader loader = [](const char* name) { return sf::Context::getFunction(name); };
    std::unique_ptr<k3::ImagePresenter> presenter;
    if (window && options->backend == k3::Backend::Software) {
        auto created = k3::ImagePresenter::create(loader);      // Shows the software backend's images

        if (!created) {
            std::cerr << "Cannot initialize rendering: " << created.error() << std::endl;
            return 1;
        }
        presenter = std::move(*created);
    }
    auto created = k3::createRasterizer(options->backend, {.width = WIDTH, .height = HEIGHT, .glLoader = loader, .antiAliasing = options->antiAliasing, .renderScale = options->renderScale});

    if (!created) {
        std::cerr << "Cannot initialize rendering: " << created.error() << std::endl;
        return 1;
    }
    auto rasterizer = std::move(*created);

    if (!options->screenshot.empty())
        return takeScreenshot(*options, world, *rasterizer);

    std::cout << "ZQSD/WASD move, Shift run, Space jump, mouse or arrows camera, T time of day, F fog, V anti-aliasing, H HUD, Escape quit\n"
              << "Rendering with " << rasterizer->name() << std::endl;

    window->setMouseCursorVisible(false);
    window->setMouseCursorGrabbed(true);

    const Hud hud;
    k3::FrameStats previous;        // The HUD shows the last finished frame: the culling is known once it ends
    bool showHud = true;
    bool focused = true;
    auto last = std::chrono::steady_clock::now();
    float fpsTime = 0.f;
    int fpsFrames = 0, fps = 0;
    const sf::Vector2i center(static_cast<int>(window->getSize().x / 2), static_cast<int>(window->getSize().y / 2));
    sf::Mouse::setPosition(center, *window);

    while (window->isOpen()) {
        sf::Event event;
        while (window->pollEvent(event)) {
            if (event.type == sf::Event::Closed || (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape))
                window->close();
            else if (event.type == sf::Event::Resized)
                rasterizer->resize(event.size.width, event.size.height);
            else if (event.type == sf::Event::LostFocus)
                focused = false;
            else if (event.type == sf::Event::GainedFocus)
                focused = true;
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::T)
                world.nextTimeOfDay();
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F)
                world.toggleFog();
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::G)
                world.toggleFlatRobot();
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::H)
                showHud = !showHud;
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::B)
                world.toggleBounds();
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::R) {
                // Full resolution, half, then big pixels.
                const float scale = rasterizer->renderScale();

                rasterizer->setRenderScale(scale > 0.9f ? 0.5f : scale > 0.4f ? 0.25f : 1.f);
            }
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::V) {
                const auto next = rasterizer->antiAliasing() == k3::AntiAliasing::None   ? k3::AntiAliasing::FXAA
                                  : rasterizer->antiAliasing() == k3::AntiAliasing::FXAA ? k3::AntiAliasing::SSAA
                                                                                         : k3::AntiAliasing::None;

                rasterizer->setAntiAliasing(next);
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::min(std::chrono::duration<float>(now - last).count(), 0.05f);
        last = now;

        float yawTurn = 0.f, pitchTurn = 0.f;
        if (focused) {
            const sf::Vector2i mouse = sf::Mouse::getPosition(*window) - center;

            yawTurn = static_cast<float>(mouse.x) * MOUSE_SENSITIVITY;
            pitchTurn = -static_cast<float>(mouse.y) * MOUSE_SENSITIVITY;
            sf::Mouse::setPosition(center, *window);
        }
        yawTurn += ((pressed(sf::Keyboard::Right) ? 1.f : 0.f) - (pressed(sf::Keyboard::Left) ? 1.f : 0.f)) * KEY_TURN_SPEED * dt;

        world.update(dt, focused ? readInput() : Input{}, yawTurn, pitchTurn);
        world.draw(*rasterizer);
        if (showHud)
            hud.draw(*rasterizer, world.level(), world.player().position(), world.cameraYaw(), fps, previous);
        rasterizer->endFrame();
        previous = rasterizer->stats();

        if (presenter)
            presenter->present(rasterizer->readPixels(), rasterizer->width(), rasterizer->height());
        window->display();

        fpsTime += dt;
        ++fpsFrames;
        if (fpsTime >= 0.5f) {
            fps = static_cast<int>(static_cast<float>(fpsFrames) / fpsTime + 0.5f);
            window->setTitle("kronk3d arena - " + std::string(rasterizer->name()) + ", " + toString(rasterizer->antiAliasing()) + ", x" + std::to_string(rasterizer->renderScale()).substr(0, 4) + ", " + world.timeOfDay().name
                             + (world.fog() ? ", fog" : ", no fog") + " - " + std::to_string(fps) + " fps, "
                             + std::to_string(rasterizer->stats().culledDraws) + "/" + std::to_string(rasterizer->stats().drawCalls) + " draws culled");
            fpsTime = 0.f;
            fpsFrames = 0;
        }
    }
    return 0;
}
