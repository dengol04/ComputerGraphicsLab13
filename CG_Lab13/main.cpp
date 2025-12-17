#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

struct Vertex {
    float x, y, z;
    float u, v;
};

struct PlanetParams {
    float distance;
    float orbitSpeed;
    float selfSpeed;
    float scale;
    float startAngle;
};

Vertex createVertex(int posIndex, int uvIndex,
    const std::vector<sf::Vector3f>& positions,
    const std::vector<sf::Vector2f>& uvs)
{
    Vertex v = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int p_idx = posIndex - 1;
    int t_idx = uvIndex - 1;
    const float TEXTURE_SCALE = 1.0f;

    if (p_idx >= 0 && p_idx < positions.size()) {
        v.x = positions[p_idx].x;
        v.y = positions[p_idx].y;
        v.z = positions[p_idx].z;
    }

    if (t_idx >= 0 && t_idx < uvs.size()) {
        v.u = uvs[t_idx].x * TEXTURE_SCALE;
        v.v = (1.0f - uvs[t_idx].y) * TEXTURE_SCALE;
    }

    return v;
}

std::vector<Vertex> loadModel(const std::string& filename) {
    std::vector<sf::Vector3f> positions;
    std::vector<sf::Vector2f> uvs;
    std::vector<Vertex> finalVertices;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening " << filename << std::endl;
        return finalVertices;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            positions.push_back({ x, y, z });
        }
        else if (prefix == "vt") {
            float u, v;
            ss >> u >> v;
            uvs.push_back({ u, v });
        }
        else if (prefix == "f") {
            std::vector<int> currentFaceIndices;
            std::string vertexData;

            while (ss >> vertexData) {
                if (vertexData.empty()) continue;
                std::stringstream vss(vertexData);
                std::string indexStr;
                int v_index = 0, vt_index = 0;

                for (int k = 0; std::getline(vss, indexStr, '/'); k++) {
                    if (indexStr.empty()) continue;
                    try {
                        int index = std::stoi(indexStr);
                        if (k == 0) v_index = index;
                        else if (k == 1) vt_index = index;
                    }
                    catch (...) {}
                }
                currentFaceIndices.push_back(v_index);
                currentFaceIndices.push_back(vt_index);
            }

            int numVertices = currentFaceIndices.size() / 2;
            if (numVertices < 3) continue;

            int v1_index = currentFaceIndices[0];
            int vt1_index = currentFaceIndices[1];

            for (int i = 1; i < numVertices - 1; ++i) {
                int v2_index = currentFaceIndices[i * 2];
                int vt2_index = currentFaceIndices[i * 2 + 1];
                int v3_index = currentFaceIndices[(i + 1) * 2];
                int vt3_index = currentFaceIndices[(i + 1) * 2 + 1];

                finalVertices.push_back(createVertex(v1_index, vt1_index, positions, uvs));
                finalVertices.push_back(createVertex(v2_index, vt2_index, positions, uvs));
                finalVertices.push_back(createVertex(v3_index, vt3_index, positions, uvs));
            }
        }
    }
    return finalVertices;
}

sf::Vector3f camPos(-15.0f, 15.0f, 35.0f);
float camYaw = -30.0f;
float camPitch = -15.0f;

float camSpeed = 60.0f;

int main() {
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 3; settings.minorVersion = 0;
    sf::RenderWindow window(sf::VideoMode(800, 600), "Lab CG", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setActive(true);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);

    sf::Texture texture;
    if (!texture.loadFromFile("./skull.jpg")) {
        sf::Image img; img.create(64, 64, sf::Color::Cyan);
        texture.loadFromImage(img);
    }
    sf::Texture::bind(&texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    std::vector<Vertex> modelData = loadModel("./Skull.obj");
    if (modelData.empty()) return -1;

    GLuint vboID;
    glGenBuffers(1, &vboID);
    glBindBuffer(GL_ARRAY_BUFFER, vboID);
    glBufferData(GL_ARRAY_BUFFER, modelData.size() * sizeof(Vertex), modelData.data(), GL_STATIC_DRAW);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (void*)0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));

    std::vector<PlanetParams> planets = {
        { 5.0f,  45.0f, 100.0f, 0.06f, 0.0f },
        { 7.5f,  30.0f, 80.0f,  0.08f, 90.0f },
        { 10.5f, 22.0f, 120.0f, 0.09f, 180.0f },
        { 14.0f, 18.0f, 90.0f,  0.07f, 270.0f },
        { 19.0f, 10.0f, 150.0f, 0.15f, 45.0f }
    };

    sf::Clock frameClock;
    sf::Clock globalClock;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::Resized) glViewport(0, 0, event.size.width, event.size.height);
        }

        float deltaTime = frameClock.restart().asSeconds();

        float radYaw = camYaw * 3.1415926f / 180.0f;
        float radPitch = camPitch * 3.1415926f / 180.0f;

        sf::Vector3f forward(
            -sinf(radYaw) * cosf(radPitch),
            sinf(radPitch),
            -cosf(radYaw) * cosf(radPitch)
        );

        sf::Vector3f right(
            cosf(radYaw),
            0.0f,
           -sinf(radYaw)
        );

        sf::Vector3f Up(
            -forward.x * forward.y,
            1.0f - forward.y * forward.y,
            -forward.z * forward.y
        );

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
            camPos += forward * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
            camPos -= forward * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
            camPos -= right * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
            camPos += right * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) 
            camPos += Up * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) 
            camPos -= Up * camSpeed * deltaTime;

        float rotSpeed = 90.0f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
            camYaw += rotSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
            camYaw -= rotSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
            camPitch += rotSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
            camPitch -= rotSpeed * deltaTime;

        if (camPitch > 89.0f)  camPitch = 89.0f;
        if (camPitch < -89.0f) camPitch = -89.0f;

        float time = globalClock.getElapsedTime().asSeconds();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)window.getSize().x / window.getSize().y;
        glFrustum(-aspect * 0.5f, aspect * 0.5f, -0.5f, 0.5f, 1.0f, 120.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glRotatef(-camPitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-camYaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-camPos.x, -camPos.y, -camPos.z);

        glPushMatrix();
        {
            glRotatef(time * 15.0f, 0.0f, 1.0f, 0.0f);
            float sunScale = 0.3f;
            glScalef(sunScale, sunScale, sunScale);
            glDrawArrays(GL_TRIANGLES, 0, modelData.size());
        }
        glPopMatrix();

        for (const auto& planet : planets) {
            glPushMatrix();

            glRotatef(time * planet.orbitSpeed + planet.startAngle, 0.0f, 1.0f, 0.0f);
            glTranslatef(planet.distance, 0.0f, 0.0f);
            glRotatef(time * planet.selfSpeed, 0.0f, 1.0f, 0.0f);
            glScalef(planet.scale, planet.scale, planet.scale);

            glDrawArrays(GL_TRIANGLES, 0, modelData.size());

            glPopMatrix();
        }

        window.display();

    }
    glDeleteBuffers(1, &vboID);
    return 0;
}