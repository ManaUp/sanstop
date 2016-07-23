#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define IMAGE_DIM 512
#define IMAGE_PIXELS (IMAGE_DIM * IMAGE_DIM)
#define DDS_HEADER_SIZE 128
#define VMARGIN 8
#define HMARGIN 4

typedef struct {
  uint8_t* data;
  int page;
  int x, y; // x = left edge; y = bottom edge
} Buffer;

typedef struct {
  char* fontFileName;
  char* fontPrefix;
  char* targets;
  int size;
  int hMargin, vMargin;
  int hShift, vShift;
} Config;

typedef struct {
  int id;
  int page;
  int left, right, top, bottom;
  // For future reference:
  // B = right - left - 2
  // A and C are unknown
} Glyph;

// Initializes a Buffer struct.
int initializeBuffer(Buffer* b);
// Increments the page counter, clears the data buffer, and resets coordinates.
void flipPage(Buffer* b, Config* c);
// Returns 1 if there is not enough room in the current row to fit another
// glyph of a given width, and thus the program should advance to the space
// above.
int shouldGoUp(Buffer* b, int width);
// Returns 1 if there is not enough room in the current image to fit another
// glyph of a given height, and thus the program should flip to a new page.
int shouldFlip(Buffer* b, Config* c, int height);
void goUp(Buffer* b, int height);
void advance(Buffer* b, int width);

// Writes the current page to a file.
int writePage(Buffer* b, Config* c);

// Blits the character stored in face->glyph to the buffer.
// Assumes that there is enough room.
// Does not advance automatically.
// Stores glyph definition in g.
// g->id must be set.
void blit(Buffer* b, FT_Face face, Glyph* gp, int tw, int th, int size);
// Same as blit, but checks if there is enough room and advances
// automatically afterwards.
int blitAndAdvance(Buffer* b, FT_Face face, Glyph* gp, Config* c);

void writeXMLHeader(FILE* f, FT_Face face, Config* c);
void writeXMLPageData(FILE* f, Config* c, Buffer* b);
void writeXMLGlyphData(FILE* f, int glyphCount, Glyph* glyphs, Config* c);
void writeXMLFooter(FILE* f);

int readConfig(Config* c, int argc, char** argv);
void cleanConfig(Config* c);
void printUsage();
void printHelp();
