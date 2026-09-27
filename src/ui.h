// Drawing helpers shared by the front end (main.cpp) and the game layer (game.cpp).
// Sizes are the loaded VT323 sizes: 17, 18, 19, 20, 21, 30 and 34 px.
#pragma once
#include "raylib.h"

#include <string>

struct Theme { Color enamel, enamelDeep, plate, ink, dim, red, amber, shadow; };
extern Theme TH;

Color hexc(unsigned h, float a = 1);
Font& F(int size);
float tw(int size, const std::string& s, float sp = 0);
// Text whose CSS-style line box starts at y with line height lh.
void txt(int size, const std::string& s, float x, float y, float lh, Color c, float sp = 0);
Color fade(Color c, float a);
void rr(Rectangle r, float rad, Color c);
void rrLine(Rectangle r, float rad, float th, Color c);
// An enamel street-sign plate: soft shadow, deep edge, face and an inset line.
void plate(Rectangle r, float rad, float inset, float innerRad, Color bg, float alpha = 1);
void toast(const std::string& s);
