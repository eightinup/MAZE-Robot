#include <SDL2/SDL.h>
#include <emscripten.h>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

const int W = 400;
const int H = 500;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

enum State { MENU, MAZE, TANKS, MINER };
State current = MENU;

// --- ЛАБИРИНТ ---
int mx = 1, my = 1;
bool mazeWon = false;
const int mazeWalls[3][3] = {{0,1,0},{0,1,0},{0,0,0}};
bool mazeCanMove(int x, int y) { return x >= 0 && x < 3 && y >= 0 && y < 3 && mazeWalls[y][x] == 0; }

// --- ТАНКИ ---
float px_t = 50, py_t = 50, pa_t = 0;
int php = 3;
float ex_t = 350, ey_t = 50;
int ehp = 2;
std::vector<float> bx, by, bdx, bdy;
std::vector<int> bulletOwner; // 0 — игрок, 1 — противник
int pcd = 0, ecd = 0;
SDL_Rect walls[] = {
    {150, 0, 20, 100}, {150, 150, 20, 100}, {250, 150, 20, 150},
    {0, 200, 100, 20}, {300, 200, 100, 20}, {150, 300, 100, 20}
};

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
void drawPixelRobot(int x, int y) {
    SDL_SetRenderDrawColor(renderer, 255, 174, 78, 255);
    SDL_Rect body={x+8,y+8,32,28}; SDL_RenderFillRect(renderer,&body);
    SDL_Rect earL={x+2,y+15,6,12}, earR={x+40,y+15,6,12};
    SDL_RenderFillRect(renderer,&earL); SDL_RenderFillRect(renderer,&earR);
    SDL_SetRenderDrawColor(renderer, 255, 236, 190, 255);
    SDL_Rect eyeL={x+15,y+16,6,7}, eyeR={x+27,y+16,6,7};
    SDL_RenderFillRect(renderer,&eyeL); SDL_RenderFillRect(renderer,&eyeR);
    SDL_SetRenderDrawColor(renderer, 48, 42, 67, 255);
    SDL_Rect pupilL={x+17,y+18,3,4}, pupilR={x+29,y+18,3,4};
    SDL_RenderFillRect(renderer,&pupilL); SDL_RenderFillRect(renderer,&pupilR);
    SDL_Rect mouth={x+19,y+29,10,3}; SDL_RenderFillRect(renderer,&mouth);
    SDL_SetRenderDrawColor(renderer, 112, 91, 130, 255);
    SDL_Rect antenna={x+22,y+1,4,7}; SDL_RenderFillRect(renderer,&antenna);
    SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
    SDL_Rect tip={x+21,y-2,6,4}; SDL_RenderFillRect(renderer,&tip);
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
    ex_t = 350; ey_t = 50; ehp = 2;
    bx.clear(); by.clear(); bdx.clear(); bdy.clear(); bulletOwner.clear();
    pcd = 0; ecd = 0;
}

void handleMinerCommand(int cmd) {
    if (cmd == 1 && px_m > 0 && grid[py_m][px_m-1] == 0) px_m--;
    else if (cmd == 2 && px_m < 9 && grid[py_m][px_m+1] == 0) px_m++;
    else if (cmd == 3 && py_m < 7) { grid[py_m+1][px_m] = 0; py_m++; }
    else if (cmd == 4 && px_m < 9 && py_m < 7 && grid[py_m+1][px_m+1] == 0) grid[py_m+1][px_m+1] = 1;
    while (py_m < 7 && grid[py_m+1][px_m] == 0) py_m++; // Гравитация
    if (px_m == 8 && py_m == 4) minerWon = true;
}

void mainLoop() {
    SDL_Event e;
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    bool clicked = false;
    
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_MOUSEBUTTONDOWN) { clicked = true; mouseX = e.button.x; mouseY = e.button.y; }
        if (e.type == SDL_KEYDOWN) {
            if (current == MAZE) {
                if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_UP) { if(mazeCanMove(mx,my-1)) my--; }
                if (e.key.keysym.sym == SDLK_s || e.key.keysym.sym == SDLK_DOWN) { if(mazeCanMove(mx,my+1)) my++; }
                if (e.key.keysym.sym == SDLK_a || e.key.keysym.sym == SDLK_LEFT) { if(mazeCanMove(mx-1,my)) mx--; }
                if (e.key.keysym.sym == SDLK_d || e.key.keysym.sym == SDLK_RIGHT) { if(mazeCanMove(mx+1,my)) mx++; }
                if (mx==2 && my==2) mazeWon = true;
            }
            if (current == TANKS && e.key.keysym.sym == SDLK_SPACE && pcd <= 0) {
                bx.push_back(px_t); by.push_back(py_t);
                bdx.push_back(cos(pa_t)*6); bdy.push_back(sin(pa_t)*6); bulletOwner.push_back(0); pcd = 20;
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 250, 245, 235, 255); // Светлый фон
    SDL_RenderClear(renderer);

    if (current == MENU) {
        // Звёздный фон в ограниченной палитре
        SDL_SetRenderDrawColor(renderer, 23, 21, 43, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 66, 57, 91, 255);
        for (int i=0; i<18; i++) {
            int sx=(i*73+19)%390, sy=(i*47+13)%490;
            SDL_Rect star={sx,sy,2+(i%2),2+(i%2)}; SDL_RenderFillRect(renderer,&star);
        }
        SDL_SetRenderDrawColor(renderer, 48, 42, 72, 255);
        SDL_Rect header={18,18,364,112}; SDL_RenderFillRect(renderer,&header);
        SDL_SetRenderDrawColor(renderer, 116, 91, 145, 255); SDL_RenderDrawRect(renderer,&header);
        drawPixelRobot(32,48);
        drawText("MAZE ROBOT", 100, 48, 5, {255,236,190,255});
        drawText("PIXEL ARCADE", 103, 82, 3, {126,224,190,255});
        drawText("SELECT GAME", 132, 145, 3, {207,195,226,255});

        bool h1 = mouseX>=30 && mouseX<=370 && mouseY>=180 && mouseY<=260;
        bool h2 = mouseX>=30 && mouseX<=370 && mouseY>=275 && mouseY<=355;
        bool h3 = mouseX>=30 && mouseX<=370 && mouseY>=370 && mouseY<=450;
        drawBtn(30,180,340,80,h1,clicked&&h1,{199,112,63,255});
        drawBtn(30,275,340,80,h2,clicked&&h2,{166,72,79,255});
        drawBtn(30,370,340,80,h3,clicked&&h3,{63,145,113,255});
        drawText("01  MAZE", 105, 204, 5, {255,244,214,255});
        drawText("02  TANKS", 105, 299, 5, {255,244,214,255});
        drawText("03  MINER", 105, 394, 5, {255,244,214,255});
        drawText("WASD / ARROWS  -  TOUCH", 92, 468, 2, {170,162,199,255});
        if (clicked) {
            if (h1) { current = MAZE; mx=0; my=0; mazeWon=false; }
            if (h2) { current = TANKS; resetTanks(); }
            if (h3) { current = MINER; resetMiner(); }
        }
    } else {
        // Кнопка "Назад" (стрелочка)
        bool hBack = (mouseX>=10 && mouseX<=60 && mouseY>=10 && mouseY<=50);
        drawBtn(10, 10, 50, 40, hBack, clicked && hBack, {150, 150, 150, 255});
        drawArrow(5, 0, 2); // Стрелка влево

        if (clicked && hBack) current = MENU;

        if (current == MAZE) {
            int cx = 50, cy = 80, cell = 100;
            SDL_SetRenderDrawColor(renderer, 255, 215, 176, 255);
            for(int wy=0; wy<3; wy++) for(int wx=0; wx<3; wx++) {
                if (mazeWalls[wy][wx]) { SDL_SetRenderDrawColor(renderer, 66, 57, 84, 255); SDL_Rect wall = {cx+wx*cell+5, cy+wy*cell+5, cell-10, cell-10}; SDL_RenderFillRect(renderer, &wall); }
            }
            for(int i=0; i<=3; i++) {
                SDL_RenderDrawLine(renderer, cx+i*cell, cy, cx+i*cell, cy+300);
                SDL_RenderDrawLine(renderer, cx, cy+i*cell, cx+300, cy+i*cell);
            }
            if (mazeWon) SDL_SetRenderDrawColor(renderer, 46, 139, 46, 255);
            else SDL_SetRenderDrawColor(renderer, 180, 230, 180, 255);
            SDL_Rect exitR = {cx+2*cell+10, cy+2*cell+10, cell-20, cell-20}; SDL_RenderFillRect(renderer, &exitR);
            
            SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255);
            SDL_Rect robot = {cx+mx*cell+10, cy+my*cell+10, cell-20, cell-20}; SDL_RenderFillRect(renderer, &robot);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100); SDL_Rect eye1 = {cx+mx*cell+20, cy+my*cell+25, 8, 8}, eye2 = {cx+mx*cell+45, cy+my*cell+25, 8, 8};
            SDL_RenderFillRect(renderer, &eye1); SDL_RenderFillRect(renderer, &eye2); // Глазки робота

            // D-Pad
            bool hu = (mouseX>=175 && mouseX<=225 && mouseY>=400 && mouseY<=450);
            bool hd = (mouseX>=175 && mouseX<=225 && mouseY>=460 && mouseY<=510);
            bool hl = (mouseX>=115 && mouseX<=165 && mouseY>=460 && mouseY<=510);
            bool hr = (mouseX>=235 && mouseX<=285 && mouseY>=460 && mouseY<=510);
            SDL_Color gray = {150, 150, 150, 255};
            
            drawBtn(175, 400, 50, 50, hu, clicked&&hu, gray); drawArrow(175, 400, 0);
            drawBtn(175, 460, 50, 50, hd, clicked&&hd, gray); drawArrow(175, 460, 1);
            drawBtn(115, 460, 50, 50, hl, clicked&&hl, gray); drawArrow(115, 460, 2);
            drawBtn(235, 460, 50, 50, hr, clicked&&hr, gray); drawArrow(235, 460, 3);
            
            if (clicked) {
                if (hu) { if(mazeCanMove(mx,my-1)) my--; }
                if (hd) { if(mazeCanMove(mx,my+1)) my++; }
                if (hl) { if(mazeCanMove(mx-1,my)) mx--; }
                if (hr) { if(mazeCanMove(mx+1,my)) mx++; }
                if (mx==2 && my==2) mazeWon = true;
            }
        }
        else if (current == TANKS) {
            if (php > 0 && ehp > 0) {
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_W]) { py_t -= 2; pa_t = -1.57; }
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_S]) { py_t += 2; pa_t = 1.57; }
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A]) { px_t -= 2; pa_t = 3.14; }
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_D]) { px_t += 2; pa_t = 0; }
                if (px_t < 15) px_t = 15; if (px_t > 385) px_t = 385;
                if (py_t < 15) py_t = 15; if (py_t > 385) py_t = 385;

                if (ex_t < px_t) ex_t += 1; else ex_t -= 1;
                if (ey_t < py_t) ey_t += 1; else ey_t -= 1;

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
            }

            SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
            for(auto& w : walls) SDL_RenderFillRect(renderer, &w);
            
            SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255); SDL_Rect p = {(int)px_t-10, (int)py_t-10, 20, 20}; SDL_RenderFillRect(renderer, &p);
            if (ehp > 0) { SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255); SDL_Rect en = {(int)ex_t-10, (int)ey_t-10, 20, 20}; SDL_RenderFillRect(renderer, &en); }
            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
            for(int i=0; i<(int)bx.size(); i++) { SDL_Rect b = {(int)bx[i]-3, (int)by[i]-3, 6, 6}; SDL_RenderFillRect(renderer, &b); }

            // D-Pad и Огонь
            bool hu = (mouseX>=175 && mouseX<=225 && mouseY>=400 && mouseY<=450);
            bool hd = (mouseX>=175 && mouseX<=225 && mouseY>=460 && mouseY<=510);
            bool hl = (mouseX>=115 && mouseX<=165 && mouseY>=460 && mouseY<=510);
            bool hr = (mouseX>=235 && mouseX<=285 && mouseY>=460 && mouseY<=510);
            bool hf = (mouseX>=300 && mouseX<=380 && mouseY>=420 && mouseY<=480);
            SDL_Color gray = {150, 150, 150, 255}, red = {220, 60, 60, 255};

            drawBtn(175, 400, 50, 50, hu, clicked&&hu, gray); drawArrow(175, 400, 0);
            drawBtn(175, 460, 50, 50, hd, clicked&&hd, gray); drawArrow(175, 460, 1);
            drawBtn(115, 460, 50, 50, hl, clicked&&hl, gray); drawArrow(115, 460, 2);
            drawBtn(235, 460, 50, 50, hr, clicked&&hr, gray); drawArrow(235, 460, 3);
            drawBtn(300, 420, 80, 60, hf, clicked&&hf, red);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_Rect fireIcon = {325, 435, 30, 30}; SDL_RenderDrawRect(renderer, &fireIcon); // Иконка выстрела

            if (clicked && php > 0) {
                if (hu) { py_t -= 15; pa_t = -1.57; }
                if (hd) { py_t += 15; pa_t = 1.57; }
                if (hl) { px_t -= 15; pa_t = 3.14; }
                if (hr) { px_t += 15; pa_t = 0; }
                if (hf && pcd <= 0) { bx.push_back(px_t); by.push_back(py_t); bdx.push_back(cos(pa_t)*6); bdy.push_back(sin(pa_t)*6); bulletOwner.push_back(0); pcd = 20; }
            }
        }
        else if (current == MINER) {
            int cell = 40;
            for(int i=0; i<8; i++) for(int j=0; j<10; j++) {
                if (grid[i][j] == 1) { SDL_SetRenderDrawColor(renderer, 180, 130, 80, 255); SDL_Rect r = {j*cell, i*cell+40, cell, cell}; SDL_RenderFillRect(renderer, &r); }
            }
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255); SDL_Rect g = {8*cell+10, 4*cell+50, cell-20, cell-20}; SDL_RenderFillRect(renderer, &g); // Алмаз
            SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255); SDL_Rect p = {px_m*cell+5, py_m*cell+45, cell-10, cell-10}; SDL_RenderFillRect(renderer, &p); // Игрок

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
            SDL_Color gray = {150, 150, 150, 255}, green = {60, 180, 60, 255};

            drawBtn(10, 370, 40, 40, hb1, clicked&&hb1, gray); drawArrow(0, 350, 2);
            drawBtn(60, 370, 40, 40, hb2, clicked&&hb2, gray); drawArrow(50, 350, 3);
            drawBtn(110, 370, 40, 40, hb3, clicked&&hb3, gray); drawPickaxe(100, 360);
            drawBtn(160, 370, 40, 40, hb4, clicked&&hb4, gray); drawBlockIcon(150, 360);
            drawBtn(210, 370, 80, 40, hbr, clicked&&hbr, green); drawPlayIcon(215, 370);

            if (clicked && execIdx == -1) {
                if (hb1) program.push_back(1);
                if (hb2) program.push_back(2);
                if (hb3) program.push_back(3);
                if (hb4) program.push_back(4);
                if (hbr && !program.empty()) execIdx = 0;
            }
            if (execIdx >= 0 && SDL_GetTicks() - lastExec > 400) {
                handleMinerCommand(program[execIdx]);
                execIdx++; if (execIdx >= (int)program.size()) execIdx = -1; lastExec = SDL_GetTicks();
            }
            if (minerWon) { SDL_SetRenderDrawColor(renderer, 0, 200, 100, 100); SDL_Rect ov = {0,0,400,500}; SDL_RenderFillRect(renderer, &ov); }
        }
    }
    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(W, H, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "MAZE Robot - Pixel Arcade");
    resetMiner(); resetTanks();
    emscripten_set_main_loop(mainLoop, 0, 1);
    return 0;
}
