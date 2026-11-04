#include <SDL2/SDL.h>
#include <emscripten.h>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

// Рекорды хранятся локально в браузере и переживают перезагрузку страницы.
EM_JS(int, loadBestMaze, (), {
    try { var n = parseInt(localStorage.getItem('mazeRobot.bestMaze') || '0', 10); return isNaN(n) ? 0 : n; }
    catch (e) { return 0; }
});
EM_JS(int, loadTankWins, (), {
    try { var n = parseInt(localStorage.getItem('mazeRobot.tankWins') || '0', 10); return isNaN(n) ? 0 : n; }
    catch (e) { return 0; }
});
EM_JS(int, loadMinerWins, (), {
    try { var n = parseInt(localStorage.getItem('mazeRobot.minerWins') || '0', 10); return isNaN(n) ? 0 : n; }
    catch (e) { return 0; }
});
EM_JS(void, storeRecords, (int maze, int tanks, int miner), {
    try {
        localStorage.setItem('mazeRobot.bestMaze', String(maze));
        localStorage.setItem('mazeRobot.tankWins', String(tanks));
        localStorage.setItem('mazeRobot.minerWins', String(miner));
    } catch (e) { /* Игра продолжает работать, даже если хранилище недоступно. */ }
});
EM_JS(int, loadAchievements, (), {
    try { var n = parseInt(localStorage.getItem('mazeRobot.achievements') || '0', 10); return isNaN(n) ? 0 : n; }
    catch (e) { return 0; }
});
EM_JS(void, storeAchievements, (int value), {
    try { localStorage.setItem('mazeRobot.achievements', String(value)); } catch (e) {}
});

const int W = 400;
const int H = 500;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

enum State { MENU, MAZE, TANKS, MINER };
State current = MENU;
bool paused = false;

// --- ЛАБИРИНТ: ТРИ УРОВНЯ 5x5 ---
// 0 — свободная клетка, 1 — стена. У каждого уровня есть путь от (0,0) к (4,4).
const int mazeMaps[3][5][5] = {
    {{0,0,1,0,0},{1,0,1,0,1},{0,0,0,0,1},{0,1,1,0,0},{0,0,0,1,0}},
    {{0,1,0,0,0},{0,1,0,1,0},{0,0,0,1,0},{1,1,0,0,0},{0,0,0,1,0}},
    {{0,0,0,1,0},{1,1,0,1,0},{0,0,0,0,0},{0,1,1,1,0},{0,0,0,0,0}}
};
int mazeLevel = 0, mx = 0, my = 0, mazeSteps = 0;
bool mazeWon = false, mazeAllWon = false;
int bestMazeSteps = 0, tankWins = 0, minerWins = 0;
bool tankVictorySaved = false;\nint achievementMask = 0;\nbool achievementsOpen = false;\nvoid unlockAchievement(int bit) {\n    int flag = 1 << bit;\n    if ((achievementMask & flag) == 0) { achievementMask |= flag; storeAchievements(achievementMask); }\n}
bool mazeCanMove(int x, int y) {
    return x >= 0 && x < 5 && y >= 0 && y < 5 && mazeMaps[mazeLevel][y][x] == 0;
}
void resetMazeLevel() { mx=0; my=0; mazeSteps=0; mazeWon=false; mazeAllWon=false; }
void tryMazeMove(int dx, int dy) {
    if (mazeWon || mazeAllWon) return;
    int nx=mx+dx, ny=my+dy;
    if (mazeCanMove(nx,ny)) {
        mx=nx; my=ny; mazeSteps++;
        if (mx==4 && my==4) {
            mazeWon=true;
            unlockAchievement(0);
            if (bestMazeSteps == 0 || mazeSteps < bestMazeSteps) {
                bestMazeSteps = mazeSteps;
                storeRecords(bestMazeSteps, tankWins, minerWins);
            }
            if (mazeLevel==2) { mazeAllWon=true; unlockAchievement(1); }
        }
    }
}
void nextMazeLevel() {
    if (mazeLevel < 2) mazeLevel++;
    else mazeLevel=0;
    resetMazeLevel();
}

// --- ТАНКИ ---
float px_t = 50, py_t = 50, pa_t = 0;
int php = 3;
float ex_t = 350, ey_t = 50;
int ehp = 2;
std::vector<float> bx, by, bdx, bdy;
std::vector<int> bulletOwner; // 0 — игрок, 1 — противник
int pcd = 0, ecd = 0;
std::vector<SDL_Rect> walls;
int tanksWave = 1;
int enemyMoveTick = 0;
const SDL_Rect tankArenas[3][6] = {
    {{150,0,20,100},{150,150,20,100},{250,150,20,150},{0,200,100,20},{300,200,100,20},{150,300,100,20}},
    {{80,110,20,150},{180,60,20,120},{280,180,20,150},{130,300,140,20},{20,230,70,20},{310,90,70,20}},
    {{100,80,20,170},{200,180,20,170},{300,80,20,170},{30,280,100,20},{150,80,100,20},{270,280,100,20}}
};
void setTankArena(int index) {
    walls.clear();
    for (int i=0; i<6; i++) walls.push_back(tankArenas[index%3][i]);
}
bool tankBlocked(float x, float y) {
    if (x < 15 || x > 385 || y < 15 || y > 385) return true;
    SDL_Rect body={(int)x-10,(int)y-10,20,20};
    for (auto &wall : walls) if (SDL_HasIntersection(&body,&wall)) return true;
    return false;
}

// --- ШАХТЁР ---
int grid[8][10];
int px_m = 1, py_m = 4;
std::vector<int> program;
int execIdx = -1;
Uint32 lastExec = 0;
bool minerWon = false;


// --- ПИКСЕЛЬНЫЙ ШРИФТ 3x5 ---
uint16_t glyph(char c) {
    switch (c) {
        case 'A': return 0x7B6F; case 'B': return 0x6B6B; case 'C': return 0x7927;
        case 'D': return 0x6B6B; case 'E': return 0x7937; case 'F': return 0x7911;
        case 'G': return 0x796F; case 'H': return 0x5B6D; case 'I': return 0x7497;
        case 'J': return 0x124B; case 'K': return 0x5D55; case 'L': return 0x4927;
        case 'M': return 0x5F6D; case 'N': return 0x5B6D; case 'O': return 0x7B6F;
        case 'P': return 0x7B39; case 'Q': return 0x7B6F; case 'R': return 0x7B5D;
        case 'S': return 0x793E; case 'T': return 0x7492; case 'U': return 0x5B6F;
        case 'V': return 0x5B6A; case 'W': return 0x5B7F; case 'X': return 0x5AAD;
        case 'Y': return 0x5A92; case 'Z': return 0x72B7;
        case '0': return 0x7B6F; case '1': return 0x7497; case '2': return 0x73B7;
        case '3': return 0x73B6; case '4': return 0x5B92; case '5': return 0x79B6;
        case '6': return 0x79BF; case '7': return 0x7292; case '8': return 0x7BBF;
        case '9': return 0x7BB6; case '-': return 0x01C0; case ':': return 0x0410;
        case '.': return 0x0002; case '/': return 0x12A4; case '+': return 0x05D0;
        default: return 0;
    }
}
void drawText(const std::string& value, int x, int y, int scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int cursor = x;
    for (char raw : value) {
        char c = raw >= 'a' && raw <= 'z' ? (char)(raw - 32) : raw;
        if (c == ' ') { cursor += 4 * scale; continue; }
        uint16_t bits = glyph(c);
        for (int row=0; row<5; row++) for (int col=0; col<3; col++) {
            int bit = 14 - (row*3 + col);
            if (bits & (1u << bit)) {
                SDL_Rect px = {cursor + col*scale, y + row*scale, scale, scale};
                SDL_RenderFillRect(renderer, &px);
            }
        }
        cursor += 4 * scale;
    }
}
void spriteRect(int x, int y, int w, int h, int scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
    SDL_Rect r={x,y,w*scale,h*scale}; SDL_RenderFillRect(renderer,&r);
}
void drawPixelRobot(int x, int y, int scale=2) {
    // Бело-серебряный робот с голубым визором и маленькой антенной.
    spriteRect(x+4*scale,y+1*scale,2,2,scale,{55,190,255,255});
    spriteRect(x+5*scale,y+3*scale,2,2,scale,{180,205,230,255});
    spriteRect(x+2*scale,y+5*scale,10,8,scale,{225,237,248,255});
    spriteRect(x+3*scale,y+6*scale,8,5,scale,{35,48,76,255});
    spriteRect(x+4*scale,y+7*scale,2,2,scale,{40,225,255,255});
    spriteRect(x+8*scale,y+7*scale,2,2,scale,{40,225,255,255});
    spriteRect(x+5*scale,y+10*scale,4,1,scale,{95,190,220,255});
    spriteRect(x+1*scale,y+7*scale,1,4,scale,{100,180,220,255});
    spriteRect(x+12*scale,y+7*scale,1,4,scale,{100,180,220,255});
    spriteRect(x+4*scale,y+13*scale,6,5,scale,{190,210,230,255});
    spriteRect(x+5*scale,y+14*scale,4,2,scale,{45,205,235,255});
    spriteRect(x+2*scale,y+14*scale,2,4,scale,{225,237,248,255});
    spriteRect(x+10*scale,y+14*scale,2,4,scale,{225,237,248,255});
    spriteRect(x+4*scale,y+18*scale,2,3,scale,{125,155,190,255});
    spriteRect(x+8*scale,y+18*scale,2,3,scale,{125,155,190,255});
}
void drawPixelTank(int x, int y, int scale=2) {
    // Зелёный танк: гусеницы, корпус и направленная вправо башня.
    spriteRect(x+1*scale,y+5*scale,13,5,scale,{35,48,65,255});
    spriteRect(x+2*scale,y+6*scale,11,3,scale,{105,125,145,255});
    for(int i=0;i<5;i++) spriteRect(x+(2+i*2)*scale,y+7*scale,1,1,scale,{35,48,65,255});
    spriteRect(x+2*scale,y+4*scale,11,4,scale,{65,165,65,255});
    spriteRect(x+4*scale,y+2*scale,6,4,scale,{105,205,75,255});
    spriteRect(x+7*scale,y+1*scale,7,1,scale,{105,205,75,255});
    spriteRect(x+12*scale,y+0*scale,2,2,scale,{190,220,100,255});
    spriteRect(x+5*scale,y+3*scale,2,1,scale,{35,95,55,255});
}
void drawPixelMiner(int x, int y, int scale=2) {
    // Шахтёр в золотой каске с киркой.
    spriteRect(x+4*scale,y+1*scale,7,2,scale,{255,190,35,255});
    spriteRect(x+3*scale,y+3*scale,9,2,scale,{225,135,25,255});
    spriteRect(x+5*scale,y+4*scale,5,4,scale,{245,185,135,255});
    spriteRect(x+6*scale,y+5*scale,1,1,scale,{35,35,55,255});
    spriteRect(x+9*scale,y+5*scale,1,1,scale,{35,35,55,255});
    spriteRect(x+5*scale,y+8*scale,6,5,scale,{185,90,35,255});
    spriteRect(x+3*scale,y+8*scale,2,4,scale,{245,185,135,255});
    spriteRect(x+11*scale,y+8*scale,2,4,scale,{245,185,135,255});
    spriteRect(x+5*scale,y+13*scale,2,3,scale,{65,75,100,255});
    spriteRect(x+9*scale,y+13*scale,2,3,scale,{65,75,100,255});
    spriteRect(x+13*scale,y+3*scale,1,10,scale,{145,95,55,255});
    spriteRect(x+12*scale,y+2*scale,5,1,scale,{210,225,240,255});
    spriteRect(x+15*scale,y+3*scale,2,2,scale,{210,225,240,255});
}

// --- ФУНКЦИИ ОТРИСОВКИ UI ---
void drawBtn(int x, int y, int w, int h, bool hover, bool active, SDL_Color color) {
    SDL_Rect shadow = {x + 3, y + 3, w, h};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 50);
    SDL_RenderFillRect(renderer, &shadow);

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);

    if (hover) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 30);
        SDL_RenderFillRect(renderer, &r);
    }
    if (active) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 30);
        SDL_RenderFillRect(renderer, &r);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
    SDL_RenderDrawRect(renderer, &r);
}

void drawArrow(int x, int y, int dir) { // 0 up, 1 down, 2 left, 3 right
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int cx = x + 25, cy = y + 25;
    if (dir == 0) { SDL_RenderDrawLine(renderer, cx, cy-10, cx-10, cy+10); SDL_RenderDrawLine(renderer, cx, cy-10, cx+10, cy+10); SDL_RenderDrawLine(renderer, cx, cy-10, cx, cy+10); }
    if (dir == 1) { SDL_RenderDrawLine(renderer, cx, cy+10, cx-10, cy-10); SDL_RenderDrawLine(renderer, cx, cy+10, cx+10, cy-10); SDL_RenderDrawLine(renderer, cx, cy-10, cx, cy+10); }
    if (dir == 2) { SDL_RenderDrawLine(renderer, cx-10, cy, cx+10, cy-10); SDL_RenderDrawLine(renderer, cx-10, cy, cx+10, cy+10); SDL_RenderDrawLine(renderer, cx-10, cy, cx+10, cy); }
    if (dir == 3) { SDL_RenderDrawLine(renderer, cx+10, cy, cx-10, cy-10); SDL_RenderDrawLine(renderer, cx+10, cy, cx-10, cy+10); SDL_RenderDrawLine(renderer, cx+10, cy, cx-10, cy); }
}

void drawPickaxe(int x, int y) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x+10, y+35, x+30, y+15); // ручка
    SDL_RenderDrawLine(renderer, x+15, y+10, x+35, y+20); // лезвие
    SDL_RenderDrawLine(renderer, x+15, y+10, x+20, y+5);
    SDL_RenderDrawLine(renderer, x+35, y+20, x+30, y+25);
}

void drawBlockIcon(int x, int y) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect r = {x+12, y+12, 26, 26};
    SDL_RenderDrawRect(renderer, &r);
    SDL_RenderDrawLine(renderer, x+12, y+12, x+25, y+25);
    SDL_RenderDrawLine(renderer, x+38, y+12, x+25, y+25);
}

void drawPlayIcon(int x, int y) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x+15, y+10, x+15, y+30);
    SDL_RenderDrawLine(renderer, x+15, y+10, x+30, y+20);
    SDL_RenderDrawLine(renderer, x+15, y+30, x+30, y+20);
}

// --- СБРОС ИГР ---
void resetMiner() {
    int map[8][10] = {
        {0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},
        {1,1,1,0,1,1,1,1,1,1}, {1,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1}
    };
    for(int i=0; i<8; i++) for(int j=0; j<10; j++) grid[i][j] = map[i][j];
    px_m = 1; py_m = 4; program.clear(); execIdx = -1; minerWon = false;
}

void resetTanks() {
    px_t = 50; py_t = 50; pa_t = 0; php = 3;
    tanksWave = 1; enemyMoveTick = 0; tankVictorySaved = false;
    setTankArena(0);
    ex_t = 350; ey_t = 50; ehp = 2;
    bx.clear(); by.clear(); bdx.clear(); bdy.clear(); bulletOwner.clear();
    pcd = 0; ecd = 35;
}

void handleMinerCommand(int cmd) {
    if (cmd == 1 && px_m > 0 && grid[py_m][px_m-1] == 0) px_m--;
    else if (cmd == 2 && px_m < 9 && grid[py_m][px_m+1] == 0) px_m++;
    else if (cmd == 3 && py_m < 7) { grid[py_m+1][px_m] = 0; py_m++; }
    else if (cmd == 4 && px_m < 9 && py_m < 7 && grid[py_m+1][px_m+1] == 0) grid[py_m+1][px_m+1] = 1;
    // Вертикальное копание запускает падение; горизонтальное перемещение
    // позволяет пройти по верхнему тоннелю, не проваливаясь в шахту раньше времени.
    if (cmd == 3) {
        while (py_m < 7 && grid[py_m+1][px_m] == 0) py_m++;
    }
    if (px_m == 8 && py_m == 4 && !minerWon) {
        minerWon = true;
        minerWins++;
        unlockAchievement(3);
        if (tankWins + minerWins >= 10) unlockAchievement(4);
        storeRecords(bestMazeSteps, tankWins, minerWins);
    }
}

void mainLoop() {
    SDL_Event e;
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    bool clicked = false;
    
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_MOUSEBUTTONDOWN) { clicked = true; mouseX = e.button.x; mouseY = e.button.y; }
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE && current != MENU) paused = !paused;
            if (!paused && current == MAZE) {
                if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_UP) tryMazeMove(0,-1);
                if (e.key.keysym.sym == SDLK_s || e.key.keysym.sym == SDLK_DOWN) tryMazeMove(0,1);
                if (e.key.keysym.sym == SDLK_a || e.key.keysym.sym == SDLK_LEFT) tryMazeMove(-1,0);
                if (e.key.keysym.sym == SDLK_d || e.key.keysym.sym == SDLK_RIGHT) tryMazeMove(1,0);
            }
            if (!paused && current == TANKS && e.key.keysym.sym == SDLK_SPACE && pcd <= 0) {
                bx.push_back(px_t); by.push_back(py_t);
                bdx.push_back(cos(pa_t)*6); bdy.push_back(sin(pa_t)*6); bulletOwner.push_back(0); pcd = 20;
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 250, 245, 235, 255); // Светлый фон
    SDL_RenderClear(renderer);

    if (paused && current != MENU) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 18, 16, 35, 225);
        SDL_Rect shade={0,0,W,H}; SDL_RenderFillRect(renderer,&shade);
        SDL_SetRenderDrawColor(renderer, 48, 42, 72, 255);
        SDL_Rect panel={38,155,324,190}; SDL_RenderFillRect(renderer,&panel);
        SDL_SetRenderDrawColor(renderer, 126, 224, 190, 255); SDL_RenderDrawRect(renderer,&panel);
        bool resume = mouseX>=70 && mouseX<=330 && mouseY>=235 && mouseY<=280;
        bool toMenu = mouseX>=70 && mouseX<=330 && mouseY>=295 && mouseY<=335;
        drawBtn(70,235,260,45,resume,clicked&&resume,{63,145,113,255});
        drawBtn(70,295,260,40,toMenu,clicked&&toMenu,{166,72,79,255});
        drawText("PAUSED", 146, 185, 6, {255,236,190,255});
        drawText("RESUME", 158, 251, 3, {255,244,214,255});
        drawText("MAIN MENU", 143, 307, 3, {255,244,214,255});
        if (clicked && resume) paused=false;
        if (clicked && toMenu) { paused=false; current=MENU; }
        SDL_RenderPresent(renderer);
        return;
    }

    if (current == MENU) {
        // Modern Pixel Art arcade menu: три игровые карточки.
        SDL_SetRenderDrawColor(renderer,9,13,39,255); SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer,28,35,76,255);
        for(int i=0;i<34;i++) {
            int sx=(i*73+19)%396, sy=(i*47+13)%496;
            SDL_Rect star={sx,sy,2+(i%3==0),2+(i%3==0)}; SDL_RenderFillRect(renderer,&star);
        }
        SDL_SetRenderDrawColor(renderer,18,29,66,255);
        SDL_Rect header={16,14,368,76}; SDL_RenderFillRect(renderer,&header);
        SDL_SetRenderDrawColor(renderer,55,115,190,255); SDL_RenderDrawRect(renderer,&header);
        drawText("MAZE ROBOT",112,29,5,{80,220,255,255});
        drawText("PIXEL ARCADE",140,58,2,{210,225,255,255});
        drawText("CHOOSE YOUR GAME",111,98,2,{175,190,230,255});
        if (achievementsOpen) {
            SDL_SetRenderDrawColor(renderer,22,29,65,255);
            SDL_Rect panel={25,112,350,330}; SDL_RenderFillRect(renderer,&panel);
            SDL_SetRenderDrawColor(renderer,80,210,255,255); SDL_RenderDrawRect(renderer,&panel);
            drawText("ACHIEVEMENTS",83,132,4,{100,225,255,255});
            const char* names[5]={"MAZE CLEAR","ALL MAZES","TANK ACE","DIAMOND","ARCADE PRO"};
            const char* hints[5]={"FIND AN EXIT","CLEAR 3 LEVELS","WIN TANKS","FIND THE GEM","10 TOTAL WINS"};
            SDL_Color gold={255,210,90,255}, muted={115,130,165,255};
            for(int i=0;i<5;i++) {
                bool earned=(achievementMask & (1<<i))!=0;
                SDL_SetRenderDrawColor(renderer,earned?45:38,earned?75:43,earned?65:70,255);
                SDL_Rect row={42,174+i*48,316,40}; SDL_RenderFillRect(renderer,&row);
                drawText(earned?"+":"-",53,183,3,earned?gold:muted);
                drawText(names[i],83,180,2,earned?gold:muted);
                drawText(hints[i],83,195,1,earned?SDL_Color{180,235,190,255}:muted);
            }
            bool close=mouseX>=130&&mouseX<=270&&mouseY>=390&&mouseY<=430;
            drawBtn(130,390,140,38,close,clicked&&close,{55,105,165,255});
            drawText("BACK",176,403,3,{255,255,255,255});
            if(clicked&&close) achievementsOpen=false;
        } else {
            bool awards=mouseX>=315&&mouseX<=390&&mouseY>=455&&mouseY<=495;
            drawBtn(315,455,75,35,awards,clicked&&awards,{70,85,145,255});
            drawText("AWARD",322,466,2,{255,255,255,255});
            if(clicked&&awards) achievementsOpen=true;

        bool h1=mouseX>=18 && mouseX<=382 && mouseY>=116 && mouseY<=220;
        bool h2=mouseX>=18 && mouseX<=382 && mouseY>=230 && mouseY<=334;
        bool h3=mouseX>=18 && mouseX<=382 && mouseY>=344 && mouseY<=448;
        SDL_Color blue={18,48,112,255}, greenCard={18,65,53,255}, orange={91,43,22,255};
        SDL_Color blueEdge={35,170,255,255}, greenEdge={105,230,75,255}, orangeEdge={255,145,45,255};
        SDL_Rect cards[3]={{18,116,364,104},{18,230,364,104},{18,344,364,104}};
        SDL_Color fills[3]={blue,greenCard,orange};
        SDL_Color edges[3]={blueEdge,greenEdge,orangeEdge};
        for(int i=0;i<3;i++) {
            SDL_SetRenderDrawColor(renderer,0,0,0,180);
            SDL_Rect shadow={cards[i].x+4,cards[i].y+5,cards[i].w,cards[i].h}; SDL_RenderFillRect(renderer,&shadow);
            SDL_SetRenderDrawColor(renderer,fills[i].r,fills[i].g,fills[i].b,255); SDL_RenderFillRect(renderer,&cards[i]);
            SDL_SetRenderDrawColor(renderer,edges[i].r,edges[i].g,edges[i].b,255); SDL_RenderDrawRect(renderer,&cards[i]);
            SDL_Rect art={cards[i].x+9,cards[i].y+9,104,86};
            SDL_SetRenderDrawColor(renderer,edges[i].r,edges[i].g,edges[i].b,55); SDL_RenderFillRect(renderer,&art);
        }
        drawPixelRobot(49,132,3);
        drawPixelTank(45,250,3);
        drawPixelMiner(45,361,3);
        drawText("MAZE",145,132,5,{220,245,255,255});
        drawText("FIND THE EXIT",145,160,2,{65,225,255,255});
        drawText("TANKS",145,246,5,{225,255,210,255});
        drawText("BATTLE ENEMIES",145,274,2,{150,245,105,255});
        drawText("MINER",145,360,5,{255,235,205,255});
        drawText("DIG AND COLLECT",145,388,2,{255,190,90,255});
        drawBtn(274,177,94,30,h1,clicked&&h1,{20,130,235,255});
        drawBtn(274,291,94,30,h2,clicked&&h2,{65,175,45,255});
        drawBtn(274,405,94,30,h3,clicked&&h3,{235,112,20,255});
        drawText("PLAY",292,186,3,{255,255,255,255});
        drawText("PLAY",292,300,3,{255,255,255,255});
        drawText("PLAY",292,414,3,{255,255,255,255});
        drawText("BEST MZ",54,466,2,{155,180,230,255});
        drawText(bestMazeSteps ? std::to_string(bestMazeSteps) : "--",118,466,2,{80,220,255,255});
        drawText("TANK",168,466,2,{155,180,230,255});
        drawText(std::to_string(tankWins),204,466,2,{150,245,105,255});
        drawText("MINER",238,466,2,{155,180,230,255});
        drawText(std::to_string(minerWins),286,466,2,{255,190,90,255});
        if(clicked) {
            if(h1) { current=MAZE; mazeLevel=0; resetMazeLevel(); }
            if(h2) { current=TANKS; resetTanks(); }
            if(h3) { current=MINER; resetMiner(); }
        }
        }
    } else {
        // Общая панель навигации
        bool hBack = mouseX>=10 && mouseX<=60 && mouseY>=10 && mouseY<=50;
        bool hPause = mouseX>=340 && mouseX<=390 && mouseY>=10 && mouseY<=50;
        drawBtn(10,10,50,40,hBack,clicked&&hBack,{91,82,117,255});
        drawArrow(5,0,2);
        drawBtn(340,10,50,40,hPause,clicked&&hPause,{91,82,117,255});
        drawText("II",355,22,4,{255,244,214,255});
        if (clicked && hBack) { current=MENU; paused=false; }
        if (clicked && hPause) paused=true;

        if (current == MAZE) {
            drawText("MAZE",165,25,4,{48,42,67,255});
            drawText("LEVEL",28,58,2,{91,82,117,255});
            drawText(std::to_string(mazeLevel+1)+"/3",83,58,2,{63,145,113,255});
            drawText("STEPS",285,58,2,{91,82,117,255});
            drawText(std::to_string(mazeSteps),345,58,2,{199,112,63,255});
            bool mazeResetHit=mouseX>=175 && mouseX<=235 && mouseY>=48 && mouseY<=76;
            drawBtn(175,48,60,28,mazeResetHit,clicked&&mazeResetHit,{166,72,79,255});
            drawText("RST",190,56,2,{255,244,214,255});
            if(clicked && mazeResetHit) resetMazeLevel();
            int cx = 60, cy = 82, cell = 56;
            for(int wy=0; wy<5; wy++) for(int wx=0; wx<5; wx++) {
                SDL_Rect tile={cx+wx*cell+2,cy+wy*cell+2,cell-4,cell-4};
                if (mazeMaps[mazeLevel][wy][wx]) {
                    SDL_SetRenderDrawColor(renderer,66,57,84,255); SDL_RenderFillRect(renderer,&tile);
                    SDL_SetRenderDrawColor(renderer,116,91,145,255); SDL_RenderDrawRect(renderer,&tile);
                } else {
                    SDL_SetRenderDrawColor(renderer,235,225,205,255); SDL_RenderFillRect(renderer,&tile);
                    SDL_SetRenderDrawColor(renderer,215,201,177,255); SDL_RenderDrawRect(renderer,&tile);
                }
            }
            SDL_SetRenderDrawColor(renderer,63,145,113,255);
            SDL_Rect exitR={cx+4*cell+10,cy+4*cell+10,cell-20,cell-20}; SDL_RenderFillRect(renderer,&exitR);
            drawText("E",cx+4*cell+23,cy+4*cell+24,3,{255,244,214,255});
            drawPixelRobot(cx+mx*cell+10,cy+my*cell+5,2);

            // D-Pad
            bool hu = (mouseX>=175 && mouseX<=225 && mouseY>=390 && mouseY<=440);
            bool hd = (mouseX>=175 && mouseX<=225 && mouseY>=450 && mouseY<=500);
            bool hl = (mouseX>=115 && mouseX<=165 && mouseY>=450 && mouseY<=500);
            bool hr = (mouseX>=235 && mouseX<=285 && mouseY>=450 && mouseY<=500);
            SDL_Color gray = {150, 150, 150, 255};
            
            drawBtn(175, 390, 50, 50, hu, clicked&&hu, gray); drawArrow(175, 390, 0);
            drawBtn(175, 450, 50, 50, hd, clicked&&hd, gray); drawArrow(175, 450, 1);
            drawBtn(115, 450, 50, 50, hl, clicked&&hl, gray); drawArrow(115, 450, 2);
            drawBtn(235, 450, 50, 50, hr, clicked&&hr, gray); drawArrow(235, 450, 3);
            
            if (clicked) {
                if (hu) tryMazeMove(0,-1);
                if (hd) tryMazeMove(0,1);
                if (hl) tryMazeMove(-1,0);
                if (hr) tryMazeMove(1,0);
            }
            if (mazeWon) {
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,18,16,35,220);
                SDL_Rect shade={0,0,W,H}; SDL_RenderFillRect(renderer,&shade);
                SDL_SetRenderDrawColor(renderer,48,42,72,255);
                SDL_Rect panel={35,175,330,165}; SDL_RenderFillRect(renderer,&panel);
                drawText(mazeAllWon ? "ALL CLEAR!" : "LEVEL CLEAR!",75,195,4,{126,224,190,255});
                drawText(("STEPS "+std::to_string(mazeSteps)).c_str(),145,235,3,{255,236,190,255});
                bool again=mouseX>=85 && mouseX<=315 && mouseY>=275 && mouseY<=325;
                drawBtn(85,275,230,50,again,clicked&&again,{63,145,113,255});
                drawText(mazeAllWon ? "PLAY AGAIN" : "NEXT LEVEL",112,292,3,{255,244,214,255});
                if(clicked && again) nextMazeLevel();
            }
        }
        else if (current == TANKS) {
            drawText("TANKS",165,25,4,{48,42,67,255});
            if (php > 0 && ehp > 0) {
                const Uint8* keys = SDL_GetKeyboardState(NULL);
                float nextX=px_t, nextY=py_t;
                if (keys[SDL_SCANCODE_W]) { nextY-=2; pa_t=-1.57f; }
                if (keys[SDL_SCANCODE_S]) { nextY+=2; pa_t=1.57f; }
                if (keys[SDL_SCANCODE_A]) { nextX-=2; pa_t=3.14f; }
                if (keys[SDL_SCANCODE_D]) { nextX+=2; pa_t=0.0f; }
                if (!tankBlocked(nextX,py_t)) px_t=nextX;
                if (!tankBlocked(px_t,nextY)) py_t=nextY;

                // Противник двигается к игроку, но проверяет оба направления
                // и выбирает свободную ось, если прямой путь закрыт стеной.
                int enemyMoveInterval = tanksWave == 1 ? 2 : 1;
                if (++enemyMoveTick >= enemyMoveInterval) {
                    enemyMoveTick = 0;
                    float dx = px_t - ex_t, dy = py_t - ey_t;
                    float stepX = dx > 0 ? 1.0f : (dx < 0 ? -1.0f : 0.0f);
                    float stepY = dy > 0 ? 1.0f : (dy < 0 ? -1.0f : 0.0f);
                    bool moved = false;
                    if (fabs(dx) >= fabs(dy) && stepX != 0 && !tankBlocked(ex_t+stepX,ey_t)) {
                        ex_t += stepX; moved = true;
                    }
                    if (!moved && stepY != 0 && !tankBlocked(ex_t,ey_t+stepY)) {
                        ey_t += stepY; moved = true;
                    }
                    if (!moved && stepX != 0 && !tankBlocked(ex_t+stepX,ey_t)) ex_t += stepX;
                }

                if (pcd > 0) pcd--; if (ecd > 0) ecd--;
                if (ecd <= 0) {
                    float ang = atan2(py_t - ey_t, px_t - ex_t);
                    bx.push_back(ex_t); by.push_back(ey_t); bdx.push_back(cos(ang)*4); bdy.push_back(sin(ang)*4); bulletOwner.push_back(1); ecd = 60;
                }

                for(int i=0; i<(int)bx.size(); i++) {
                    bx[i] += bdx[i]; by[i] += bdy[i];
                    bool removeBullet = bx[i]<0 || bx[i]>400 || by[i]<0 || by[i]>400;
                    SDL_Rect shot = {(int)bx[i]-2,(int)by[i]-2,4,4};
                    for (auto &wall : walls) if (SDL_HasIntersection(&shot,&wall)) removeBullet = true;
                    if (bulletOwner[i] == 1 && hypot(bx[i]-px_t, by[i]-py_t) < 15) { php--; removeBullet = true; }
                    if (bulletOwner[i] == 0 && ehp > 0 && hypot(bx[i]-ex_t, by[i]-ey_t) < 15) { ehp--; removeBullet = true; }
                    if (removeBullet) { bx.erase(bx.begin()+i); by.erase(by.begin()+i); bdx.erase(bdx.begin()+i); bdy.erase(bdy.begin()+i); bulletOwner.erase(bulletOwner.begin()+i); i--; }
                }
                // Три волны: каждая следующая меняет арену и усиливает противника.
                if (ehp <= 0 && tanksWave < 3) {
                    tanksWave++;
                    setTankArena(tanksWave-1);
                    ex_t = 350; ey_t = 50;
                    ehp = 2 + tanksWave - 1;
                    ecd = 45;
                    bx.clear(); by.clear(); bdx.clear(); bdy.clear(); bulletOwner.clear();
                }
            }

            SDL_SetRenderDrawColor(renderer, 48, 42, 67, 255);
            SDL_Rect hud={15,55,370,30}; SDL_RenderFillRect(renderer,&hud);
            drawText("WAVE",165,65,2,{255,236,190,255});
            drawText(std::to_string(tanksWave)+"/3",190,65,2,{126,224,190,255});
            drawText("YOU",25,65,2,{255,236,190,255});
            drawText(std::to_string(php),65,65,2,{126,224,190,255});
            drawText("ENEMY",245,65,2,{255,236,190,255});
            drawText(std::to_string(ehp),335,65,2,{255,145,145,255});
            SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
            for(auto& w : walls) SDL_RenderFillRect(renderer, &w);
            
            drawPixelTank((int)px_t-16,(int)py_t-16,2);
            if (ehp > 0) drawPixelTank((int)ex_t-16,(int)ey_t-16,2);
            for(int i=0; i<(int)bx.size(); i++) {
                if (bulletOwner[i] == 0) SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
                else SDL_SetRenderDrawColor(renderer, 255, 70, 70, 255);
                SDL_Rect b = {(int)bx[i]-3, (int)by[i]-3, 6, 6}; SDL_RenderFillRect(renderer, &b);
            }

            // D-Pad и Огонь
            bool hu = (mouseX>=175 && mouseX<=225 && mouseY>=390 && mouseY<=440);
            bool hd = (mouseX>=175 && mouseX<=225 && mouseY>=450 && mouseY<=500);
            bool hl = (mouseX>=115 && mouseX<=165 && mouseY>=450 && mouseY<=500);
            bool hr = (mouseX>=235 && mouseX<=285 && mouseY>=450 && mouseY<=500);
            bool hf = (mouseX>=300 && mouseX<=380 && mouseY>=420 && mouseY<=480);
            SDL_Color gray = {150, 150, 150, 255}, red = {220, 60, 60, 255};

            drawBtn(175, 390, 50, 50, hu, clicked&&hu, gray); drawArrow(175, 390, 0);
            drawBtn(175, 450, 50, 50, hd, clicked&&hd, gray); drawArrow(175, 450, 1);
            drawBtn(115, 450, 50, 50, hl, clicked&&hl, gray); drawArrow(115, 450, 2);
            drawBtn(235, 450, 50, 50, hr, clicked&&hr, gray); drawArrow(235, 450, 3);
            drawBtn(300, 420, 80, 60, hf, clicked&&hf, red);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_Rect fireIcon = {325, 435, 30, 30}; SDL_RenderDrawRect(renderer, &fireIcon); // Иконка выстрела

            if (clicked && php > 0) {
                if (hu && !tankBlocked(px_t,py_t-15)) { py_t -= 15; pa_t = -1.57; }
                if (hd && !tankBlocked(px_t,py_t+15)) { py_t += 15; pa_t = 1.57; }
                if (hl && !tankBlocked(px_t-15,py_t)) { px_t -= 15; pa_t = 3.14; }
                if (hr && !tankBlocked(px_t+15,py_t)) { px_t += 15; pa_t = 0; }
                if (hf && pcd <= 0) { bx.push_back(px_t); by.push_back(py_t); bdx.push_back(cos(pa_t)*6); bdy.push_back(sin(pa_t)*6); bulletOwner.push_back(0); pcd = 20; }
            }
            // Победа засчитывается только если игрок пережил финальный обмен выстрелами.
            if (php > 0 && ehp <= 0 && tanksWave >= 3 && !tankVictorySaved) {
                tankWins++;
                unlockAchievement(2);
                if (tankWins + minerWins >= 10) unlockAchievement(4);
                tankVictorySaved = true;
                storeRecords(bestMazeSteps, tankWins, minerWins);
            }
            if (php <= 0 || (ehp <= 0 && tanksWave >= 3)) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 18,16,35,220);
                SDL_Rect shade={0,0,W,H}; SDL_RenderFillRect(renderer,&shade);
                SDL_SetRenderDrawColor(renderer,48,42,72,255);
                SDL_Rect panel={35,175,330,150}; SDL_RenderFillRect(renderer,&panel);
                drawText(php<=0 ? "GAME OVER" : "YOU WIN", 105,205,5,{255,236,190,255});
                drawText(php<=0 ? "TRY AGAIN" : "3 WAVES CLEAR", 112,235,2,{126,224,190,255});
                bool retry=mouseX>=100 && mouseX<=300 && mouseY>=255 && mouseY<=305;
                drawBtn(100,255,200,50,retry,clicked&&retry,{63,145,113,255});
                drawText("RETRY",160,273,3,{255,244,214,255});
                if(clicked && retry) resetTanks();
            }
        }
        else if (current == MINER) {
            drawText("MINER",165,25,4,{48,42,67,255});
            int cell = 40;
            for(int i=0; i<8; i++) for(int j=0; j<10; j++) {
                if (grid[i][j] == 1) { SDL_SetRenderDrawColor(renderer, 180, 130, 80, 255); SDL_Rect r = {j*cell, i*cell+40, cell, cell}; SDL_RenderFillRect(renderer, &r); }
            }
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255); SDL_Rect g = {8*cell+10, 4*cell+50, cell-20, cell-20}; SDL_RenderFillRect(renderer, &g); // Алмаз
            drawPixelMiner(px_m*cell+4,py_m*cell+43,2); // Пиксельный шахтёр

            // Чипы программы
            for(size_t i=0; i<program.size(); i++) {
                SDL_SetRenderDrawColor(renderer, i==(size_t)execIdx ? 255 : 220, i==(size_t)execIdx ? 100 : 220, i==(size_t)execIdx ? 0 : 220, 255);
                SDL_Rect chip = {10 + (int)(i%10)*38, 420 + (int)(i/10)*38, 30, 30}; SDL_RenderFillRect(renderer, &chip);
            }

            // Кнопки команд
            bool hb1 = (mouseX>=10 && mouseX<=50 && mouseY>=370 && mouseY<=410);
            bool hb2 = (mouseX>=60 && mouseX<=100 && mouseY>=370 && mouseY<=410);
            bool hb3 = (mouseX>=110 && mouseX<=150 && mouseY>=370 && mouseY<=410);
            bool hb4 = (mouseX>=160 && mouseX<=200 && mouseY>=370 && mouseY<=410);
            bool hbr = (mouseX>=210 && mouseX<=290 && mouseY>=370 && mouseY<=410);
            bool hreset = (mouseX>=300 && mouseX<=390 && mouseY>=370 && mouseY<=410);
            SDL_Color gray = {150, 150, 150, 255}, green = {60, 180, 60, 255};

            drawBtn(10, 370, 40, 40, hb1, clicked&&hb1, gray); drawArrow(0, 350, 2);
            drawBtn(60, 370, 40, 40, hb2, clicked&&hb2, gray); drawArrow(50, 350, 3);
            drawBtn(110, 370, 40, 40, hb3, clicked&&hb3, gray); drawPickaxe(100, 360);
            drawBtn(160, 370, 40, 40, hb4, clicked&&hb4, gray); drawBlockIcon(150, 360);
            drawBtn(210, 370, 80, 40, hbr, clicked&&hbr, green); drawPlayIcon(215, 370);
            drawBtn(300, 370, 90, 40, hreset, clicked&&hreset, {166,72,79,255});
            drawText("RESET", 315, 384, 2, {255,244,214,255});

            if (clicked && hreset) resetMiner();
            else if (clicked && execIdx == -1 && !minerWon) {
                if (hb1) program.push_back(1);
                if (hb2) program.push_back(2);
                if (hb3) program.push_back(3);
                if (hb4) program.push_back(4);
                if (hbr && !program.empty()) { execIdx = 0; lastExec = SDL_GetTicks(); }
            }
            if (execIdx >= 0 && execIdx < (int)program.size() && !minerWon && SDL_GetTicks() - lastExec > 400) {
                handleMinerCommand(program[execIdx]);
                execIdx++;
                if (execIdx >= (int)program.size() || minerWon) execIdx = -1;
                lastExec = SDL_GetTicks();
            }
            if (minerWon) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 18,16,35,210);
                SDL_Rect ov = {0,0,400,500}; SDL_RenderFillRect(renderer, &ov);
                drawText("DIAMOND FOUND",70,220,4,{126,224,190,255});
                drawText("WELL DONE",125,260,4,{255,236,190,255});
                bool retry = mouseX>=100 && mouseX<=300 && mouseY>=310 && mouseY<=360;
                drawBtn(100,310,200,50,retry,clicked&&retry,{63,145,113,255});
                drawText("PLAY AGAIN",119,329,3,{255,244,214,255});
                if (clicked && retry) resetMiner();
            }
        }
    }
    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(W, H, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "MAZE Robot - Pixel Arcade");
    bestMazeSteps = loadBestMaze();
    achievementMask = loadAchievements();
    tankWins = loadTankWins();
    minerWins = loadMinerWins();
    resetMiner(); resetTanks();
    emscripten_set_main_loop(mainLoop, 0, 1);
    return 0;
}
