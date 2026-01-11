#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;

    uniform sampler2D texture1;

    void main() {
        FragColor = texture(texture1, TexCoord);
    }
)";

struct Mat4 {
    float m[16];

    static Mat4 Identity() {
        Mat4 res;
        for (int i = 0; i < 16; ++i) res.m[i] = 0.0f;
        res.m[0] = res.m[5] = res.m[10] = res.m[15] = 1.0f;
        return res;
    }

    static Mat4 Perspective(float fov, float aspect, float zNear, float zFar) {
        Mat4 res;
        for (int i = 0; i < 16; ++i) res.m[i] = 0.0f;
        float tanHalfFovy = tan(fov / 2.0f);
        res.m[0] = 1.0f / (aspect * tanHalfFovy);
        res.m[5] = 1.0f / tanHalfFovy;
        res.m[10] = -(zFar + zNear) / (zFar - zNear);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return res;
    }

    static Mat4 LookAt(sf::Vector3f eye, sf::Vector3f center, sf::Vector3f up) {
        sf::Vector3f f = center - eye;
        float lenF = sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
        f /= lenF;

        float lenUp = sqrt(up.x * up.x + up.y * up.y + up.z * up.z);
        up /= lenUp;

        sf::Vector3f s(
            f.y * up.z - f.z * up.y,
            f.z * up.x - f.x * up.z,
            f.x * up.y - f.y * up.x
        );
        float lenS = sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
        if (lenS != 0) s /= lenS;

        sf::Vector3f u(
            s.y * f.z - s.z * f.y,
            s.z * f.x - s.x * f.z,
            s.x * f.y - s.y * f.x
        );

        Mat4 res = Identity();
        res.m[0] = s.x; res.m[4] = s.y; res.m[8] = s.z;
        res.m[1] = u.x; res.m[5] = u.y; res.m[9] = u.z;
        res.m[2] = -f.x; res.m[6] = -f.y; res.m[10] = -f.z;
        res.m[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
        res.m[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
        res.m[14] = -(-f.x * eye.x - f.y * eye.y - f.z * eye.z);
        return res;
    }

    Mat4 operator*(const Mat4& r) const {
        Mat4 res;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[col * 4 + k] * r.m[k * 4 + row];
                }
                res.m[col * 4 + row] = sum;
            }
        }
        return res;
    }

    static Mat4 Translate(float x, float y, float z) {
        Mat4 res = Identity();
        res.m[12] = x; res.m[13] = y; res.m[14] = z;
        return res;
    }

    static Mat4 Scale(float x, float y, float z) {
        Mat4 res = Identity();
        res.m[0] = x; res.m[5] = y; res.m[10] = z;
        return res;
    }

    static Mat4 RotateY(float angleDegrees) {
        Mat4 res = Identity();
        float rad = angleDegrees * 3.14159265f / 180.0f;
        float c = cos(rad);
        float s = sin(rad);
        res.m[0] = c; res.m[8] = s;
        res.m[2] = -s; res.m[10] = c;
        return res;
    }
};

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

    if (p_idx >= 0 && p_idx < (int)positions.size()) {
        v.x = positions[p_idx].x;
        v.y = positions[p_idx].y;
        v.z = positions[p_idx].z;
    }
    if (t_idx >= 0 && t_idx < (int)uvs.size()) {
        v.u = uvs[t_idx].x;
        v.v = 1.0f - uvs[t_idx].y;
    }
    return v;
}

std::vector<Vertex> loadModel(const std::string& filename) {
    std::vector<sf::Vector3f> positions;
    std::vector<sf::Vector2f> uvs;
    std::vector<Vertex> finalVertices;

    std::ifstream file(filename);
    if (!file.is_open()) return finalVertices;

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
            std::vector<int> vIndices, vtIndices;
            std::string vertexData;
            while (ss >> vertexData) {
                std::stringstream vss(vertexData);
                std::string segment;
                int idx = 0;
                int v_idx = 0, vt_idx = 0;
                while (std::getline(vss, segment, '/')) {
                    if (!segment.empty()) {
                        int val = std::stoi(segment);
                        if (idx == 0) v_idx = val;
                        else if (idx == 1) vt_idx = val;
                    }
                    idx++;
                }
                vIndices.push_back(v_idx);
                vtIndices.push_back(vt_idx);
            }
            for (size_t i = 1; i < vIndices.size() - 1; ++i) {
                finalVertices.push_back(createVertex(vIndices[0], vtIndices[0], positions, uvs));
                finalVertices.push_back(createVertex(vIndices[i], vtIndices[i], positions, uvs));
                finalVertices.push_back(createVertex(vIndices[i + 1], vtIndices[i + 1], positions, uvs));
            }
        }
    }
    return finalVertices;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << infoLog << std::endl;
    }
    return shader;
}

sf::Vector3f camPos(0.0f, 60.0f, 200.0f);
float camYaw = 0.0f;
float camPitch = -20.0f;
float camSpeed = 80.0f;

int main() {
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::RenderWindow window(sf::VideoMode(1024, 768), "Solar System", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setActive(true);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

    glEnable(GL_DEPTH_TEST);

    GLuint vShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vShader);
    glAttachShader(shaderProgram, fShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vShader);
    glDeleteShader(fShader);

    std::vector<Vertex> modelData = loadModel("./Skull.obj");
    if (modelData.empty()) return -1;

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, modelData.size() * sizeof(Vertex), modelData.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    sf::Texture texture;
    if (!texture.loadFromFile("./skull.jpg")) {
        sf::Image img; img.create(64, 64, sf::Color::Green);
        texture.loadFromImage(img);
    }
    texture.setSmooth(true);
    texture.setRepeated(true);

    std::vector<PlanetParams> planets = {
        { 40.0f,  45.0f, 100.0f, 0.18f, 0.0f },
        { 65.0f,  30.0f, 80.0f,  0.22f, 90.0f },
        { 95.0f, 22.0f, 120.0f, 0.25f, 180.0f },
        { 130.0f, 18.0f, 90.0f,  0.20f, 270.0f },
        { 170.0f, 10.0f, 150.0f, 0.40f, 45.0f }
    };

    sf::Clock frameClock;
    sf::Clock globalClock;

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint texLoc = glGetUniformLocation(shaderProgram, "texture1");

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::Resized) glViewport(0, 0, event.size.width, event.size.height);
        }

        float deltaTime = frameClock.restart().asSeconds();
        float time = globalClock.getElapsedTime().asSeconds();

        float radYaw = camYaw * 3.14159f / 180.0f;
        float radPitch = camPitch * 3.14159f / 180.0f;

        sf::Vector3f forward(
            -sin(radYaw) * cos(radPitch),
            sin(radPitch),
            -cos(radYaw) * cos(radPitch)
        );

        sf::Vector3f right(
            cos(radYaw),
            0.0f,
            -sin(radYaw)
        );

        sf::Vector3f camUp(
            -forward.x * forward.y,
            1.0f - forward.y * forward.y,
            -forward.z * forward.y
        );

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) camPos += forward * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) camPos -= forward * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) camPos -= right * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) camPos += right * camSpeed * deltaTime;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) camPos += camUp * camSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) camPos -= camUp * camSpeed * deltaTime;

        float rotSpeed = 90.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  camYaw += rotSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) camYaw -= rotSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))    camPitch += rotSpeed * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))  camPitch -= rotSpeed * deltaTime;

        if (camPitch > 89.0f) camPitch = 89.0f;
        if (camPitch < -89.0f) camPitch = -89.0f;

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glActiveTexture(GL_TEXTURE0);
        sf::Texture::bind(&texture);
        glUniform1i(texLoc, 0);

        float aspect = (float)window.getSize().x / (float)window.getSize().y;
        Mat4 projMat = Mat4::Perspective(45.0f * 3.14159f / 180.0f, aspect, 1.0f, 600.0f);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMat.m);

        Mat4 viewMat = Mat4::LookAt(camPos, camPos + forward, camUp);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMat.m);

        glBindVertexArray(VAO);

        Mat4 modelMat = Mat4::Identity();
        Mat4 sunRot = Mat4::RotateY(time * 15.0f);
        Mat4 sunScale = Mat4::Scale(0.7f, 0.7f, 0.7f);

        modelMat = sunRot * sunScale;
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelMat.m);
        glDrawArrays(GL_TRIANGLES, 0, modelData.size());

        for (const auto& p : planets) {
            Mat4 mScale = Mat4::Scale(p.scale, p.scale, p.scale);
            Mat4 mSelfRot = Mat4::RotateY(time * p.selfSpeed);
            Mat4 mTrans = Mat4::Translate(p.distance, 0.0f, 0.0f);
            Mat4 mOrbitRot = Mat4::RotateY(time * p.orbitSpeed + p.startAngle);

            Mat4 pModel = mOrbitRot * mTrans * mSelfRot * mScale;

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, pModel.m);
            glDrawArrays(GL_TRIANGLES, 0, modelData.size());
        }

        glBindVertexArray(0);
        window.display();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    return 0;
}