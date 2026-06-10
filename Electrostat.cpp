#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <numbers>

sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
#define PI 3.1415926535
const int ARROW_BLOCK_SIZE = 35;
const int MAP_BLOCK_SIZE = ARROW_BLOCK_SIZE/2;
const int WIDTH = desktop.width;
const int HEIGHT = desktop.height;
const int ARROW_COLS = WIDTH/ARROW_BLOCK_SIZE;
const int ARROW_ROWS = HEIGHT/ARROW_BLOCK_SIZE;
const int MAP_COLS = WIDTH/MAP_BLOCK_SIZE;
const int MAP_ROWS = HEIGHT/MAP_BLOCK_SIZE;
const int CHARGE_RADIUS = 12.f;
const int CHARGE_RADIUS2 = (CHARGE_RADIUS * CHARGE_RADIUS); //squared CHARGE_RADIUS
const int MAX_ARROW_LEN = 10.f;
const float MAX_ARROW_BRIGHTNESS = 2.f;
const float MAX_POT_BRIGHTNESS = 40.f;

typedef struct Charge{
    double q=0.f;
    double q_real=0.f;
    float x, y;
    float vx=0, vy=0;
    float ax=0, ay=0;
    float mass=1.f;
} charge;

typedef struct Vector{
    float x, y;
    float Ex, Ey;
    float strength;
} arrow;

typedef struct pot{
    float x, y;
    float P;
} potential;

//INITIALIZING ALL VECTORS, SHAPES AND THE WINDOW
struct SimulationInit{
    sf::RenderWindow window;
    sf::RectangleShape stem;
    sf::ConvexShape head;
    sf::RectangleShape mapBlock;
    sf::CircleShape ch;
    sf::CircleShape point;
    std::vector<charge> charges;
    std::vector<arrow> FieldArrows;
    std::vector<potential> PotentialMap;

    SimulationInit() :
        window(sf::VideoMode(WIDTH, HEIGHT), "Electrostatics", sf::Style::Default, sf::ContextSettings(0,0,0,3,1)),
        stem(sf::Vector2f(16.f, 2.f)),
        head(3),
        ch(CHARGE_RADIUS),
        point(2.f),
        mapBlock(sf::Vector2f((WIDTH*1.f/MAP_COLS), (HEIGHT*1.f/MAP_ROWS)))
    {
        window.setFramerateLimit(60);

        mapBlock.setOrigin(WIDTH/(2*MAP_COLS), HEIGHT/(2*MAP_ROWS));

        head.setPoint(0, sf::Vector2f(5.f, 0.f));   // The tip
        head.setPoint(1, sf::Vector2f(0.f, -3.f));  // Top back corner  
        head.setPoint(2, sf::Vector2f(0.f, 3.f));   // Bottom back corner
        head.setOrigin(0.f, 0.f);

        ch.setOrigin(CHARGE_RADIUS, CHARGE_RADIUS);
        point.setOrigin(2.f, 2.f);
        point.setFillColor(sf::Color::White);

        FieldArrows.resize(ARROW_ROWS*ARROW_COLS);
        PotentialMap.resize(MAP_ROWS*MAP_COLS);
    }
};


void drawCharges(SimulationInit& sim) {
    for(const auto& c: sim.charges){
        sim.ch.setPosition(c.x, c.y);
        if(c.q<0){
            sim.ch.setFillColor(sf::Color::Red);
        }
        else if(c.q>0){
            sim.ch.setFillColor(sf::Color::Blue);
        }
        else{
            sim.ch.setFillColor(sf::Color::Green);
        }
        sim.window.draw(sim.ch);
    }
}
void drawFieldArrows(float& arrow_len, float& scale_factor, SimulationInit& sim){
    int index=0;
    float angle_rad=0, angle_deg=0;
    float headX, headY;
    for(int i=0; i<ARROW_ROWS; i++){
        for(int j=0; j<ARROW_COLS; j++){
            index = i*ARROW_COLS + j;
            angle_rad = (std::atan2(sim.FieldArrows[index].Ey, sim.FieldArrows[index].Ex));
            angle_deg = angle_rad * (180/PI);

            float mag = std::sqrt((sim.FieldArrows[index].Ex * sim.FieldArrows[index].Ex) + (sim.FieldArrows[index].Ey * sim.FieldArrows[index].Ey));
            arrow_len = std::sqrt(mag) * scale_factor;
            if(arrow_len > MAX_ARROW_LEN){
                arrow_len = MAX_ARROW_LEN;
            }
            if(arrow_len <= (-MAX_ARROW_LEN)){
                arrow_len = -MAX_ARROW_LEN;
            }

            float strength = sim.FieldArrows[index].strength;
            sf::Uint8 r, g, b;
            if(strength < 0.1f){
                float t = strength/0.1f;
                t = std::pow(t, 1.7f);
                r = static_cast<sf::Uint8>(15 + (t * (-15)));
                g = static_cast<sf::Uint8>(20 + (t * (230)));
                b = static_cast<sf::Uint8>(20 + (t * (92)));
            }
            else{
                float t = (strength - 0.1f)/0.9f;
                r = static_cast<sf::Uint8>(0 + (t * (237)));
                g = static_cast<sf::Uint8>(255 + (t * (-214)));
                b = static_cast<sf::Uint8>(117 + (t * (-61)));
            }
            sim.stem.setPosition(sim.FieldArrows[index].x, sim.FieldArrows[index].y);
            sim.stem.setSize(sf::Vector2f(arrow_len, 2.f));
            sim.stem.setFillColor(sf::Color(r, g, b));
            sim.stem.setOrigin(arrow_len/2.f, 1.f);
            sim.stem.setRotation(angle_deg);

            headX = sim.FieldArrows[index].x + ((arrow_len/2) * std::cos(angle_rad));
            headY = sim.FieldArrows[index].y + ((arrow_len/2) * std::sin(angle_rad));
            sim.head.setPosition(headX, headY);
            sim.head.setFillColor(sf::Color(r, g, b));
            sim.head.setRotation(angle_deg);

            sim.window.draw(sim.stem);
            sim.window.draw(sim.head);
        }
    }
}
sf::Color getPotentialColor(const float total_V) {
    // 1. Calculate the raw ratio and clamp it strictly between -1.0 and 1.0
    float ratio = total_V / MAX_POT_BRIGHTNESS;
    ratio = std::max(-1.0f, std::min(1.0f, ratio));

    float sign = (ratio >= 0.0f) ? 1.0f : -1.0f;
    float strength = sign * std::sqrt(std::abs(ratio));

    // Negative (#2535AA) -> Neutral (#4B4A4F) -> Positive (#C32727)
    sf::Uint8 negR = 205,  negG = 120,  negB = 120;  // Cobalt Blue
    sf::Uint8 neuR = 10,  neuG = 10,  neuB = 10;   // Dark Gray
    sf::Uint8 posR = 120, posG = 120,  posB = 205;   // Crimson Red

    sf::Uint8 finalR = neuR, finalG = neuG, finalB = neuB;

    if (strength > 0.0f) {
        // Positive Branch: Blend from Neutral (t=0) to Positive (t=1)
        float t = strength; 
        finalR = neuR + static_cast<sf::Uint8>((posR - neuR) * t);
        finalG = neuG + static_cast<sf::Uint8>((posG - neuG) * t);
        finalB = neuB + static_cast<sf::Uint8>((posB - neuB) * t);
    } 
    else if (strength < 0.0f) {
        // Negative Branch: Blend from Neutral (t=0) to Negative (t=1)
        float t = std::abs(strength); 
        finalR = neuR + static_cast<sf::Uint8>((negR - neuR) * t);
        finalG = neuG + static_cast<sf::Uint8>((negG - neuG) * t);
        finalB = neuB + static_cast<sf::Uint8>((negB - neuB) * t);
    }

    return sf::Color(finalR, finalG, finalB);
}
void drawPotentialMap(SimulationInit& sim){
    float x,  y, P;
    for(size_t i=0; i<MAP_ROWS; i++){
        for(size_t j=0; j<MAP_COLS; j++){
            int index = i*MAP_COLS + j;
            x = sim.PotentialMap[index].x;
            y = sim.PotentialMap[index].y;
            P = sim.PotentialMap[index].P;
            sf::Color blockColor = getPotentialColor(P);

            sim.mapBlock.setPosition(x, y);
            sim.mapBlock.setFillColor(blockColor);
            sim.window.draw(sim.mapBlock);
        }
    }
}

void updateCharges(SimulationInit& sim){
    // SL = 0.01;
    // SE = 1e5;
    // 1/ke = 1.1123471e-10;
    // double K = 1/ke * (SL*SL) * SE;

    size_t size = sim.charges.size();
    const double K = 1.1123471e-9;
    for(auto& c: sim.charges){
        c.ax = 0;
        c.ay = 0;
        c.q_real = K * c.q;
    }

    float Fx = 0;
    float Fy = 0;
    for(size_t i=0; i<size; i++){
        float dx, dy, r2, r;
        for(size_t j=i+1; j<size; j++){
            charge& c1 = sim.charges[i];
            charge& c2 = sim.charges[j];

            dx = c1.x - c2.x;
            dy = c1.y - c2.y;
            r2 = (dx*dx + dy*dy);
            r = std::sqrt(r2);

            if(r <= (2*CHARGE_RADIUS)){
                double avg = (c1.q + c2.q)/2;
                double damp = -0.9f;
                float current_r = (r==0.f)? 0.1f : r;
                c1.q = avg;
                c1.vx = (damp)*c1.vx;
                c1.vy = (damp)*c1.vy;

                c2.q = avg;
                c2.vy = (damp)*c2.vy;
                c2.vx = (damp)*c2.vx;
                
                float overlap = (2 * CHARGE_RADIUS) - current_r;
                float nudgeX = (dx / current_r) * (overlap * 0.5f);
                float nudgeY = (dy / current_r) * (overlap * 0.5f);
                c1.x += nudgeX;
                c1.y += nudgeY;
                c2.x -= nudgeX;
                c2.y -= nudgeY;
            }
            else{
                float F = (8.988e15 * c1.q_real * c2.q_real)/(r2);
                Fx = F * (dx/r);
                Fy = F * (dy/r);

                c1.ax += Fx/(c1.mass);
                c1.ay += Fy/(c1.mass);

                c2.ax -= Fx/(c2.mass);
                c2.ay -= Fy/(c2.mass);
            }
        }
    }

    float dt = 0.1f;
    for(auto& c: sim.charges){
        c.vx += c.ax * dt;
        c.vy += c.ay * dt;

        c.x += c.vx * dt;
        c.y += c.vy * dt;
    }

    //erasing out-of bounds charges (border at WIDTH+2000, HEIGHT+2000)
    static unsigned short int e = 0;
    std::erase_if(sim.charges, [](const charge& c) {
        bool k = (c.x < -2000 || c.x > WIDTH+2000) ||
                 (c.y < -2000 || c.y > HEIGHT+2000);
        if(k){ e++; std::cout << "Erased " << e << " charges" << std::endl; }
        return k;
    });
}
void updateFieldArrows(SimulationInit& sim){
    for(int i=0; i<ARROW_ROWS; i++){
        for(int j=0; j<ARROW_COLS; j++){
            int index = i*ARROW_COLS + j;
            float total_Ex=0;
            float total_Ey=0;
            sim.FieldArrows[index].strength = 0.01f;
            sim.FieldArrows[index].x = j*ARROW_BLOCK_SIZE + (ARROW_BLOCK_SIZE/2);
            sim.FieldArrows[index].y = i*ARROW_BLOCK_SIZE + (ARROW_BLOCK_SIZE/2);
            for(const auto& c: sim.charges){
                
                float dx = sim.FieldArrows[index].x - c.x;
                float dy = sim.FieldArrows[index].y - c.y;
                float r2 = (dx*dx) + (dy*dy);
                float r = std::sqrt(r2);
                if(r<=CHARGE_RADIUS){
                    r = CHARGE_RADIUS;
                }
                float E = (c.q)/r2;
                total_Ex += E * (dx/r);
                total_Ey += E * (dy/r);
            }
            sim.FieldArrows[index].Ey = total_Ey;
            sim.FieldArrows[index].Ex = total_Ex;
            float mag = std::sqrt(total_Ex*total_Ex + total_Ey*total_Ey);
            mag /= MAX_ARROW_BRIGHTNESS;
            if(mag>1.f) mag=1.f;
            if(mag<0.f) mag = 0.f;
            sim.FieldArrows[index].strength = std::sqrt(mag);
        }
    }
}
void updatePotentialMap(SimulationInit& sim){
    for(size_t i=0; i<MAP_ROWS; i++){
        for(size_t j=0; j<MAP_COLS; j++){
            int index = i*MAP_COLS + j;
            float total_P=0;
            sim.PotentialMap[index].x = j*MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE/2);
            sim.PotentialMap[index].y = i*MAP_BLOCK_SIZE + (MAP_BLOCK_SIZE/2);
            for(const auto& c: sim.charges){
                float dx = sim.PotentialMap[index].x - c.x;
                float dy = sim.PotentialMap[index].y - c.y;
                float r2 = (dx*dx) + (dy*dy);
                float r = std::sqrt(r2);
                if(r < CHARGE_RADIUS){
                    r = CHARGE_RADIUS;
                }
                float P = c.q/r;
                total_P += P;
            }
            sim.PotentialMap[index].P = total_P;
        }
    }
}

int main(){
    struct SimulationInit sim;

    int selectedIdx = -1;
    float arrow_len = 16.f;
    float scale_factor = 5.f;
    bool isSimRunning = false;
    bool displayElectricField = true;
    bool displayPotentialMap = false;
    
    while(sim.window.isOpen()){
        sf::Vector2i mousePos = sf::Mouse::getPosition(sim.window);
        sf::Event event;
        //---INPUT CHECKS---
        while(sim.window.pollEvent(event)){
            if(event.type == sf::Event::Closed){
                sim.window.close();
            }

            if(event.type == sf::Event::MouseButtonPressed){
                float dx, dy, r2;
                float x = event.mouseButton.x;
                float y = event.mouseButton.y;
                if(event.mouseButton.button == sf::Mouse::Left){
                    bool found=false;
                    for(int i=0; i<sim.charges.size(); i++){
                        dx = x - sim.charges[i].x;
                        dy = y - sim.charges[i].y;
                        r2 = (dx*dx) + (dy*dy);
                        if(r2 < CHARGE_RADIUS2){
                            selectedIdx = i;
                            found = true;
                            break;
                        }
                    }                
                    if(!found && !isSimRunning){
                        selectedIdx = -1;
                        charge new_charge;
                        new_charge.x = x;
                        new_charge.y = y;
                        new_charge.q = 5000.f;
                        new_charge.mass = 1.f;
                        sim.charges.push_back(new_charge);
                    }

                }
                if(event.mouseButton.button == sf::Mouse::Right){
                    bool found=false;
                    for(int i=0; i<sim.charges.size(); i++){
                        dx = x - sim.charges[i].x;
                        dy = y - sim.charges[i].y;
                        r2 = (dx*dx) + (dy*dy);
                        if(r2 < CHARGE_RADIUS2){
                            selectedIdx = i;
                            found = true;
                            break;
                        }
                    }                    
                    if(!found && !isSimRunning){
                        selectedIdx = -1;
                        charge new_charge;
                        new_charge.x = x;
                        new_charge.y = y;
                        new_charge.q = -5000.f;
                        new_charge.mass = 1.f;
                        sim.charges.push_back(new_charge);
                    }                  
                }

            }
            if(event.type == sf::Event::MouseWheelScrolled && selectedIdx != -1){
                if(event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel){
                    float delta = event.mouseWheelScroll.delta;
                    if(delta > 0.f){
                        sim.charges[selectedIdx].q += delta*(500.f);
                    }
                    else if(delta < 0.f){
                        sim.charges[selectedIdx].q -= (-delta)*(500.f);                      
                    }
                }
            }
            if(event.type == sf::Event::KeyPressed && selectedIdx != -1){
                if(event.key.code == sf::Keyboard::M){
                    sim.charges[selectedIdx].mass += 2.f;
                    std::cout << "mass = " << sim.charges[selectedIdx].mass << std::endl;            
                }
                if(event.key.code == sf::Keyboard::N){
                    if(sim.charges[selectedIdx].mass > 0) sim.charges[selectedIdx].mass -= 2.f;
                    else if(sim.charges[selectedIdx].mass <=0) sim.charges[selectedIdx].mass = 0.f;
                    std::cout << "mass = " << sim.charges[selectedIdx].mass << std::endl;
                }
                if(event.key.code == sf::Keyboard::Delete){
                    sim.charges.erase(sim.charges.begin() + selectedIdx);
                    selectedIdx = -1;
                }
            }
        
            if(event.type == sf::Event::KeyPressed) {
                // These keys work even if the screen is empty
                if(event.key.code == sf::Keyboard::Space){
                    isSimRunning = !isSimRunning;
                }
                if(event.key.code == sf::Keyboard::Escape){
                    selectedIdx = -1;
                }
                if(event.key.code == sf::Keyboard::E){
                    displayElectricField = true;
                    displayPotentialMap = false;
                }
                if(event.key.code == sf::Keyboard::P){
                    displayPotentialMap = true;
                    displayElectricField = false;
                }
                if(event.key.code == sf::Keyboard::R){
                    selectedIdx = -1;
                    isSimRunning = false;
                    sim.charges.clear();
                    std::cout << "Simulation Cleared." << std::endl;
                }
            }
        }
        if(sf::Mouse::isButtonPressed(sf::Mouse::Left) && selectedIdx != -1 && !isSimRunning){
            sim.charges[selectedIdx].x = (float)mousePos.x;
            sim.charges[selectedIdx].y = (float)mousePos.y;
        }



        //----RENDER----
        sim.window.clear(sf::Color::Black);

        if(displayElectricField) updateFieldArrows(sim);
        if(displayPotentialMap) updatePotentialMap(sim);
        if(isSimRunning) updateCharges(sim);

        if(displayElectricField) drawFieldArrows(arrow_len, scale_factor, sim);
        if(displayPotentialMap) drawPotentialMap(sim);
        
        drawCharges(sim);
        if(!isSimRunning && selectedIdx != -1){
            sim.point.setPosition(sim.charges[selectedIdx].x, sim.charges[selectedIdx].y);
            sim.window.draw(sim.point);
        }

        sim.window.display();
    }
    return 0;
}
