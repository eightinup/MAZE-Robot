#include <SDL2/SDL.h>
#include <emscripten.h>

const int SCREEN_W = 400;
const int SCREEN_H = 400;
const int CELL = 100;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

int px = 0, py = 0;
bool won = false;

void render() {
    // фон
    SDL_SetRenderDrawColor(renderer, 255, 243, 230, 255);
    SDL_RenderClear(renderer);

    // сетка
    SDL_SetRenderDrawColor(renderer, 255, 215, 176, 255);
    for (int i = 0; i <= 3; i++) {
        SDL_RenderDrawLine(renderer, i * CELL, 0, i * CELL, SCREEN_H);
        SDL_RenderDrawLine(renderer, 0, i * CELL, SCREEN_W, i * CELL);
    }

    // выход
    if (won)
        SDL_SetRenderDrawColor(renderer, 46, 139, 46, 255);
    else
        SDL_SetRenderDrawColor(renderer, 212, 245, 212, 255);
    SDL_Rect exit = { 2 * CELL + 10, 2 * CELL + 10, CELL - 20, CELL - 20 };
    SDL_RenderFillRect(renderer, &exit);

    // робот
    SDL_SetRenderDrawColor(renderer, 255, 154, 60, 255);
    SDL_Rect robot = { px * CELL + 10, py * CELL + 10, CELL - 20, CELL - 20 };
    SDL_RenderFillRect(renderer, &robot);

    SDL_RenderPresent(renderer);
}

void move(int dx, int dy) {
    if (won) return;
    int nx = px + dx;
    int ny = py + dy;
    if (nx >= 0 && nx < 3 && ny >= 0 && ny < 3) {
        px = nx;
        py = ny;
        if (px == 2 && py == 2) won = true;
    }
}

void mainLoop() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_w: case SDLK_UP:    move(0, -1); break;
                case SDLK_s: case SDLK_DOWN:  move(0,  1); break;
                case SDLK_a: case SDLK_LEFT:  move(-1, 0); break;
                case SDLK_d: case SDLK_RIGHT: move( 1, 0); break;
            }
        }
    }
    render();
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(SCREEN_W, SCREEN_H, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "Maze");

    emscripten_set_main_loop(mainLoop, 0, 1);
    return 0;
}
