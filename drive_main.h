#ifndef DRIVE_MAIN_H
#define DRIVE_MAIN_H

#include "Inkplate.h"

extern Inkplate display;

struct Result {
  String firstLine;
  String remainingText;
};

void setup_logic();
void logic();
Result parseInput(String input);
void display_text(Result result);
void display_git(Result result);

#endif