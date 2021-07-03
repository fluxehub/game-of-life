#include <SDL.h>
#include "game.h"
#include "draw.h"
#include "rle.h"

typedef enum { add, delete } ClickAction;

// Global vars for the program
// Cry about it
Game* game;

SDL_Window* window;
SDL_Renderer* renderer;
int width = 70;
int height = 50;

bool quit = false;
bool paused = true;
bool paused_before_click;
bool mouse_down = false;
bool should_draw;
ClickAction click_action;
double speed = 0.25;

void do_event(SDL_Event event) {
    switch (event.type) {
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_SPACE:
                    paused = !paused;
                    break;
                case SDLK_c:
                    game_clear(game);
                    should_draw = true;
                    break;
                case SDLK_LEFT:
                    // Speed limiter
                    if (speed != 1) {
                        speed += 0.05;
                    }
                    break;
                case SDLK_RIGHT:
                    // Speed limiter
                    if (speed != 0.05) {
                        speed -= 0.05;
                    }
                    break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN: {
            // Save paused state so we can unpause if necessary when done editing
            paused_before_click = paused;
            paused = true;
            mouse_down = true;

            // Convert mouse coords to grid coords
            int x = event.motion.x / CELL_SIZE;
            int y = event.motion.y / CELL_SIZE;

            // Set click action for further cells that are dragged onto
            // e.g. if a cell is added, any grid cells dragged over should also be added
            bool cell_exists = game_get_cell(game, x, y);
            click_action = cell_exists ? delete : add;

            // Toggle the cell state and draw the new board
            game_toggle_cell(game, x, y);
            should_draw = true;
            break;
        }
        case SDL_MOUSEMOTION:
            if (mouse_down) {
                // Convert mouse coords to grid coords
                int x = event.motion.x / CELL_SIZE;
                int y = event.motion.y / CELL_SIZE;

                // Only edit if the cell state matches the click action
                bool cell_exists = game_get_cell(game, x, y);

                if ((cell_exists && click_action == delete) || (!cell_exists && click_action == add)) {
                    game_toggle_cell(game, x, y);
                    should_draw = true;
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            // Restore pause state
            paused = paused_before_click;
            mouse_down = false;
            break;
        default:
            break;
    }
}

// Allows for drawing while resizing
int filter_event(void *data, SDL_Event* event) {
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        int set_width = event->window.data1;
        int set_height = event->window.data2;

        // Set the new grid width
        // Add one cell for cells that clip off-screen
        width = set_width / CELL_SIZE + 1;
        height = set_height / CELL_SIZE + 1;
        draw_game(renderer, game, width, height);

        return 0;
    }

    return 1;
}

int main() {
    char* test_file = "x = 36, y = 9, rule = B3/S23\n"
                      "24bo$22bobo$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o$2o8bo3bob2o4b\n"
                      "obo$10bo5bo7bo$11bo3bo$12b2o!";
    RLEGame rle_game;
    rle_parse(&rle_game, test_file);

    SDL_Log("Width: %d, Height: %d", rle_game.width, rle_game.height);

    if (!draw_initialize(&window, &renderer, width, height)) {
        return EXIT_FAILURE;
    }

    game = rle_game.game;

    u_int32_t tick_time = SDL_GetTicks();
    u_int32_t poll_time = SDL_GetTicks();

    // Draw initial board
    draw_game(renderer, game, width, height);

    SDL_SetEventFilter(filter_event, NULL);
    while (!quit) {
        // By default, don't draw when paused
        // Overridden by events that update the screen while paused
        // (e.g. placing cells)
        should_draw = !paused;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            do_event(event);
        }

        // If it's time to do a new tick, perform it
        if (SDL_GetTicks() - tick_time > (100 * speed) && !paused) {
            game_tick(game);
            tick_time = SDL_GetTicks();
        }

        if (should_draw) {
            draw_game(renderer, game, width, height);
        }

        // Poll at 60 Hz or else we will use 100% of the CPU lol
        u_int32_t elapsed = SDL_GetTicks() - poll_time;
        if (elapsed < 16) {
            SDL_Delay(16 - (SDL_GetTicks() - poll_time));
        }

        poll_time = SDL_GetTicks();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_free(game);

    return EXIT_SUCCESS;
}