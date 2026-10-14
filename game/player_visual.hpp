#pragma once
#include <SDL.h>
#include <cmath>

// Visual skeleton only. Movement and collisions still use the existing body box.
inline void drawPlayer(SDL_Renderer* renderer, float x, float y, bool grounded,
                       float stepDistance, int movement, int scale) {
    const float swing = grounded && movement != 0 ? std::sin(stepDistance * 6.2831853f) : 0;
    auto limb = [&](float ax, float ay, float bx, float by, float cx, float cy) {
        const int aX = static_cast<int>((x + ax) * scale), aY = static_cast<int>((y + ay) * scale);
        const int bX = static_cast<int>((x + bx) * scale), bY = static_cast<int>((y + by) * scale);
        const int cX = static_cast<int>((x + cx) * scale), cY = static_cast<int>((y + cy) * scale);
        for (int offset = 0; offset < 2; ++offset) {
            SDL_RenderDrawLine(renderer, aX + offset, aY, bX + offset, bY);
            SDL_RenderDrawLine(renderer, bX + offset, bY, cX + offset, cY);
        }
    };
    const float footY = grounded ? 2.9f : 2.65f;
    SDL_SetRenderDrawColor(renderer, 55, 91, 144, 255);
    limb(0.9f, 1.7f, 0.65f + swing * 0.2f, 2.3f, 0.5f + swing * 0.35f, footY);
    SDL_SetRenderDrawColor(renderer, 81, 131, 198, 255);
    limb(1.1f, 1.7f, 1.3f - swing * 0.2f, 2.3f, 1.4f - swing * 0.35f, footY);
    SDL_SetRenderDrawColor(renderer, 230, 169, 112, 255);
    limb(0.8f, 0.95f, 0.35f, 1.35f - swing * 0.15f, 0.25f, 1.8f - swing * 0.25f);
    SDL_SetRenderDrawColor(renderer, 255, 218, 148, 255);
    limb(1.2f, 0.95f, 1.6f, 1.35f + swing * 0.15f, 1.7f, 1.8f + swing * 0.25f);
    SDL_SetRenderDrawColor(renderer, 201, 105, 65, 255);
    SDL_Rect torso{static_cast<int>((x + 0.6f) * scale), static_cast<int>((y + 0.9f) * scale),
                   static_cast<int>(0.85f * scale), static_cast<int>(0.85f * scale)};
    SDL_RenderFillRect(renderer, &torso);
    SDL_SetRenderDrawColor(renderer, 255, 218, 148, 255);
    SDL_Rect head{static_cast<int>((x + 0.65f) * scale), static_cast<int>((y + 0.1f) * scale),
                  static_cast<int>(0.75f * scale), static_cast<int>(0.7f * scale)};
    SDL_RenderFillRect(renderer, &head);
    SDL_SetRenderDrawColor(renderer, 24, 32, 45, 255);
    SDL_Rect eye{head.x + (movement < 0 ? 1 : head.w - 2), head.y + 2, 1, 2};
    SDL_RenderFillRect(renderer, &eye);
}
