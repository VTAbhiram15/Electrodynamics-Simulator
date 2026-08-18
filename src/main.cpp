#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <numbers>
#include <omp.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

constexpr double PI = 3.1415926535;
constexpr int ARROW_BLOCK_SIZE = 20;
constexpr int MAP_BLOCK_SIZE = ARROW_BLOCK_SIZE / 8;
constexpr float CHARGE_RADIUS = 12.f;
constexpr float CHARGE_RADIUS2 = (CHARGE_RADIUS * CHARGE_RADIUS); // squared CHARGE_RADIUS
constexpr float MAX_ARROW_LEN = 10.f;
constexpr float MAX_ARROW_BRIGHTNESS = 2.f;
constexpr float MAX_POT_BRIGHTNESS = 40.f;

// SFML 3: VideoMode size is accessed via .size.x and .size.y
sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
const int WIDTH = desktop.size.x;
const int HEIGHT = desktop.size.y;
const int ARROW_COLS = WIDTH / ARROW_BLOCK_SIZE;
const int ARROW_ROWS = HEIGHT / ARROW_BLOCK_SIZE;
const int MAP_COLS = WIDTH / MAP_BLOCK_SIZE;
const int MAP_ROWS = HEIGHT / MAP_BLOCK_SIZE;

typedef struct Charge {
    double q = 0.f;
    double q_real = 0.f;
    float x, y;
    float vx = 0, vy = 0;
    float ax = 0, ay = 0;
    float mass = 1.f;
} charge;

typedef struct Vector {
    float x, y;
    float Ex, Ey;
    float strength;
} arrow;

typedef struct pot {
    float x, y;
    float P;
} potential;

// INITIALIZING ALL VECTORS, SHAPES AND THE WINDOW
struct SimulationInit {
    sf::RenderWindow window;             // window
    sf::RectangleShape stem;            // arrow body
    sf::ConvexShape head;               // arrow head
    sf::CircleShape ch;                 // charge
    sf::CircleShape point;              // selected indicator
    std::vector<charge> charges;         // Charges on screen
    std::vector<arrow> FieldArrows;      // Electric Field arrows
    std::vector<potential> PotentialMap; // Electric Potential Map
    sf::VertexArray lines{sf::PrimitiveType::Lines}; // Equipotential lines
    // SFML 3: Triangles replace Quads (2 triangles = 6 vertices per quad)
    sf::VertexArray mapVertices{sf::PrimitiveType::Triangles, (size_t)MAP_ROWS * MAP_COLS * 6};
    std::vector<float> targetPotentials{-500, -200, -100, -50, -20, -10, -5, -2, -1, 0, 1, 2, 5, 10, 20, 50, 100, 200, 500};

    SimulationInit() :
        window(sf::VideoMode({static_cast<unsigned int>(WIDTH), static_cast<unsigned int>(HEIGHT)}), "Electrostatics", sf::Style::Default, sf::State::Windowed, sf::ContextSettings(0, 0, 0, 3, 1)),
        stem(sf::Vector2f(16.f, 2.f)),
        head(3),
        ch(CHARGE_RADIUS),
        point(2.f)
    {
        window.setFramerateLimit(60);
        
        head.setPoint(0, sf::Vector2f(5.f, 0.f));   // The tip
        head.setPoint(1, sf::Vector2f(0.f, -3.f));  // Top back corner  
        head.setPoint(2, sf::Vector2f(0.f, 3.f));   // Bottom back corner
        head.setOrigin({0.f, 0.f});
        
        ch.setOrigin({CHARGE_RADIUS, CHARGE_RADIUS});
        point.setOrigin({2.f, 2.f});
        point.setFillColor(sf::Color::White);
        
        FieldArrows.resize(ARROW_ROWS * ARROW_COLS);
        PotentialMap.resize(MAP_ROWS * MAP_COLS);
    }
};

sf::Color getPotentialColor(const float total_V) {
    float strength = std::clamp(total_V / MAX_POT_BRIGHTNESS, -1.0f, 1.0f);

    uint8_t negR = 205, negG = 120, negB = 120; // Cobalt Blue
    uint8_t neuR = 10,  neuG = 10,  neuB = 10;  // Dark Gray
    uint8_t posR = 120, posG = 120, posB = 205; // Crimson Red

    uint8_t finalR = neuR, finalG = neuG, finalB = neuB;

    if (strength > 0.0f) {
        float t = strength; 
        finalR = neuR + static_cast<uint8_t>((posR - neuR) * t);
        finalG = neuG + static_cast<uint8_t>((posG - neuG) * t);
        finalB = neuB + static_cast<uint8_t>((posB - neuB) * t);
    } 
    else if (strength < 0.0f) {
        float t = std::abs(strength); 
        finalR = neuR + static_cast<uint8_t>((negR - neuR) * t);
        finalG = neuG + static_cast<uint8_t>((negG - neuG) * t);
        finalB = neuB + static_cast<uint8_t>((negB - neuB) * t);
    }

    return sf::Color(finalR, finalG, finalB);
}

float getInterpolatedPos(float Va, float Vb, float Vt, float posA, float posB) {
    if (std::abs(Vb - Va) < 1e-5f) {
        return posA;
    }
    float t = (Vt - Va) / (Vb - Va);
    return posA + t * (posB - posA);
}

void drawCharges(SimulationInit& sim) {
    for (const auto& c : sim.charges) {
        sim.ch.setPosition({c.x, c.y});
        if (c.q < 0) {
            sim.ch.setFillColor(sf::Color::Red);
        }
        else if (c.q > 0) {
            sim.ch.setFillColor(sf::Color::Blue);
        }
        else {
            sim.ch.setFillColor(sf::Color::Green);
        }
        sim.window.draw(sim.ch);
    }
}

void drawFieldArrows(float& arrow_len, float& scale_factor, SimulationInit& sim) {
    int index = 0;
    float angle_rad = 0, angle_deg = 0;
    float headX, headY;
    for (int i = 0; i < ARROW_ROWS; i++) {
        for (int j = 0; j < ARROW_COLS; j++) {
            index = i * ARROW_COLS + j;
            angle_rad = std::atan2(sim.FieldArrows[index].Ey, sim.FieldArrows[index].Ex);
            angle_deg = angle_rad * (180.f / static_cast<float>(PI));

            float mag = std::sqrt((sim.FieldArrows[index].Ex * sim.FieldArrows[index].Ex) + (sim.FieldArrows[index].Ey * sim.FieldArrows[index].Ey));
            arrow_len = std::sqrt(mag) * scale_factor;
            if (arrow_len > MAX_ARROW_LEN) {
                arrow_len = MAX_ARROW_LEN;
            }
            if (arrow_len <= (-MAX_ARROW_LEN)) {
                arrow_len = -MAX_ARROW_LEN;
            }

            float strength = sim.FieldArrows[index].strength;
            uint8_t r, g, b;
            if (strength < 0.1f) {
                float t = strength / 0.1f;
                t = std::pow(t, 1.7f);
                r = static_cast<uint8_t>(15 + (t * (-15)));
                g = static_cast<uint8_t>(20 + (t * (230)));
                b = static_cast<uint8_t>(20 + (t * (92)));
            }
            else {
                float t = (strength - 0.1f) / 0.9f;
                r = static_cast<uint8_t>(0 + (t * (237)));
                g = static_cast<uint8_t>(215 + (t * (-214)));
                b = static_cast<uint8_t>(100 + (t * (-61)));
            }

            sim.stem.setPosition({sim.FieldArrows[index].x, sim.FieldArrows[index].y});
            sim.stem.setSize(sf::Vector2f(arrow_len, 2.f));
            sim.stem.setFillColor(sf::Color(r, g, b));
            sim.stem.setOrigin({arrow_len / 2.f, 1.f});
            sim.stem.setRotation(sf::degrees(angle_deg));

            headX = sim.FieldArrows[index].x + ((arrow_len / 2.f) * std::cos(angle_rad));
            headY = sim.FieldArrows[index].y + ((arrow_len / 2.f) * std::sin(angle_rad));
            sim.head.setPosition({headX, headY});
            sim.head.setFillColor(sf::Color(r, g, b));
            sim.head.setRotation(sf::degrees(angle_deg));

            sim.window.draw(sim.stem);
            sim.window.draw(sim.head);
        }
    }
}

void drawPotentialMap(SimulationInit& sim) {
    for (size_t i = 0; i < MAP_ROWS; i++) {
        for (size_t j = 0; j < MAP_COLS; j++) {
            int blockIdx = i * MAP_COLS + j;
            int vertexIndex = blockIdx * 6;
            float x = j * MAP_BLOCK_SIZE;
            float y = i * MAP_BLOCK_SIZE;
            float P = sim.PotentialMap[blockIdx].P;
            sf::Color blockColor = getPotentialColor(P);

            // Triangle 1
            sim.mapVertices[vertexIndex]     = sf::Vertex(sf::Vector2f(x, y), blockColor);
            sim.mapVertices[vertexIndex + 1] = sf::Vertex(sf::Vector2f(x + MAP_BLOCK_SIZE, y), blockColor);
            sim.mapVertices[vertexIndex + 2] = sf::Vertex(sf::Vector2f(x + MAP_BLOCK_SIZE, y + MAP_BLOCK_SIZE), blockColor);

            // Triangle 2
            sim.mapVertices[vertexIndex + 3] = sf::Vertex(sf::Vector2f(x, y), blockColor);
            sim.mapVertices[vertexIndex + 4] = sf::Vertex(sf::Vector2f(x + MAP_BLOCK_SIZE, y + MAP_BLOCK_SIZE), blockColor);
            sim.mapVertices[vertexIndex + 5] = sf::Vertex(sf::Vector2f(x, y + MAP_BLOCK_SIZE), blockColor);
        }
    }
    sim.window.draw(sim.mapVertices);
}

void drawEquipotentialLines(SimulationInit& sim) {
    sim.window.draw(sim.lines);
}

void updateEquipotentialLinesScreen(SimulationInit& sim) {
    sim.lines.clear();
    for (size_t i = 0; i < MAP_ROWS - 1; i++) {
        for (size_t j = 0; j < MAP_COLS - 1; j++) {
            float Vtl = sim.PotentialMap[i * MAP_COLS + j].P;
            float Vtr = sim.PotentialMap[i * MAP_COLS + (j + 1)].P;
            float Vbr = sim.PotentialMap[(i + 1) * MAP_COLS + (j + 1)].P;
            float Vbl = sim.PotentialMap[(i + 1) * MAP_COLS + j].P;

            float xl = j * MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE / 2.f);
            float xr = (j + 1) * MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE / 2.f);
            float yt = i * MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE / 2.f);
            float yb = (i + 1) * MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE / 2.f);
            
            for (const auto& Vt : sim.targetPotentials) {
                sf::Color baseColor = getPotentialColor(Vt);
                sf::Color equiPotentialLinesColor;
                equiPotentialLinesColor.r = static_cast<uint8_t>(std::min(255, baseColor.r + 60));
                equiPotentialLinesColor.g = static_cast<uint8_t>(std::min(255, baseColor.g + 60));
                equiPotentialLinesColor.b = static_cast<uint8_t>(std::min(255, baseColor.b + 60));
                equiPotentialLinesColor.a = 200;

                uint8_t state = 0 & 0x0F;
                if (Vtl >= Vt) state |= 8;
                if (Vtr >= Vt) state |= 4;
                if (Vbr >= Vt) state |= 2;
                if (Vbl >= Vt) state |= 1;

                switch (state) {
                    case 1: {
                        float iY = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        float iX = getInterpolatedPos(Vbr, Vbl, Vt, xr, xl);
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iY), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yb), equiPotentialLinesColor));
                        break;
                    }
                    case 2: {
                        float iY = getInterpolatedPos(Vtr, Vbr, Vt, yt, yb);
                        float iX = getInterpolatedPos(Vbl, Vbr, Vt, xl, xr);
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iY), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yb), equiPotentialLinesColor));
                        break;
                    }
                    case 3: {
                        float iYl = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        float iYr = getInterpolatedPos(Vtr, Vbr, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iYl), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iYr), equiPotentialLinesColor));
                        break;
                    }
                    case 4: {
                        float iX = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iY = getInterpolatedPos(Vbr, Vtr, Vt, yb, yt);
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iY), equiPotentialLinesColor));
                        break;
                    }
                    case 5: {
                        float iy_left = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        float ix_bottom = getInterpolatedPos(Vbl, Vbr, Vt, xl, xr);
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iy_left), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(ix_bottom, yb), equiPotentialLinesColor));

                        float ix_top = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iy_right = getInterpolatedPos(Vtr, Vbr, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(ix_top, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iy_right), equiPotentialLinesColor));
                        break;
                    }
                    case 6: { 
                        float iXt = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iXb = getInterpolatedPos(Vbl, Vbr, Vt, xl, xr);
                        sim.lines.append(sf::Vertex(sf::Vector2f(iXt, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(iXb, yb), equiPotentialLinesColor));
                        break;
                    }
                    case 7: { 
                        float iX = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iY = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iY), equiPotentialLinesColor));
                        break;
                    }
                    case 8: { 
                        float iX = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iY = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iY), equiPotentialLinesColor));
                        break;
                    }
                    case 9: { 
                        float iXt = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iXb = getInterpolatedPos(Vbl, Vbr, Vt, xl, xr);
                        sim.lines.append(sf::Vertex(sf::Vector2f(iXt, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(iXb, yb), equiPotentialLinesColor));
                        break;
                    }
                    case 10: { 
                        float ix_top = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iy_left = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(ix_top, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iy_left), equiPotentialLinesColor));

                        float ix_bottom = getInterpolatedPos(Vbl, Vbr, Vt, xl, xr);
                        float iy_right = getInterpolatedPos(Vtr, Vbr, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(ix_bottom, yb), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iy_right), equiPotentialLinesColor));
                        break;
                    }
                    case 11: {
                        float iX = getInterpolatedPos(Vtl, Vtr, Vt, xl, xr);
                        float iY = getInterpolatedPos(Vtr, Vbr, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yt), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iY), equiPotentialLinesColor));
                        break;
                    }
                    case 12: { 
                        float iYl = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        float iYr = getInterpolatedPos(Vtr, Vbr, Vt, yt, yb);
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iYl), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iYr), equiPotentialLinesColor));
                        break;
                    }
                    case 13: { 
                        float iY = getInterpolatedPos(Vtr, Vbr, Vt, yt, yb);
                        float iX = getInterpolatedPos(Vbl, Vbr, Vt, xl, xr);
                        sim.lines.append(sf::Vertex(sf::Vector2f(xr, iY), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yb), equiPotentialLinesColor));
                        break;
                    }
                    case 14: { 
                        float iY = getInterpolatedPos(Vtl, Vbl, Vt, yt, yb);
                        float iX = getInterpolatedPos(Vbl, Vbr, Vt, xl, xr);
                        sim.lines.append(sf::Vertex(sf::Vector2f(xl, iY), equiPotentialLinesColor));
                        sim.lines.append(sf::Vertex(sf::Vector2f(iX, yb), equiPotentialLinesColor));
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
}

void updateCharges(SimulationInit& sim) {
    size_t size = sim.charges.size();
    const double K = 1.1123471e-9;
    std::ranges::for_each(sim.charges, [K](charge& c) {
        c.ax = 0.f;
        c.ay = 0.f;
        c.q_real = K * c.q;
    });

    float Fx = 0;
    float Fy = 0;
    for (size_t i = 0; i < size; i++) {
        float dx, dy, r2, r;
        for (size_t j = i + 1; j < size; j++) {
            charge& c1 = sim.charges[i];
            charge& c2 = sim.charges[j];

            dx = c1.x - c2.x;
            dy = c1.y - c2.y;
            r2 = (dx * dx + dy * dy);
            r = std::sqrt(r2);

            if (r <= (2 * CHARGE_RADIUS)) {
                double avg = (c1.q + c2.q) / 2;
                double damp = -0.9f;
                float current_r = (r == 0.f) ? 0.1f : r;
                c1.q = avg;
                c1.vx = (damp) * c1.vx;
                c1.vy = (damp) * c1.vy;

                c2.q = avg;
                c2.vy = (damp) * c2.vy;
                c2.vx = (damp) * c2.vx;
                
                float overlap = (2 * CHARGE_RADIUS) - current_r;
                float nudgeX = (dx / current_r) * (overlap * 0.5f);
                float nudgeY = (dy / current_r) * (overlap * 0.5f);
                c1.x += nudgeX;
                c1.y += nudgeY;
                c2.x -= nudgeX;
                c2.y -= nudgeY;
            }
            else {
                float F = (8.988e15f * static_cast<float>(c1.q_real * c2.q_real)) / (r2);
                Fx = F * (dx / r);
                Fy = F * (dy / r);

                c1.ax += Fx / (c1.mass);
                c1.ay += Fy / (c1.mass);

                c2.ax -= Fx / (c2.mass);
                c2.ay -= Fy / (c2.mass);
            }
        }
    }

    float dt = 0.05f;
    for (auto& c : sim.charges) {
        c.vx += c.ax * dt;
        c.vy += c.ay * dt;

        c.x += c.vx * dt;
        c.y += c.vy * dt;
    }

    static unsigned short int e = 0;
    std::erase_if(sim.charges, [](const charge& c) {
        bool k = (c.x < -2000 || c.x > WIDTH + 2000) ||
                 (c.y < -2000 || c.y > HEIGHT + 2000);
        if (k) { e++; std::cout << "Erased " << e << " charges" << '\n'; }
        return k;
    });
}

void updateFieldArrows(SimulationInit& sim) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < ARROW_ROWS; i++) {
        for (int j = 0; j < ARROW_COLS; j++) {
            int index = i * ARROW_COLS + j;
            float total_Ex = 0;
            float total_Ey = 0;
            sim.FieldArrows[index].strength = 0.01f;
            sim.FieldArrows[index].x = j * ARROW_BLOCK_SIZE + (ARROW_BLOCK_SIZE / 2.f);
            sim.FieldArrows[index].y = i * ARROW_BLOCK_SIZE + (ARROW_BLOCK_SIZE / 2.f);
            for (const auto& c : sim.charges) {
                float dx = sim.FieldArrows[index].x - c.x;
                float dy = sim.FieldArrows[index].y - c.y;
                float r2 = (dx * dx) + (dy * dy);
                float r = std::sqrt(r2);
                if (r <= CHARGE_RADIUS) {
                    r = CHARGE_RADIUS;
                }
                float E = static_cast<float>(c.q) / (r * r);
                total_Ex += E * (dx / r);
                total_Ey += E * (dy / r);
            }
            sim.FieldArrows[index].Ey = total_Ey;
            sim.FieldArrows[index].Ex = total_Ex;
            float mag = std::sqrt(total_Ex * total_Ex + total_Ey * total_Ey);
            mag /= MAX_ARROW_BRIGHTNESS;
            if (mag > 1.f) mag = 1.f;
            if (mag < 0.f) mag = 0.f;
            sim.FieldArrows[index].strength = std::sqrt(mag);
        }
    }
}

void updatePotentialMapScreen(SimulationInit& sim) {
    #pragma omp parallel for collapse(2)
    for (size_t i = 0; i < MAP_ROWS; i++) {
        for (size_t j = 0; j < MAP_COLS; j++) {
            int index = i * MAP_COLS + j;
            float total_P = 0;
            sim.PotentialMap[index].x = j * MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE / 2.f);
            sim.PotentialMap[index].y = i * MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE / 2.f);
            for (const auto& c : sim.charges) {
                float dx = sim.PotentialMap[index].x - c.x;
                float dy = sim.PotentialMap[index].y - c.y;
                float r2 = (dx * dx) + (dy * dy);
                float r = std::sqrt(r2);
                if (r < CHARGE_RADIUS) {
                    r = CHARGE_RADIUS;
                }
                float P = static_cast<float>(c.q) / r;
                total_P += P;
            }
            sim.PotentialMap[index].P = total_P;
        }
    }
}

int main() {
    struct SimulationInit sim;

    int selectedIdx = -1;
    float arrow_len = 16.f;
    float scale_factor = 5.f;
    bool isSimRunning = false;
    bool updateElectricField = true;
    bool updatePotentialMap = false;
    bool updateEquipotentialLines = updatePotentialMap;
    bool displayElectricField = true;
    bool displayPotentialMap = false;
    bool displayEquipotentialLines = displayPotentialMap;

    while (sim.window.isOpen()) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(sim.window);
        
        // --- INPUT CHECKS (SFML 3 Event Polling) ---
        while (const std::optional event = sim.window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                sim.window.close();
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                float dx, dy, r2;
                float x = static_cast<float>(mousePressed->position.x);
                float y = static_cast<float>(mousePressed->position.y);

                if (mousePressed->button == sf::Mouse::Button::Left) {
                    bool found = false;
                    for (size_t i = 0; i < sim.charges.size(); i++) {
                        dx = x - sim.charges[i].x;
                        dy = y - sim.charges[i].y;
                        r2 = (dx * dx) + (dy * dy);
                        if (r2 < CHARGE_RADIUS2) {
                            selectedIdx = static_cast<int>(i);
                            found = true;
                            break;
                        }
                    }                
                    if (!found && !isSimRunning) {
                        selectedIdx = -1;
                        charge new_charge;
                        new_charge.x = x;
                        new_charge.y = y;
                        new_charge.q = 5000.f;
                        new_charge.mass = 1.f;
                        sim.charges.push_back(new_charge);
                        updateElectricField = updatePotentialMap = true;
                    }
                }
                if (mousePressed->button == sf::Mouse::Button::Right) {
                    bool found = false;
                    for (size_t i = 0; i < sim.charges.size(); i++) {
                        dx = x - sim.charges[i].x;
                        dy = y - sim.charges[i].y;
                        r2 = (dx * dx) + (dy * dy);
                        if (r2 < CHARGE_RADIUS2) {
                            selectedIdx = static_cast<int>(i);
                            found = true;
                            break;
                        }
                    }                        
                    if (!found && !isSimRunning) {
                        selectedIdx = -1;
                        charge new_charge;
                        new_charge.x = x;
                        new_charge.y = y;
                        new_charge.q = -5000.f;
                        new_charge.mass = 1.f;
                        sim.charges.push_back(new_charge);
                        updateElectricField = updatePotentialMap = true;
                    }                  
                }
            }

            if (const auto* wheelScrolled = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (selectedIdx != -1 && !isSimRunning) {
                    if (wheelScrolled->wheel == sf::Mouse::Wheel::Vertical) {
                        float delta = wheelScrolled->delta;
                        if (delta > 0.f) {
                            sim.charges[selectedIdx].q += delta * (500.f);
                        }
                        else if (delta < 0.f) {
                            sim.charges[selectedIdx].q -= (-delta) * (500.f);                      
                        }
                        updateElectricField = updatePotentialMap = true;
                    }
                }
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (selectedIdx != -1) {
                    if (keyPressed->code == sf::Keyboard::Key::M) {
                        sim.charges[selectedIdx].mass += 2.f;
                        updateElectricField = updatePotentialMap = true;
                        std::cout << "mass = " << sim.charges[selectedIdx].mass << '\n';            
                    }
                    if (keyPressed->code == sf::Keyboard::Key::N) {
                        if (sim.charges[selectedIdx].mass > 2.0f) sim.charges[selectedIdx].mass -= 2.f;
                        updateElectricField = updatePotentialMap = true;
                        std::cout << "mass = " << sim.charges[selectedIdx].mass << '\n';
                    }
                    if (keyPressed->code == sf::Keyboard::Key::Delete) {
                        sim.charges.erase(sim.charges.begin() + selectedIdx);
                        updateElectricField = updatePotentialMap = true;
                        selectedIdx = -1;
                    }
                }

                // Global key commands
                if (keyPressed->code == sf::Keyboard::Key::Space) {
                    isSimRunning = !isSimRunning;
                    selectedIdx = -1;
                    updateElectricField = updatePotentialMap = true;
                }
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    selectedIdx = -1;
                }
                if (keyPressed->code == sf::Keyboard::Key::E) {
                    displayElectricField = updateElectricField = true;
                    displayPotentialMap = false;
                }
                if (keyPressed->code == sf::Keyboard::Key::P) {
                    displayPotentialMap = updatePotentialMap = true;
                    displayElectricField = false;
                }
                if (keyPressed->code == sf::Keyboard::Key::R) {
                    selectedIdx = -1;
                    isSimRunning = false;
                    updateElectricField = updatePotentialMap = true;
                    sim.charges.clear();
                    std::cout << "Simulation Cleared." << '\n';
                }
            }
        }

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && selectedIdx != -1 && !isSimRunning) {
            sim.charges[selectedIdx].x = static_cast<float>(mousePos.x);
            sim.charges[selectedIdx].y = static_cast<float>(mousePos.y);
            if (displayElectricField) updateElectricField = true;
            if (displayPotentialMap) updatePotentialMap = true;
        }

        displayEquipotentialLines = displayPotentialMap;
        updateEquipotentialLines = updatePotentialMap;

        // ---- RENDER ----

        sim.window.clear(sf::Color::Black);

        if (updateElectricField) {
            updateFieldArrows(sim);
            updateElectricField = false;
        }
        if (updatePotentialMap || updateEquipotentialLines) {
            updatePotentialMapScreen(sim);
            updatePotentialMap = false;
        }
        if (updateEquipotentialLines) {
            updateEquipotentialLinesScreen(sim);
            updateEquipotentialLines = false;
        } 
        if (isSimRunning) {
            updateCharges(sim);
            if (displayElectricField) updateElectricField = true;
            if (displayPotentialMap) updatePotentialMap = true;
            if (displayEquipotentialLines) updateEquipotentialLines = true;
        }

        if (displayElectricField) drawFieldArrows(arrow_len, scale_factor, sim);
        if (displayPotentialMap) drawPotentialMap(sim);
        if (displayEquipotentialLines) {
            drawEquipotentialLines(sim);
        }
        drawCharges(sim);
        if (!isSimRunning && selectedIdx != -1) {
            sim.point.setPosition({sim.charges[selectedIdx].x, sim.charges[selectedIdx].y});
            sim.window.draw(sim.point);
        }

        sim.window.display();

        if (sim.charges.empty() && isSimRunning) {
            selectedIdx = -1;
            isSimRunning = false;
            std::cout << "All charges left the borders. Simulation Cleared." << '\n';
        }
    }
    return 0;
}