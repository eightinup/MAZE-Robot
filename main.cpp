#include <SDL2/SDL.h>
#include <emscripten.h>
#include <vector>
#include <cmath>

const int W = 400;
const int H = 500; // Высота больше, чтобы влезли кнопки

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

enum State { MENU, MAZE, TANKS, MINER };
State current = MENU;

// --- ПЕРЕМЕННЫЕ ДЛЯ ЛАБИРИНТА ---
int mx = 1, my = 1;
bool mazeWon = false;

// --- ПЕРЕМЕННЫЕ ДЛЯ ТАНКОВ ---
float px_t = 50, py_t = 50, pa_t = 0;
int php = 3;
float ex_t = 350, ey_t = 50;
int ehp = 2;
std::vector<float> bx, by, bdx, bdy;
int pcd = 0, ecd = 0;
SDL_Rect walls[] = {
    {150, 0, 20, 100}, {150, 150, 20, 100}, {250, 150, 20, 150},
    {0, 200, 100, 20}, {300, 200, 100, 20}, {150, 300, 100, 20}
};

// --- ПЕРЕМЕННЫЕ ДЛЯ ШАХТЁРА ---
int grid[8][10];
int px_m = 1, py_m = 4;
std::vector<int> program;
int execIdx = -1;
Uint32 lastExec = 0;
bool minerWon = false;

void resetMiner() {
    int map[8][10] = {
        {0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},
        {1,1,1,0,1,1,1,1,1,1},
        {1,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1}
    };
    for(int i=0; i<8; i++) for(int j=0; j<10; j++) grid[i][j] = map[i][j];
    px_m = 1; py_m = 4;
    program.clear();
    execIdx = -1;
    minerWon = false;
}

void resetTanks() {
    px_t = 50; py_t = 50; pa_t = 0; php = 3;
    ex_t = 350; ey_t = 50; ehp = 2;
    bx.clear(); by.clear(); bdx.clear(); bdy.clear();
    pcd = 0; ecd = 0;
}

void handleMinerCommand(int cmd) {
    if (cmd == 1 && px_m > 0 && grid[py_m][px_m-1] == 0) px_m--;
    else if (cmd == 2 && px_m < 9 && grid[py_m][px_m+1] == 0) px_m++;
    else if (cmd == 3 && py_m < 7) { grid[py_m+1][px_m] = 0; py_m++; }
    else if (cmd == 4 && px_m < 9 && grid[py_m+1][px_m+1] == 0) grid[py_m+1][px_m+1] = 1;
    
    // Гравитация (если под ногами пусто - падаем)
    while (py_m < 7 && grid[py_m+1][px_m] == 0) py_m++;
    
    // Победа (дошли до алмаза)
    if (px_m == 8 && py_m == 4) minerWon = true;
}

void mainLoop() {
    SDL_Event e;
    int mouseX, mouseY;
    Uint32 mouse = SDL_GetMouseState(&mouseX, &mouseY);
    bool clicked = false;
    
    // Обработка событий
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_MOUSEBUTTONDOWN) { clicked = true; mouseX = e.button.x; mouseY = e.button.y; }
        if (e.type == SDL_KEYDOWN) {
            if (current == MAZE) {
                if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_UP) { if(my>0) my--; }
                if (e.key.keysym.sym == SDLK_s || e.key.keysym.sym == SDLK_DOWN) { if(my<2) my++; }
                if (e.key.keysym.sym == SDLK_a || e.key.keysym.sym == SDLK_LEFT) { if(mx>0) mx--; }
                if (e.key.keysym.sym == SDLK_d || e.key.keysym.sym == SDLK_RIGHT) { if(mx<2) mx++; }
                if (mx==2 && my==2) mazeWon = true;
            }
            if (current == TANKS && e.key.keysym.sym == SDLK_SPACE && pcd <= 0) {
                bx.push_back(px_t); by.push_back(py_t);
                bdx.push_back(cos(pa_t)*6); bdy.push_back(sin(pa_t)*6);
                pcd = 20;
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 243, 230, 255);
    SDL_RenderClear(renderer);

    if (current == MENU) {
        // 3 кнопки меню
        SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255); // Оранжевая (Лабиринт)
        SDL_Rect r1 = {50, 50, 300, 100}; SDL_RenderFillRect(renderer, &r1);
        
        SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255); // Красная (Танки)
        SDL_Rect r2 = {50, 180, 300, 100}; SDL_RenderFillRect(renderer, &r2);
        
        SDL_SetRenderDrawColor(renderer, 60, 180, 60, 255); // Зелёная (Шахтёр)
        SDL_Rect r3 = {50, 310, 300, 100}; SDL_RenderFillRect(renderer, &r3);

        if (clicked) {
            if (mouseY >= 50 && mouseY <= 150) { current = MAZE; mx=1; my=1; mazeWon=false; }
            if (mouseY >= 180 && mouseY <= 280) { current = TANKS; resetTanks(); }
            if (mouseY >= 310 && mouseY <= 410) { current = MINER; resetMiner(); }
        }
    } else {
        // Кнопка "Назад" (серая в левом верхнем углу)
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_Rect back = {10, 10, 60, 40}; SDL_RenderFillRect(renderer, &back);
        if (clicked && mouseX >= 10 && mouseX <= 70 && mouseY >= 10 && mouseY <= 50) current = MENU;

        if (current == MAZE) {
            int cx = 50, cy = 80, cell = 100;
            SDL_SetRenderDrawColor(renderer, 255, 215, 176, 255);
            for(int i=0; i<=3; i++) {
                SDL_RenderDrawLine(renderer, cx+i*cell, cy, cx+i*cell, cy+300);
                SDL_RenderDrawLine(renderer, cx, cy+i*cell, cx+300, cy+i*cell);
            }
            SDL_SetRenderDrawColor(renderer, mazeWon ? 46 : 212, mazeWon ? 139 : 245, mazeWon ? 46 : 212, 255);
            SDL_Rect exitR = {cx+2*cell+10, cy+2*cell+10, cell-20, cell-20}; SDL_RenderFillRect(renderer, &exitR);
            SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255);
            SDL_Rect robot = {cx+mx*cell+10, cy+my*cell+10, cell-20, cell-20}; SDL_RenderFillRect(renderer, &robot);
            
            // Кнопки управления для телефона
            SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            SDL_Rect up = {175, 400, 50, 50}; SDL_RenderFillRect(renderer, &up);
            SDL_Rect dn = {175, 460, 50, 50}; SDL_RenderFillRect(renderer, &dn);
            SDL_Rect lf = {115, 460, 50, 50}; SDL_RenderFillRect(renderer, &lf);
            SDL_Rect rt = {235, 460, 50, 50}; SDL_RenderFillRect(renderer, &rt);
            
            if (clicked) {
                if (mouseX>=175 && mouseX<=225 && mouseY>=400 && mouseY<=450) { if(my>0) my--; }
                if (mouseX>=175 && mouseX<=225 && mouseY>=460 && mouseY<=510) { if(my<2) my++; }
                if (mouseX>=115 && mouseX<=165 && mouseY>=460 && mouseY<=510) { if(mx>0) mx--; }
                if (mouseX>=235 && mouseX<=285 && mouseY>=460 && mouseY<=510) { if(mx<2) mx++; }
                if (mx==2 && my==2) mazeWon = true;
            }
        }
        else if (current == TANKS) {
            if (php > 0 && ehp > 0) {
                // Логика движения
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_W]) { py_t -= 2; pa_t = -1.57; }
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_S]) { py_t += 2; pa_t = 1.57; }
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A]) { px_t -= 2; pa_t = 3.14; }
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_D]) { px_t += 2; pa_t = 0; }
                if (px_t < 15) px_t = 15; if (px_t > 385) px_t = 385;
                if (py_t < 15) py_t = 15; if (py_t > 385) py_t = 385;

                // Враг
                if (ex_t < px_t) ex_t += 1; else ex_t -= 1;
                if (ey_t < py_t) ey_t += 1; else ey_t -= 1;

                if (pcd > 0) pcd--;
                if (ecd > 0) ecd--;
                if (ecd <= 0) {
                    float ang = atan2(py_t - ey_t, px_t - ex_t);
                    bx.push_back(ex_t); by.push_back(ey_t);
                    bdx.push_back(cos(ang)*4); bdy.push_back(sin(ang)*4);
                    ecd = 60;
                }

                // Пули
                for(int i=0; i<(int)bx.size(); i++) {
                    bx[i] += bdx[i]; by[i] += bdy[i];
                    if (bx[i]<0||bx[i]>400||by[i]<0||by[i]>400) {
                        bx.erase(bx.begin()+i); by.erase(by.begin()+i);
                        bdx.erase(bdx.begin()+i); bdy.erase(bdy.begin()+i); i--; continue;
                    }
                    if (hypot(bx[i]-px_t, by[i]-py_t) < 15) { php--; bx.erase(bx.begin()+i); by.erase(by.begin()+i); bdx.erase(bdx.begin()+i); bdy.erase(bdy.begin()+i); i--; }
                    else if (hypot(bx[i]-ex_t, by[i]-ey_t) < 15) { ehp--; bx.erase(bx.begin()+i); by.erase(by.begin()+i); bdx.erase(bdx.begin()+i); bdy.erase(bdy.begin()+i); i--; }
                }
            }

            // Отрисовка танков
            SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
            for(auto& w : walls) SDL_RenderFillRect(renderer, &w);
            SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255);
            SDL_Rect p = {px_t-10, py_t-10, 20, 20}; SDL_RenderFillRect(renderer, &p);
            if (ehp > 0) {
                SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255);
                SDL_Rect en = {ex_t-10, ey_t-10, 20, 20}; SDL_RenderFillRect(renderer, &en);
            }
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            for(int i=0; i<(int)bx.size(); i++) {
                SDL_Rect b = {bx[i]-3, by[i]-3, 6, 6}; SDL_RenderFillRect(renderer, &b);
            }

            // Управление танком (D-Pad и Огонь)
            SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            SDL_Rect up = {175, 400, 50, 50}; SDL_RenderFillRect(renderer, &up);
            SDL_Rect dn = {175, 460, 50, 50}; SDL_RenderFillRect(renderer, &dn);
            SDL_Rect lf = {115, 460, 50, 50}; SDL_RenderFillRect(renderer, &lf);
            SDL_Rect rt = {235, 460, 50, 50}; SDL_RenderFillRect(renderer, &rt);
            SDL_Rect fire = {300, 420, 80, 60}; SDL_RenderFillRect(renderer, &fire);

            if (clicked && php > 0) {
                if (mouseX>=175 && mouseX<=225 && mouseY>=400 && mouseY<=450) { py_t -= 15; pa_t = -1.57; }
                if (mouseX>=175 && mouseX<=225 && mouseY>=460 && mouseY<=510) { py_t += 15; pa_t = 1.57; }
                if (mouseX>=115 && mouseX<=165 && mouseY>=460 && mouseY<=510) { px_t -= 15; pa_t = 3.14; }
                if (mouseX>=235 && mouseX<=285 && mouseY>=460 && mouseY<=510) { px_t += 15; pa_t = 0; }
                if (mouseX>=300 && mouseX<=380 && mouseY>=420 && mouseY<=480 && pcd <= 0) {
                    bx.push_back(px_t); by.push_back(py_t);
                    bdx.push_back(cos(pa_t)*6); bdy.push_back(sin(pa_t)*6);
                    pcd = 20;
                }
            }
        }
        else if (current == MINER) {
            int cell = 40;
            for(int i=0; i<8; i++) for(int j=0; j<10; j++) {
                if (grid[i][j] == 1) {
                    SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
                    SDL_Rect r = {j*cell, i*cell+40, cell, cell}; SDL_RenderFillRect(renderer, &r);
                }
            }
            
            // Алмаз
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
            SDL_Rect g = {8*cell+10, 4*cell+50, cell-20, cell-20}; SDL_RenderFillRect(renderer, &g);
            
            // Игрок
            SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255);
            SDL_Rect p = {px_m*cell+5, py_m*cell+45, cell-10, cell-10}; SDL_RenderFillRect(renderer, &p);

            // Очередь команд
            for(size_t i=0; i<program.size(); i++) {
                SDL_SetRenderDrawColor(renderer, i==(size_t)execIdx ? 255 : 200, i==(size_t)execIdx ? 100 : 200, 0, 255);
                SDL_Rect chip = {10 + (int)(i%10)*38, 420 + (int)(i/10)*38, 30, 30}; SDL_RenderFillRect(renderer, &chip);
            }

            // Кнопки команд: Влево, Вправо, Копать, Поставить, Пуск
            SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            SDL_Rect b1={10, 370, 40, 40}, b2={60, 370, 40, 40}, b3={110, 370, 40, 40}, b4={160, 370, 40, 40}, brun={210, 370, 80, 40};
            SDL_RenderFillRect(renderer, &b1); SDL_RenderFillRect(renderer, &b2);
            SDL_RenderFillRect(renderer, &b3); SDL_RenderFillRect(renderer, &b4);
            SDL_SetRenderDrawColor(renderer, 60, 180, 60, 255); SDL_RenderFillRect(renderer, &brun);
            
            if (clicked) {
                if (execIdx == -1) {
                    if (mouseX>=10 && mouseX<=50 && mouseY>=370 && mouseY<=410) program.push_back(1);
                    if (mouseX>=60 && mouseX<=100 && mouseY>=370 && mouseY<=410) program.push_back(2);
                    if (mouseX>=110 && mouseX<=150 && mouseY>=370 && mouseY<=410) program.push_back(3);
                    if (mouseX>=160 && mouseX<=200 && mouseY>=370 && mouseY<=410) program.push_back(4);
                    if (mouseX>=210 && mouseX<=290 && mouseY>=370 && mouseY<=410 && !program.empty()) execIdx = 0;
                }
            }
            // Выполнение команд
            if (execIdx >= 0 && SDL_GetTicks() - lastExec > 400) {
                handleMinerCommand(program[execIdx]);
                execIdx++;
                if (execIdx >= (int)program.size()) execIdx = -1;
                lastExec = SDL_GetTicks();
            }
            
            if (minerWon) {
                SDL_SetRenderDrawColor(renderer, 46, 139, 46, 255);
                SDL_Rect ov = {0,0,400,400}; SDL_RenderFillRect(renderer, &ov);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(W, H, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "C++ Game Pack");
    resetMiner();
    resetTanks();
    emscripten_set_main_loop(mainLoop, 0, 1);
    return 0;
}
