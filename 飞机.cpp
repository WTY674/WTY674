#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <conio.h>    
#include <windows.h>  
#include <string> 
#include <algorithm> 

const int GAME_WIDTH = 55;   // 游戏区域宽度
const int GAME_HEIGHT = 35;  // 游戏区域高度

const int SCREEN_WIDTH = GAME_WIDTH + 2; // 左右边框各占1
const int SCREEN_HEIGHT = GAME_HEIGHT + 2 + 2; // 上下边框各占1，底部信息多占1行

const char EMPTY = ' ';

void gotoXY(int x, int y) {
    COORD pos = { (SHORT)x, (SHORT)y };
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(hOut, pos);
}

void HideCursor() {
    CONSOLE_CURSOR_INFO cursor_info = { 1, 0 };
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}

class Bullet {
public:
    int x, y;
    Bullet(int startX, int startY) : x(startX), y(startY) {}

    void move() { y--; }

    bool isOutOfBounds() { return y < 0; }
};

class Enemy {
public:
    int x, y;

    std::vector<std::string> shape = { "\\+/", " | " };
    Enemy(int startX, int startY) : x(startX), y(startY) {}

    void move() { y++; }

    bool isOutOfBounds() { return y + (int)shape.size() > GAME_HEIGHT; }

    void shoot(std::vector<Bullet*>& enemyBullets) {

        if (y + (int)shape.size() < GAME_HEIGHT) {
            enemyBullets.push_back(new Bullet(x + 1, y + (int)shape.size()));
        }
    }
};

class Airplane {
public:
    int x, y;

    std::vector<std::string> shape = { " /=\\", "<<*>>", " * *" };
    Airplane(int startX, int startY) : x(startX), y(startY) {}

    void moveLeft() { if (x > 0) x--; }

    void moveRight() { if (x < GAME_WIDTH - (int)shape[1].length()) x++; }

    void moveUp() { if (y > 0) y--; }

    void moveDown() { if (y < GAME_HEIGHT - (int)shape.size()) y++; }

    Bullet* shoot() {
        int bullet_x = x + (int)shape[0].length() / 2;
        int bullet_y = y - 1;

        // 确保子弹的初始Y坐标不会小于游戏区域的顶部0
        if (bullet_y < 0) {
            bullet_y = 0; // 强制子弹从游戏区域最顶部发射
        }
        return new Bullet(bullet_x, bullet_y);
    }
};

class Game {
private:

    CHAR_INFO screenBuffer[SCREEN_HEIGHT][SCREEN_WIDTH];
    Airplane* airplane;
    std::vector<Bullet*> bullets;
    std::vector<Bullet*> enemyBullets;
    std::vector<Enemy*> enemies;
    int score;
    int enemyCount;
    bool gameOver;
    HANDLE hOutput;

    int enemyMoveCounter;
    const int ENEMY_MOVE_FREQUENCY = 15; // 约每15帧移动一次，即 30FPS / 15帧/移动 = 2次移动/秒

    int enemyBulletMoveCounter;
    const int ENEMY_BULLET_MOVE_FREQUENCY = 3; // 约每3帧移动一次，即 30FPS / 3帧/移动 = 10次移动/秒

    void initializeScreen() {
        for (int i = 0; i < SCREEN_HEIGHT; ++i) {
            for (int j = 0; j < SCREEN_WIDTH; ++j) {
                screenBuffer[i][j].Char.AsciiChar = EMPTY;
                screenBuffer[i][j].Attributes = 7; // Default color attributes
            }
        }

        // 绘制边框
        // 顶部边框
        for (int j = 0; j < SCREEN_WIDTH; ++j) {
            screenBuffer[0][j].Char.AsciiChar = '-';
        }
        // 底部边框 (游戏区域下方)
        for (int j = 0; j < SCREEN_WIDTH; ++j) {
            screenBuffer[GAME_HEIGHT + 1][j].Char.AsciiChar = '-';
        }
        // 左右边框
        for (int i = 0; i < GAME_HEIGHT + 2; ++i) { // 包含上下边框行
            screenBuffer[i][0].Char.AsciiChar = '|';
            screenBuffer[i][GAME_WIDTH + 1].Char.AsciiChar = '|';
        }
        // 四个角
        screenBuffer[0][0].Char.AsciiChar = '+';
        screenBuffer[0][GAME_WIDTH + 1].Char.AsciiChar = '+';
        screenBuffer[GAME_HEIGHT + 1][0].Char.AsciiChar = '+';
        screenBuffer[GAME_HEIGHT + 1][GAME_WIDTH + 1].Char.AsciiChar = '+';
    }

    void render() {
        SMALL_RECT rect = { 0, 0, (SHORT)(SCREEN_WIDTH - 1), (SHORT)(SCREEN_HEIGHT - 1) };
        COORD bufferSize = { (SHORT)SCREEN_WIDTH, (SHORT)SCREEN_HEIGHT };
        COORD bufferCoord = { 0, 0 };

        WriteConsoleOutputA(hOutput, &screenBuffer[0][0], bufferSize, bufferCoord, &rect);

        // 游戏结束时显示在边框下方
        if (gameOver) {
            gotoXY(0, SCREEN_HEIGHT - 1);
            std::cout << "游戏结束！最终得分: " << score << std::endl;
        }
    }

    void updateScreen() {
        initializeScreen(); 

        for (size_t i = 0; i < airplane->shape.size(); ++i) {
            for (size_t j = 0; j < airplane->shape[i].length(); ++j) {
                int screenX = airplane->x + j + 1; // 加上边框宽度
                int screenY = airplane->y + i + 1; // 加上边框高度
                // 确保绘制在游戏区域内
                if (screenX > 0 && screenX < SCREEN_WIDTH - 1 && screenY > 0 && screenY < SCREEN_HEIGHT - 1) {
                    screenBuffer[screenY][screenX].Char.AsciiChar = airplane->shape[i][j];
                }
            }
        }

        for (auto bullet : bullets) {
            // 确保绘制在游戏区域内
            int screenX = bullet->x + 1; // 加上边框宽度
            int screenY = bullet->y + 1; // 加上边框高度
            if (screenX > 0 && screenX < SCREEN_WIDTH - 1 && screenY > 0 && screenY < SCREEN_HEIGHT - 1) {
                screenBuffer[screenY][screenX].Char.AsciiChar = '|';
            }
        }

        for (auto bullet : enemyBullets) {
            // 确保绘制在游戏区域内
            int screenX = bullet->x + 1; // 加上边框宽度
            int screenY = bullet->y + 1; // 加上边框高度
            if (screenX > 0 && screenX < SCREEN_WIDTH - 1 && screenY > 0 && screenY < SCREEN_HEIGHT - 1) {
                screenBuffer[screenY][screenX].Char.AsciiChar = '|';
            }
        }

        for (auto enemy : enemies) {
            for (size_t i = 0; i < enemy->shape.size(); ++i) {
                for (size_t j = 0; j < enemy->shape[i].length(); ++j) {
                    int screenX = enemy->x + j + 1; // 加上边框宽度
                    int screenY = enemy->y + i + 1; // 加上边框高度
                    // 确保绘制在游戏区域内
                    if (screenX > 0 && screenX < SCREEN_WIDTH - 1 && screenY > 0 && screenY < SCREEN_HEIGHT - 1) {
                        screenBuffer[screenY][screenX].Char.AsciiChar = enemy->shape[i][j];
                    }
                }
            }
        }

        if (!gameOver) {
            std::string scoreText = "得分: " + std::to_string(score);
            std::string enemyText = " 剩余敌机: " + std::to_string(enemyCount);
            std::string infoText = scoreText + enemyText;

            // 将信息写入屏幕缓冲区边框下方
            int startX = 0;
            int startY = GAME_HEIGHT + 2; // 游戏区域底部边框下方的行

            for (size_t k = 0; k < infoText.length(); ++k) {
                int bufferX = startX + k;
                if (bufferX < SCREEN_WIDTH) { // 确保不超过屏幕宽度
                    screenBuffer[startY][bufferX].Char.AsciiChar = infoText[k];
                    screenBuffer[startY][bufferX].Attributes = 7; // 默认颜色
                }
            }
        }
    }

    void handleInput() {
        if (_kbhit()) {
            char key = _getch();
            if (key == 'a' || key == 'A') airplane->moveLeft();
            else if (key == 'd' || key == 'D') airplane->moveRight();
            else if (key == 'w' || key == 'W') airplane->moveUp();
            else if (key == 's' || key == 'S') airplane->moveDown();
            else if (key == ' ') { 
                Bullet* bullet = airplane->shoot();
                bullets.push_back(bullet);
            }
        }
    }

    void updateGame() {
        for (auto it = bullets.begin(); it != bullets.end(); ) {
            (*it)->move();
            // 子弹是否超出游戏区域的判断
            if ((*it)->isOutOfBounds()) {
                delete* it; 
                it = bullets.erase(it); 
            }
            else {
                ++it;
            }
        }

        // 敌机子弹移动逻辑，实现速度控制
        enemyBulletMoveCounter++;
        if (enemyBulletMoveCounter >= ENEMY_BULLET_MOVE_FREQUENCY) {
            for (auto it = enemyBullets.begin(); it != enemyBullets.end(); ) {
                (*it)->y++; 
                // 敌机子弹是否超出游戏区域的判断
                if ((*it)->y >= GAME_HEIGHT) {
                    delete* it;
                    it = enemyBullets.erase(it);
                }
                else {
                    ++it;
                }
            }
            enemyBulletMoveCounter = 0; // 重置计数器
        }

        enemyMoveCounter++;
        if (enemyMoveCounter >= ENEMY_MOVE_FREQUENCY) {
            for (auto it = enemies.begin(); it != enemies.end(); ) {
                (*it)->move(); // 移动敌机
                // 敌机是否超出游戏区域的判断
                if ((*it)->isOutOfBounds()) {
                    delete* it;
                    it = enemies.erase(it);
                }
                else {
                    ++it;
                }
            }
            enemyMoveCounter = 0; // 重置计数器
        }

        for (auto enemy : enemies) {
            // [修改: 敌机发射子弹的几率降低一半]
            if (rand() % 100 < 2) { 
                enemy->shoot(enemyBullets);
            }
        }

        if (enemies.size() < 5 && enemyCount > (int)enemies.size()) {
            int x = rand() % (GAME_WIDTH - 4); 
            enemies.push_back(new Enemy(x, 0));
        }

        for (auto bulletIt = bullets.begin(); bulletIt != bullets.end(); ) {
            bool collision = false;
            for (auto enemyIt = enemies.begin(); enemyIt != enemies.end(); ) {
                for (size_t i = 0; i < (*enemyIt)->shape.size(); ++i) {
                    for (size_t j = 0; j < (*enemyIt)->shape[i].length(); ++j) {
                        if ((*bulletIt)->x == (*enemyIt)->x + j && (*bulletIt)->y == (*enemyIt)->y + i) {

                            delete* bulletIt;
                            bulletIt = bullets.erase(bulletIt);

                            delete* enemyIt;
                            enemyIt = enemies.erase(enemyIt);

                            score++;
                            enemyCount--; // 击败敌机才减少总数
                            collision = true;
                            break;
                        }
                    }
                    if (collision) break;
                }
                if (collision) break;
                else ++enemyIt;
            }
            if (!collision) ++bulletIt;
        }

        for (auto it = enemyBullets.begin(); it != enemyBullets.end(); ) {
            bool collidedWithPlayer = false;
            for (size_t i = 0; i < airplane->shape.size(); ++i) {
                for (size_t j = 0; j < airplane->shape[i].length(); ++j) {
                    if ((*it)->x == airplane->x + j && (*it)->y == airplane->y + i) {
                        gameOver = true;
                        collidedWithPlayer = true;
                        break;
                    }
                }
                if (collidedWithPlayer) break;
            }
            if ((*it)->y >= GAME_HEIGHT || collidedWithPlayer) { 
                delete* it;
                it = enemyBullets.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto enemy : enemies) {
            for (size_t i = 0; i < enemy->shape.size(); ++i) {
                for (size_t j = 0; j < enemy->shape[i].length(); ++j) {
                    for (size_t k = 0; k < airplane->shape.size(); ++k) {
                        for (size_t l = 0; l < airplane->shape[k].length(); ++l) {
                            if (enemy->x + j == airplane->x + l && enemy->y + i == airplane->y + k) {
                                gameOver = true;
                                break;
                            }
                        }
                        if (gameOver) break;
                    }
                    if (gameOver) break;
                }
                if (gameOver) break;
            }
            if (gameOver) break;
        }

        if (enemyCount <= 0 && !gameOver) {
            gameOver = true;
        }
    }

public:

    Game(int totalEnemies) : score(0), enemyCount(totalEnemies), gameOver(false),
        enemyMoveCounter(0), enemyBulletMoveCounter(0) {
        // 飞机初始位置在游戏区域内
        airplane = new Airplane(GAME_WIDTH / 2 - 2, GAME_HEIGHT - 3);
        hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        srand(time(0));
    }

    ~Game() {
        delete airplane;
        for (auto bullet : bullets) delete bullet;
        bullets.clear();
        for (auto bullet : enemyBullets) delete bullet;
        enemyBullets.clear();
        for (auto enemy : enemies) delete enemy;
        enemies.clear();
    }

    void run() {

        while (_kbhit()) {
            _getch();
        }

        airplane->y = GAME_HEIGHT - (int)airplane->shape.size();
        Sleep(1000); 
        while (!gameOver) {
            handleInput();
            updateGame();
            updateScreen();
            render();
            Sleep(33); 
        }

        SMALL_RECT rect = { 0, 0, (SHORT)(SCREEN_WIDTH - 1), (SHORT)(SCREEN_HEIGHT - 1) };
        COORD bufferSize = { (SHORT)SCREEN_WIDTH, (SHORT)SCREEN_HEIGHT };
        COORD bufferCoord = { 0, 0 };
        initializeScreen(); 
        WriteConsoleOutputA(hOutput, &screenBuffer[0][0], bufferSize, bufferCoord, &rect);

        gotoXY(0, SCREEN_HEIGHT - 1);
        std::cout << "游戏结束！最终得分: " << score << std::endl;

        while (_kbhit()) _getch();
        _getch();
    }
};

int main() {
    int totalEnemies = 10;

    while (_kbhit()) {
        _getch();
    }
    Sleep(2000); 

    Game game(totalEnemies); 
    HideCursor(); 
    game.run(); 

    return 0;
}