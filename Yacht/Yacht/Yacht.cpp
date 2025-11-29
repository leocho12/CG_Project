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

using glm::vec3;
using glm::mat4;

// =======================================================
// 전역 변수
// =======================================================
int gWidth = 1280;
int gHeight = 720;

GLuint gProgram = 0;
GLuint gCubeVAO = 0, gCubeVBO = 0;

// 카메라 (오른쪽 3D 장면)
vec3 camPos(0.0f, 12.0f, 22.0f);
vec3 camTarget(0.0f, 3.0f, 0.0f);
vec3 camUp(0.0f, 1.0f, 0.0f);

// 랜덤
std::mt19937 rng{ std::random_device{}() };
std::uniform_int_distribution<int> distVal(1, 6);
std::uniform_real_distribution<float> distF(-0.5f, 0.5f);

// OBJ 로드 성공 여부 표시용
bool gTrayLoaded = false;
bool gDiceLoaded = false;

// =======================================================
// 셰이더 유틸
// =======================================================
std::string LoadTextFile(const char* path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to open " << path << std::endl;
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
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, 1024, nullptr, log);
        std::cerr << "Shader compile error (" << path << "):\n" << log << std::endl;
    }
    return sh;
}

GLuint CreateProgram()
{
    GLuint vs = CompileShader("vertex.glsl", GL_VERTEX_SHADER);
    GLuint fs = CompileShader("fragment.glsl", GL_FRAGMENT_SHADER);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// =======================================================
// 아주 단순 OBJ 로더 (정점 위치만 사용)
// =======================================================
bool LoadObjPositions(const char* path, std::vector<float>& outVerts)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to open OBJ: " << path << std::endl;
        return false;
    }

    std::vector<vec3> positions;
    std::string line;

    auto addTriangle = [&](int i0, int i1, int i2) {
        vec3 v0 = positions[i0];
        vec3 v1 = positions[i1];
        vec3 v2 = positions[i2];
        outVerts.push_back(v0.x); outVerts.push_back(v0.y); outVerts.push_back(v0.z);
        outVerts.push_back(v1.x); outVerts.push_back(v1.y); outVerts.push_back(v1.z);
        outVerts.push_back(v2.x); outVerts.push_back(v2.y); outVerts.push_back(v2.z);
        };

    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        }
        else if (tag == "f") {
            std::vector<int> idx;
            std::string token;
            while (iss >> token) {
                size_t slash = token.find('/');
                std::string num = (slash == std::string::npos) ? token : token.substr(0, slash);
                int i = std::stoi(num);
                idx.push_back(i - 1);    // OBJ는 1-base
            }
            if (idx.size() >= 3) {
                for (size_t i = 1; i + 1 < idx.size(); ++i) {
                    addTriangle(idx[0], idx[i], idx[i + 1]);
                }
            }
        }
    }

    if (outVerts.empty()) {
        std::cerr << "OBJ has no geometry: " << path << std::endl;
        return false;
    }
    std::cout << "Loaded OBJ " << path << " : "
        << outVerts.size() / 3 << " vertices\n";
    return true;
}

// 모델 구조체
struct Model {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertexCount = 0;

    bool loadFromObj(const char* path)
    {
        std::vector<float> verts;
        if (!LoadObjPositions(path, verts)) return false;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            verts.size() * sizeof(float),
            verts.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(float) * 3, (void*)0);

        glBindVertexArray(0);
        vertexCount = (GLsizei)(verts.size() / 3);
        return true;
    }

    void draw(const mat4& MVP, const vec3& color) const
    {
        if (vao == 0 || vertexCount == 0) return;

        glUseProgram(gProgram);
        glBindVertexArray(vao);

        GLint mvpLoc = glGetUniformLocation(gProgram, "uMVP");
        GLint colLoc = glGetUniformLocation(gProgram, "uColor");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &MVP[0][0]);
        glUniform3fv(colLoc, 1, &color[0]);

        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glBindVertexArray(0);
    }
};

Model gTrayModel;  // Yacht.obj
Model gDiceModel;  // Dice.obj

// =======================================================
// 기본 큐브(단색) VBO (테이블용)
// =======================================================
void InitCube()
{
    const float s = 0.5f;
    float verts[] = {
        // 앞
        -s,-s, s,  s,-s, s,  s, s, s,
        -s,-s, s,  s, s, s, -s, s, s,
        // 뒤
        -s,-s,-s, -s, s,-s,  s, s,-s,
        -s,-s,-s,  s, s,-s,  s,-s,-s,
        // 왼
        -s,-s,-s, -s,-s, s, -s, s, s,
        -s,-s,-s, -s, s, s, -s, s,-s,
        // 오
         s,-s,-s,  s, s,-s,  s, s, s,
         s,-s,-s,  s, s, s,  s,-s, s,
         // 위
         -s, s,-s, -s, s, s,  s, s, s,
         -s, s,-s,  s, s, s,  s, s,-s,
         // 아래
         -s,-s,-s,  s,-s,-s,  s,-s, s,
         -s,-s,-s,  s,-s, s, -s,-s, s
    };

    glGenVertexArrays(1, &gCubeVAO);
    glGenBuffers(1, &gCubeVBO);

    glBindVertexArray(gCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gCubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

    glBindVertexArray(0);
}

void DrawCube(const mat4& MVP, const vec3& color)
{
    glUseProgram(gProgram);
    glBindVertexArray(gCubeVAO);

    GLint mvpLoc = glGetUniformLocation(gProgram, "uMVP");
    GLint colorLoc = glGetUniformLocation(gProgram, "uColor");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &MVP[0][0]);
    glUniform3fv(colorLoc, 1, &color[0]);

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// =======================================================
// Yacht 게임 상태
// =======================================================
struct Die {
    int    value;      // 1~6
    bool   held;
    vec3   pos;
    vec3   rotAxis;
    float  angle;      // 회전 애니메이션용
};

Die gDice[5];

int  gRollCount = 0;      // 한 턴에 굴린 횟수 (최대 3)
int  gTurn = 1;           // 1~12

bool gRolling = false;
float gRollTimer = 0.0f;
const float ROLL_DURATION = 0.6f;

enum CategoryType {
    CAT_ACES, CAT_DEUCES, CAT_THREES, CAT_FOURS, CAT_FIVES, CAT_SIXES,
    CAT_CHOICE, CAT_FOURKIND, CAT_FULLHOUSE,
    CAT_SMALLST, CAT_LARGEST, CAT_YACHT,
    CAT_COUNT
};

struct Category {
    const char* name;
    bool used;
    int  score;
};

Category gCats[CAT_COUNT] = {
    {"Aces",         false, 0},
    {"Deuces",       false, 0},
    {"Threes",       false, 0},
    {"Fours",        false, 0},
    {"Fives",        false, 0},
    {"Sixes",        false, 0},
    {"Choice",       false, 0},
    {"4 of a Kind",  false, 0},
    {"Full House",   false, 0},
    {"S. Straight",  false, 0},
    {"L. Straight",  false, 0},
    {"Yacht",        false, 0}
};

int TotalScore()
{
    int s = 0;
    for (int i = 0; i < CAT_COUNT; ++i) s += gCats[i].score;
    return s;
}

// 주사위 초기 위치 세팅
void InitDice()
{
    // 트레이 중앙 근처에 놓기
    float startX = -2.5f;
    float step = 1.8f;
    float y = 1.0f;   // 테이블 위 약간 위
    float z = 0.0f;

    for (int i = 0; i < 5; ++i) {
        gDice[i].value = distVal(rng);
        gDice[i].held = false;
        gDice[i].pos = vec3(startX + step * i, y, z);
        gDice[i].rotAxis = glm::normalize(vec3(distF(rng), 1.0f, distF(rng)));
        gDice[i].angle = 0.0f;
    }
    gRollCount = 0;
}

std::vector<int> CountDice()
{
    std::vector<int> c(7, 0);
    for (int i = 0; i < 5; ++i) c[gDice[i].value]++;
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
    for (int i = 0; i < 5; ++i) s += gDice[i].value;
    return s;
}

int ScoreFourKind()
{
    auto c = CountDice();
    int sum = ScoreChoice();
    for (int v = 1; v <= 6; ++v)
        if (c[v] >= 4) return sum;
    return 0;
}

int ScoreFullHouse()
{
    auto c = CountDice();
    bool three = false, two = false;
    for (int v = 1; v <= 6; ++v) {
        if (c[v] == 3) three = true;
        else if (c[v] == 2) two = true;
    }
    return (three && two) ? 25 : 0;
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
    for (int v = 1; v <= 6; ++v)
        if (c[v] == 5) return 50;
    return 0;
}

int CalcCategoryScore(CategoryType cat)
{
    switch (cat) {
    case CAT_ACES:   return ScoreUpper(1);
    case CAT_DEUCES: return ScoreUpper(2);
    case CAT_THREES: return ScoreUpper(3);
    case CAT_FOURS:  return ScoreUpper(4);
    case CAT_FIVES:  return ScoreUpper(5);
    case CAT_SIXES:  return ScoreUpper(6);
    case CAT_CHOICE: return ScoreChoice();
    case CAT_FOURKIND: return ScoreFourKind();
    case CAT_FULLHOUSE: return ScoreFullHouse();
    case CAT_SMALLST: return ScoreSmallStraight();
    case CAT_LARGEST: return ScoreLargeStraight();
    case CAT_YACHT: return ScoreYacht();
    default: return 0;
    }
}

// 굴리기 시작
void StartRoll()
{
    if (gRolling) return;
    if (gRollCount >= 3) return;

    for (int i = 0; i < 5; ++i) {
        if (gDice[i].held) continue;
        gDice[i].value = distVal(rng);
        gDice[i].rotAxis = glm::normalize(vec3(distF(rng), 1.0f, distF(rng)));
        gDice[i].angle = 0.0f;
    }

    gRolling = true;
    gRollTimer = 0.0f;
    ++gRollCount;
}

// =======================================================
// 2D 텍스트 출력 (왼쪽 점수판용) - fixed pipeline 사용
// =======================================================
void DrawBitmapText(float x, float y, const char* text)
{
    glRasterPos2f(x, y);
    while (*text) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *text++);
    }
}

// =======================================================
// 렌더링
// =======================================================
void Display()
{
    glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    // 오른쪽: 3D 테이블 + 트레이 + 주사위
    int rightX = gWidth / 3;
    int rightW = gWidth - rightX;
    glViewport(rightX, 0, rightW, gHeight);

    mat4 view = glm::lookAt(camPos, camTarget, camUp);
    mat4 proj = glm::perspective(glm::radians(45.0f),
        (float)rightW / (float)gHeight,
        0.1f, 100.0f);

    // 테이블(넓은 평판) = 큐브를 납작하게 스케일
    {
        mat4 model(1.0f);
        model = glm::translate(model, vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, vec3(12.0f, 0.5f, 10.0f));
        DrawCube(proj * view * model, vec3(0.6f, 0.4f, 0.25f)); // 나무색
    }

    // 트레이 OBJ (Yacht.obj)
    {
        mat4 model(1.0f);
        // 모델 크기가 -1~1 정도라서 6배 확대, 높이는 대략 테이블 위로
        model = glm::translate(model, vec3(0.0f, 4.6f, 0.0f));
        model = glm::scale(model, vec3(6.0f, 6.0f, 6.0f));
        gTrayModel.draw(proj * view * model, vec3(0.9f, 0.9f, 0.9f));
    }

    // 주사위 5개 OBJ (Dice.obj)
    for (int i = 0; i < 5; ++i) {
        mat4 model(1.0f);
        model = glm::translate(model, gDice[i].pos);
        model = glm::rotate(model,
            glm::radians(gDice[i].angle),
            gDice[i].rotAxis);
        // Dice.obj 가 매우 작아서 (대략 ±0.009) 80배 확대
        model = glm::scale(model, vec3(80.0f));

        vec3 color = gDice[i].held ? vec3(0.9f, 0.9f, 0.3f) : vec3(0.95f);
        gDiceModel.draw(proj * view * model, color);
    }

    // ---------------------------------------------------
    // 왼쪽: 점수판 (2D, fixed pipeline)
    // ---------------------------------------------------
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    glViewport(0, 0, gWidth / 3, gHeight);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 배경 노란 박스
    glColor3f(0.98f, 0.96f, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(0.02f, 0.02f);
    glVertex2f(0.98f, 0.02f);
    glVertex2f(0.98f, 0.98f);
    glVertex2f(0.02f, 0.98f);
    glEnd();

    // 글자 색
    glColor3f(0.0f, 0.0f, 0.0f);

    char buf[128];
    float y = 0.94f;
    DrawBitmapText(0.05f, y, "YACHT SCORE BOARD");
    y -= 0.06f;

    sprintf(buf, "Turn %d / 12   Roll %d / 3", gTurn, gRollCount);
    DrawBitmapText(0.05f, y, buf);
    y -= 0.06f;

    DrawBitmapText(0.05f, y, "SPACE: Roll   1-5: Hold/Release");
    y -= 0.06f;
    DrawBitmapText(0.05f, y, "A-F: Aces~Sixes   G:Choice  H:4Kind  J:Full  K:S.S  L:L.S  Y:Yacht");
    y -= 0.08f;

    for (int i = 0; i < CAT_COUNT; ++i) {
        sprintf(buf, "%2d. %-13s : %3d %s",
            i + 1, gCats[i].name, gCats[i].score,
            gCats[i].used ? "*" : "");
        DrawBitmapText(0.05f, y, buf);
        y -= 0.05f;
    }

    // OBJ 로딩 상태 표시
    sprintf(buf, "Tray:%s  Dice:%s",
        gTrayLoaded ? "OK" : "NG",
        gDiceLoaded ? "OK" : "NG");
    DrawBitmapText(0.05f, 0.10f, buf);

    sprintf(buf, "TOTAL : %3d", TotalScore());
    DrawBitmapText(0.05f, 0.05f, buf);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

// =======================================================
// 업데이트 (주사위 굴리는 애니메이션)
// =======================================================
void Timer(int)
{
    float dt = 0.03f; // 30ms

    if (gRolling) {
        gRollTimer += dt;
        float t = gRollTimer / ROLL_DURATION;
        if (t > 1.0f) t = 1.0f;

        for (int i = 0; i < 5; ++i) {
            if (gDice[i].held) continue;
            gDice[i].angle = 360.0f * t * 2.0f; // 대충 회전
        }

        if (gRollTimer >= ROLL_DURATION) {
            gRolling = false;
            for (int i = 0; i < 5; ++i) {
                gDice[i].angle = 0.0f;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(30, Timer, 0);
}

// =======================================================
// 입력
// =======================================================
void FinishCategory(CategoryType cat)
{
    if (gCats[cat].used) return;
    gCats[cat].score = CalcCategoryScore(cat);
    gCats[cat].used = true;
    InitDice();
    ++gTurn;
}

void Keyboard(unsigned char key, int, int)
{
    if (key >= '1' && key <= '5') {
        int idx = key - '1';
        gDice[idx].held = !gDice[idx].held;
        return;
    }

    switch (key) {
    case ' ':
        StartRoll();
        break;

        // A~F : Aces~Sixes
    case 'a': case 'A': FinishCategory(CAT_ACES);   break;
    case 'b': case 'B': FinishCategory(CAT_DEUCES); break;
    case 'c': case 'C': FinishCategory(CAT_THREES); break;
    case 'd': case 'D': FinishCategory(CAT_FOURS);  break;
    case 'e': case 'E': FinishCategory(CAT_FIVES);  break;
    case 'f': case 'F': FinishCategory(CAT_SIXES);  break;

        // G: Choice
    case 'g': case 'G': FinishCategory(CAT_CHOICE);    break;
        // H: 4Kind
    case 'h': case 'H': FinishCategory(CAT_FOURKIND);  break;
        // J: Full House
    case 'j': case 'J': FinishCategory(CAT_FULLHOUSE); break;
        // K: Small Straight
    case 'k': case 'K': FinishCategory(CAT_SMALLST);   break;
        // L: Large Straight
    case 'l': case 'L': FinishCategory(CAT_LARGEST);   break;
        // Y: Yacht
    case 'y': case 'Y': FinishCategory(CAT_YACHT);     break;

    case 27: // ESC
        exit(0);
        break;
    }

    glutPostRedisplay();
}

// =======================================================
// 초기화
// =======================================================
void InitGL()
{
    glewInit();
    glEnable(GL_DEPTH_TEST);

    gProgram = CreateProgram();
    InitCube();
    InitDice();

    // OBJ 로드
    gTrayLoaded = gTrayModel.loadFromObj("Yacht.obj");
    gDiceLoaded = gDiceModel.loadFromObj("Dice.obj");
    if (!gTrayLoaded) std::cout << "Failed to load Yacht.obj\n";
    if (!gDiceLoaded) std::cout << "Failed to load Dice.obj\n";
}

void Reshape(int w, int h)
{
    gWidth = w;
    gHeight = h;
}

// =======================================================
// main
// =======================================================
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(gWidth, gHeight);
    glutCreateWindow("Yacht Dice Game");

    InitGL();

    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutTimerFunc(30, Timer, 0);

    glutMainLoop();
    return 0;
}
