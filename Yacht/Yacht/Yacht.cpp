#define _CRT_SECURE_NO_WARNINGS
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <algorithm>
#include <string>

// =============================================================
// stb_image.h (텍스처 로드)
// =============================================================
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using glm::vec3;
using glm::mat4;

// =============================================================
// 전역 변수
// =============================================================
int gWidth = 1280;
int gHeight = 720;

GLuint gProgram = 0;
GLuint gCubeVAO = 0, gCubeVBO = 0;

GLuint gDiceTex = 0;
GLuint gTrayTex = 0;

// 카메라 (위에서 수직 내려다보는 설정)
vec3 camPos = vec3(0.0f, 25.0f, 0.0f);
vec3 camTarget = vec3(0.0f, 4.6f, 0.0f);
vec3 camUp = vec3(0.0f, 0.0f, -1.0f);

// 랜덤
std::mt19937 rng{ std::random_device{}() };
std::uniform_int_distribution<int>   distVal(1, 6);
std::uniform_real_distribution<float>distF(-0.5f, 0.5f);

// =============================================================
// Shader
// =============================================================
std::string LoadTextFile(const char* path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to open shader : " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint CompileShader(const char* path, GLenum type)
{
    std::string src = LoadTextFile(path);
    const char* csrc = src.c_str();

    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &csrc, nullptr);
    glCompileShader(sh);

    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        char log[2048];
        glGetShaderInfoLog(sh, 2048, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
    }
    return sh;
}

GLuint CreateProgram()
{
    GLuint vs = CompileShader("vertex.glsl", GL_VERTEX_SHADER);
    GLuint fs = CompileShader("fragment.glsl", GL_FRAGMENT_SHADER);

    GLuint prg = glCreateProgram();
    glAttachShader(prg, vs);
    glAttachShader(prg, fs);
    glLinkProgram(prg);

    GLint ok;
    glGetProgramiv(prg, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[2048];
        glGetProgramInfoLog(prg, 2048, nullptr, log);
        std::cerr << "Program link error: " << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prg;
}

// =============================================================
// OBJ Loader
// =============================================================
bool LoadObj(const char* path, std::vector<float>& out)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to open OBJ: " << path << std::endl;
        return false;
    }

    std::vector<glm::vec3> pos;
    std::vector<glm::vec2> uv;

    std::string line;

    auto pushVert = [&](int vi, int ti) {
        glm::vec3 p = pos[vi];
        glm::vec2 t(0, 0);
        if (ti >= 0 && ti < (int)uv.size())
            t = uv[ti];

        out.push_back(p.x);
        out.push_back(p.y);
        out.push_back(p.z);
        out.push_back(t.x);
        out.push_back(t.y);
        };

    while (std::getline(f, line))
    {
        if (line.size() < 2 || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "v")
        {
            float x, y, z;
            iss >> x >> y >> z;
            pos.push_back({ x,y,z });
        }
        else if (tag == "vt")
        {
            float u, v;
            iss >> u >> v;
            uv.push_back({ u,v });
        }
        else if (tag == "f")
        {
            std::string tok;
            std::vector<std::pair<int, int>> fverts;

            while (iss >> tok)
            {
                int vi = -1, ti = -1;
                size_t s = tok.find('/');
                if (s == std::string::npos)
                {
                    vi = std::stoi(tok) - 1;
                }
                else
                {
                    vi = std::stoi(tok.substr(0, s)) - 1;
                    std::string t = tok.substr(s + 1);
                    if (!t.empty())
                        ti = std::stoi(t) - 1;
                }
                fverts.push_back({ vi,ti });
            }

            for (int i = 1; i + 1 < (int)fverts.size(); ++i)
            {
                pushVert(fverts[0].first, fverts[0].second);
                pushVert(fverts[i].first, fverts[i].second);
                pushVert(fverts[i + 1].first, fverts[i + 1].second);
            }
        }
    }
    return !out.empty();
}

struct Model
{
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;

    bool load(const char* path)
    {
        std::vector<float> verts;
        if (!LoadObj(path, verts))
            return false;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER,
            verts.size() * sizeof(float),
            verts.data(), GL_STATIC_DRAW);

        // pos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(float) * 5, (void*)0);

        // uv
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            sizeof(float) * 5, (void*)(sizeof(float) * 3));

        glBindVertexArray(0);
        count = (GLsizei)(verts.size() / 5);

        return true;
    }

    void draw(const mat4& MVP, const vec3& col, GLuint texID, bool textured)
    {
        if (vao == 0 || count == 0) return;

        glUseProgram(gProgram);
        glBindVertexArray(vao);

        GLint mvpLoc = glGetUniformLocation(gProgram, "uMVP");
        GLint colLoc = glGetUniformLocation(gProgram, "uColor");
        GLint useTexLoc = glGetUniformLocation(gProgram, "uUseTexture");
        GLint texLoc = glGetUniformLocation(gProgram, "uTex");

        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &MVP[0][0]);
        glUniform3fv(colLoc, 1, &col[0]);
        glUniform1i(useTexLoc, textured ? 1 : 0);

        if (textured && texID != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texID);
            glUniform1i(texLoc, 0);
        }

        glDrawArrays(GL_TRIANGLES, 0, count);

        glBindVertexArray(0);
    }

};

Model trayModel;
Model diceModel;

// =============================================================
// 텍스처 로드
// =============================================================
GLuint LoadTexture(const char* path)
{
    // PNG가 위에서 아래 방향으로 저장된 경우 뒤집어서 로드
    stbi_set_flip_vertically_on_load(true);

    int w, h, c;
    unsigned char* buf = stbi_load(path, &w, &h, &c, 4);

    if (!buf)
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        w, h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, buf);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR);

    stbi_image_free(buf);
    return tex;
}

// =============================================================
// Yacht Dice 구조체
// =============================================================
struct Die {
    int   value;
    bool  held;
    vec3  pos;
    vec3  rotAxis;
    float angle;
};

Die gDice[5];

bool  gRolling = false;
float gRollTimer = 0;
const float ROLL_DUR = 0.6f;

int gTurn = 1;
int gRollCount = 0;

// =============================================================
// 주사위 값에 따른 "기본 자세" 회전
//  - Dice.obj가 "1이 위, 2가 앞" 같은 기준이 있을 텐데
//    그 기준에 맞춰 각도를 한 번만 조정하면 됨
// =============================================================
mat4 GetValueRotation(int value)
{
    
    using namespace glm;

    mat4 R(1.0f);

    switch (value)
    {
    case 1:
        R = rotate(mat4(1.0f),
            radians(-90.0f),
            vec3(1, 0, 0));  // x -90
        break;

    case 2:
        R = rotate(mat4(1.0f),
            radians(180.0f),
            vec3(0, 0, 1));  // Z -90
        break;

    case 3:
        R = rotate(mat4(1.0f),
            radians(90.0f),
            vec3(0, 0, 1));  // Z -90
        break;

    case 4:
        R = rotate(mat4(1.0f),
            radians(-90.0f),
            vec3(0, 0, 1));  // Z -90
        break;

    case 5:
        R = rotate(mat4(1.0f),
            radians(0.0f),
            vec3(0, 0, 1));  // Z -90
        break;

    case 6:
        R = rotate(mat4(1.0f),
            radians(90.0f),
            vec3(1, 0, 0));  // Z -90
        break;

    }
    return R;
}

// =============================================================
// 주사위 점수 계산
// =============================================================
std::vector<int> CountDice()
{
    std::vector<int> c(7, 0);
    for (int i = 0; i < 5; i++)
        c[gDice[i].value]++;

    return c;
}

int ScoreUpper(int face)
{
    auto c = CountDice();
    return c[face] * face;
}

int ScoreChoice()
{
    int s = 0;
    for (int i = 0; i < 5; i++) s += gDice[i].value;
    return s;
}

int ScoreFourKind()
{
    auto c = CountDice();
    int tot = ScoreChoice();
    for (int v = 1; v <= 6; v++)
        if (c[v] >= 4) return tot;
    return 0;
}

int ScoreFullHouse()
{
    auto c = CountDice();
    bool t = false, d = false;
    for (int v = 1; v <= 6; v++)
    {
        if (c[v] == 3) t = true;
        if (c[v] == 2) d = true;
    }
    return (t && d) ? 25 : 0;
}

int ScoreSmallStraight()
{
    auto c = CountDice();
    bool s1 = c[1] && c[2] && c[3] && c[4];
    bool s2 = c[2] && c[3] && c[4] && c[5];
    bool s3 = c[3] && c[4] && c[5] && c[6];
    return (s1 || s2 || s3) ? 30 : 0;
}

int ScoreLargeStraight()
{
    auto c = CountDice();
    bool s1 = c[1] && c[2] && c[3] && c[4] && c[5];
    bool s2 = c[2] && c[3] && c[4] && c[5] && c[6];
    return (s1 || s2) ? 40 : 0;
}

int ScoreYacht()
{
    auto c = CountDice();
    for (int v = 1; v <= 6; v++)
        if (c[v] == 5) return 50;
    return 0;
}

// =============================================================
// 카테고리
// =============================================================
enum CategoryType {
    ACES, DEUCES, THREES, FOURS, FIVES, SIXES,
    CHOICE, FOURKIND, FULLHOUSE,
    SSTRAIGHT, LSTRAIGHT, YACHT,
    CATCOUNT
};

struct Category {
    const char* name;
    bool used;
    int score;
};

Category gCat[CATCOUNT] = {
    {"Aces",false,0},
    {"Deuces",false,0},
    {"Threes",false,0},
    {"Fours",false,0},
    {"Fives",false,0},
    {"Sixes",false,0},
    {"Choice",false,0},
    {"4 of a Kind",false,0},
    {"Full House",false,0},
    {"S. Straight",false,0},
    {"L. Straight",false,0},
    {"Yacht",false,0},
};

int TotalScore()
{
    int s = 0;
    for (int i = 0; i < CATCOUNT; i++)
        s += gCat[i].score;
    return s;
}

// =============================================================
// Init Dice
// =============================================================

void InitDice()
{
    float start = -3.0f;
    float step = 1.5f;

    for (int i = 0; i < 5; i++)
    {
        gDice[i].value = distVal(rng);
        gDice[i].held = false;
        gDice[i].pos = vec3(start + step * i, 3.0f, 0.0f);
        gDice[i].rotAxis = glm::normalize(vec3(distF(rng), 1.0f, distF(rng)));
        gDice[i].angle = 0.0f;
    }
    gRollCount = 0;
}


// =============================================================
// 주사위 굴리기
// =============================================================
void StartRoll()
{
    if (gRolling) return;
    if (gRollCount >= 3) return;

    for (int i = 0; i < 5; i++)
    {
        if (gDice[i].held) continue;
        gDice[i].value = distVal(rng);
        gDice[i].rotAxis = glm::normalize(vec3(distF(rng), 1.0f, distF(rng)));
        gDice[i].angle = 0.0f;
    }
    gRolling = true;
    gRollTimer = 0.0f;
    gRollCount++;
}

// =============================================================
// 텍스트 출력
// =============================================================
void DrawText(float x, float y, const char* s)
{
    glRasterPos2f(x, y);
    while (*s)
    {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s);
        s++;
    }
}

// =============================================================
// 텍스트 출력 (0~gWidth, 0~gHeight 픽셀 좌표용)
// =============================================================
void DrawTextPixel(float x, float y, const char* s)
{
    glRasterPos2f(x, y);
    while (*s)
    {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s);
        ++s;
    }
}


// =============================================================
// Display
// =============================================================
void Display()
{
    glClearColor(0.85f, 0.85f, 0.85f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    int leftW = gWidth / 3;
    int rightX = leftW;
    int rightW = gWidth - leftW;

    // ---------- 3D View ----------
    glViewport(rightX, 0, rightW, gHeight);

    mat4 view = glm::lookAt(camPos, camTarget, camUp);
    mat4 proj = glm::perspective(glm::radians(45.0f),
        (float)rightW / gHeight,
        0.1f, 100.0f);

    // 바닥평판
    {
        mat4 M(1.0f);
        M = glm::translate(M, vec3(0.0f, -1.4f, 0.0f));
        M = glm::scale(M, vec3(12.0f, 0.4f, 10.0f));
        glUseProgram(gProgram);
        glUniform1i(glGetUniformLocation(gProgram, "uUseTexture"), 0);

        glBindVertexArray(gCubeVAO);
        glUniformMatrix4fv(glGetUniformLocation(gProgram, "uMVP"),
            1, GL_FALSE, &(proj * view * M)[0][0]);
        glUniform3f(glGetUniformLocation(gProgram, "uColor"),
            0.65f, 0.45f, 0.25f);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 트레이 OBJ + Yachtboard 텍스처
    {
        mat4 model(1.0f);
        model = glm::translate(model, vec3(0.0f, 4.6f, 0.0f));
        model = glm::scale(model, vec3(6.0f, 6.0f, 6.0f));

        trayModel.draw(proj * view * model, vec3(1.0f), gTrayTex, true);
    }

    // 주사위 5개 OBJ (텍스처)
    {
        for (int i = 0; i < 5; i++)
        {
            mat4 M(1.0f);

            // 위치
            M = glm::translate(M, gDice[i].pos);

            // 값에 따른 기본 회전 (윗면 맞추기)
            M *= GetValueRotation(gDice[i].value);

            // 굴리는 중이면 추가 회전
            if (gRolling && !gDice[i].held)
            {
                M = glm::rotate(M,
                    glm::radians(gDice[i].angle),
                    gDice[i].rotAxis);
            }

            // 크기
            M = glm::scale(M, vec3(0.5f));

            diceModel.draw(proj * view * M, vec3(1.0f), gDiceTex, true);
        }
    }

    // ---------------------------------------------------------
    // 주사위 값 디버그용: 각 주사위 위에 숫자 출력
    // ---------------------------------------------------------
    {
        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);

        // 전체 윈도우 기준 픽셀 좌표로 오버레이
        glViewport(0, 0, gWidth, gHeight);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, (double)gWidth, 0.0, (double)gHeight, -1.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor3f(0.0f, 0.0f, 0.0f);

        // 3D → 2D 변환용 뷰포트(오른쪽 3D 영역)
        glm::vec4 vp((float)rightX, 0.0f, (float)rightW, (float)gHeight);

        for (int i = 0; i < 5; ++i)
        {
            // 주사위 위 약간 위쪽 위치
            glm::vec3 worldPos = gDice[i].pos + glm::vec3(0.0f, 0.7f, 0.0f);

            // 3D 좌표를 화면 픽셀 좌표로 변환
            glm::vec3 winPos = glm::project(worldPos, view, proj, vp);

            char buf[8];
            sprintf(buf, "%d", gDice[i].value);

            // 숫자 살짝 위로 올리고 출력
            DrawTextPixel(winPos.x, winPos.y + 10.0f, buf);
        }

        glPopMatrix();              // MODELVIEW
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

    // ---------- 2D Scoreboard ----------
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    glViewport(0, 0, leftW, gHeight);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 배경
    glColor3f(0.98f, 0.96f, 0.60f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(1, 0);
    glVertex2f(1, 1);
    glVertex2f(0, 1);
    glEnd();

    glColor3f(0, 0, 0);

    char buf[128];
    float Y = 0.95f;

    DrawText(0.05f, Y, "YACHT SCORE BOARD");
    Y -= 0.06f;

    sprintf(buf, "Turn %d / 12    Roll %d / 3", gTurn, gRollCount);
    DrawText(0.05f, Y, buf); Y -= 0.06f;

    DrawText(0.05f, Y, "SPACE: Roll   1-5 : Hold");
    Y -= 0.05f;

    DrawText(0.05f, Y, "A-F: Aces~Sixes,  G:Choice  H:4Kind  J:Full");
    Y -= 0.05f;
    DrawText(0.05f, Y, "K:S.S   L:L.S   Y:Yacht");
    Y -= 0.08f;

    for (int i = 0; i < CATCOUNT; i++)
    {
        sprintf(buf, "%2d. %-12s : %3d %s",
            i + 1, gCat[i].name, gCat[i].score,
            gCat[i].used ? "*" : "");
        DrawText(0.05f, Y, buf);
        Y -= 0.045f;
    }

    sprintf(buf, "TOTAL : %d", TotalScore());
    DrawText(0.05f, 0.04f, buf);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glutSwapBuffers();
}

// =============================================================
// Timer
// =============================================================
void Timer(int)
{
    float dt = 0.03f;

    if (gRolling)
    {
        gRollTimer += dt;
        float t = gRollTimer / ROLL_DUR;
        if (t > 1) t = 1;

        for (int i = 0; i < 5; i++)
        {
            if (!gDice[i].held)
                gDice[i].angle = 360.0f * t * 2;
        }

        if (gRollTimer >= ROLL_DUR)
        {
            gRolling = false;
            for (int i = 0; i < 5; i++)
                gDice[i].angle = 0;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(30, Timer, 0);
}

// =============================================================
// Keyboard
// =============================================================
void Keyboard(unsigned char key, int, int)
{
    if (key >= '1' && key <= '5')
    {
        int idx = key - '1';
        gDice[idx].held = !gDice[idx].held;
        return;
    }

    switch (key)
    {
    case ' ':
        StartRoll();
        break;

    case 'a': case 'A':
        if (!gCat[ACES].used) {
            gCat[ACES].used = true;
            gCat[ACES].score = ScoreUpper(1);
            InitDice(); gTurn++;
        } break;

    case 'b': case 'B':
        if (!gCat[DEUCES].used) {
            gCat[DEUCES].used = true;
            gCat[DEUCES].score = ScoreUpper(2);
            InitDice(); gTurn++;
        } break;

    case 'c': case 'C':
        if (!gCat[THREES].used) {
            gCat[THREES].used = true;
            gCat[THREES].score = ScoreUpper(3);
            InitDice(); gTurn++;
        } break;

    case 'd': case 'D':
        if (!gCat[FOURS].used) {
            gCat[FOURS].used = true;
            gCat[FOURS].score = ScoreUpper(4);
            InitDice(); gTurn++;
        } break;

    case 'e': case 'E':
        if (!gCat[FIVES].used) {
            gCat[FIVES].used = true;
            gCat[FIVES].score = ScoreUpper(5);
            InitDice(); gTurn++;
        } break;

    case 'f': case 'F':
        if (!gCat[SIXES].used) {
            gCat[SIXES].used = true;
            gCat[SIXES].score = ScoreUpper(6);
            InitDice(); gTurn++;
        } break;

    case 'g': case 'G':
        if (!gCat[CHOICE].used) {
            gCat[CHOICE].used = true;
            gCat[CHOICE].score = ScoreChoice();
            InitDice(); gTurn++;
        } break;

    case 'h': case 'H':
        if (!gCat[FOURKIND].used) {
            gCat[FOURKIND].used = true;
            gCat[FOURKIND].score = ScoreFourKind();
            InitDice(); gTurn++;
        } break;

    case 'j': case 'J':
        if (!gCat[FULLHOUSE].used) {
            gCat[FULLHOUSE].used = true;
            gCat[FULLHOUSE].score = ScoreFullHouse();
            InitDice(); gTurn++;
        } break;

    case 'k': case 'K':
        if (!gCat[SSTRAIGHT].used) {
            gCat[SSTRAIGHT].used = true;
            gCat[SSTRAIGHT].score = ScoreSmallStraight();
            InitDice(); gTurn++;
        } break;

    case 'l': case 'L':
        if (!gCat[LSTRAIGHT].used) {
            gCat[LSTRAIGHT].used = true;
            gCat[LSTRAIGHT].score = ScoreLargeStraight();
            InitDice(); gTurn++;
        } break;

    case 'y': case 'Y':
        if (!gCat[YACHT].used) {
            gCat[YACHT].used = true;
            gCat[YACHT].score = ScoreYacht();
            InitDice(); gTurn++;
        } break;

    case 27:
        exit(0);
        break;
    }

    glutPostRedisplay();
}

// =============================================================
// InitGL
// =============================================================
void InitGL()
{
    glewInit();
    glEnable(GL_DEPTH_TEST);

    gProgram = CreateProgram();

    // 단색 큐브 (바닥용)
    float s = 0.5f;
    float cube[] = {
        // Front
        -s,-s,s,  s,-s,s,  s,s,s,
        -s,-s,s,  s,s,s,  -s,s,s,
        // Back
        -s,-s,-s, -s,s,-s, s,s,-s,
        -s,-s,-s, s,s,-s, s,-s,-s,
        // Left
        -s,-s,-s, -s,-s,s, -s,s,s,
        -s,-s,-s, -s,s,s, -s,s,-s,
        // Right
         s,-s,-s, s,s,-s, s,s,s,
         s,-s,-s, s,s,s,  s,-s,s,
         // Top
         -s,s,-s, -s,s,s, s,s,s,
         -s,s,-s, s,s,s,  s,s,-s,
         // Bottom
         -s,-s,-s,  s,-s,-s, s,-s,s,
         -s,-s,-s, s,-s,s, -s,-s,s
    };

    glGenVertexArrays(1, &gCubeVAO);
    glGenBuffers(1, &gCubeVBO);

    glBindVertexArray(gCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gCubeVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(cube),
        cube, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        sizeof(float) * 3, (void*)0);

    glBindVertexArray(0);

    // OBJ 로드
    trayModel.load("Yacht.obj");
    diceModel.load("Dice.obj");

    // 텍스처 로드
    gDiceTex = LoadTexture("Dice.png");
    gTrayTex = LoadTexture("Yachtboard.png");

    InitDice();
}

// =============================================================
// main
// =============================================================
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(gWidth, gHeight);
    glutCreateWindow("Yacht Dice Game (OBJ+Texture)");

    InitGL();

    glutDisplayFunc(Display);
    glutReshapeFunc([](int w, int h) { gWidth = w; gHeight = h; });
    glutKeyboardFunc(Keyboard);
    glutTimerFunc(30, Timer, 0);

    glutMainLoop();
    return 0;
}
