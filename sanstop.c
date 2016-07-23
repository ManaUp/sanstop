#include "sanstop.h"
#include "utf8.h"

int main(int argc, char** argv) {
  Config c = (Config) {NULL, NULL, NULL, -1, HMARGIN, VMARGIN, 0, 0};
  Glyph* glyphs = NULL;
  FT_Library ft = NULL;
  FT_Face face = NULL;
  Buffer b = (Buffer) {NULL, -1, 1, IMAGE_DIM - c.vMargin - c.vShift};
  int stat = readConfig(&c, argc, argv);
  if (stat == 0) {
    size_t glyphCap = 256;
    size_t glyphCount = 0;
    glyphs = malloc(glyphCap * sizeof(Glyph));
    if (initializeBuffer(&b) != 0) {
      fprintf(stderr, "Could not allocate memory for buffer.\n");
      fprintf(stderr, "Maybe you don't have enough?\n");
      stat = -2;
      goto end;
    }
    if (FT_Init_FreeType(&ft) != 0) {
      fprintf(stderr, "Could not init FreeType library\n");
      stat = -2;
      goto end;
    }
    if (FT_New_Face(ft, c.fontFileName, 0, &face) != 0) {
      fprintf(stderr, "Could not load font face %s\n", c.fontFileName);
      stat = -1;
      goto end;
    }
    stat = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (stat != 0) {
      fprintf(stderr, "This font doesn't have a Unicode charmap.\n");
      goto end;
    }
    FT_Set_Pixel_Sizes(face, 0, c.size);
    char* here = c.targets;
    int codepoint;
    // Load and blit glyphs
    while (*here != '\0') {
      int ustat = utf8Next(&here, &codepoint);
      //printf("[0x%x]\n", codepoint);
      if (ustat != 0) {
        fprintf(stderr, "Malformatted UTF-8.\n");
        int byte = ustat & 0xFF;
        switch (utf8DecodeErrorClass(ustat)) {
        case ERRC_UNEXPECTED_CONTINUATION:
          fprintf(stderr, "Unexpected continuation byte 0x%x.\n", byte);
          break;
        case ERRC_CONTINUATION_EXPECTED:
          fprintf(stderr, "Expected continuation byte, but got 0x%x instead.\n", byte);
          break;
        case ERRC_INVALID_UTF8_BYTE:
          fprintf(stderr, "Invalid byte 0x%x.\n", byte);
        }
        stat = -1;
        goto end;
      }
      // printf("[%x] ", codepoint);
      if (codepoint < 32 || codepoint == 0xfeff || codepoint == 0xfffe) continue;
      if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER) != 0) {
        fprintf(stderr, "Warn: glyph %lc [0x%x] does not exist in this font.\n", codepoint, codepoint);
      } else {
        // printf("[%x] ", codepoint);
        glyphs[glyphCount].id = codepoint;
        stat = blitAndAdvance(&b, face, glyphs + glyphCount, &c);
        if (stat != 0) goto end;
        ++glyphCount;
        if (glyphCount >= glyphCap) {
          glyphCap <<= 1;
          glyphs = realloc(glyphs, glyphCap * sizeof(Glyph));
        }
      }
    }
    // Write what's Left
    writePage(&b, &c);
    // Write XML file
    int len = strlen(c.fontPrefix) + 5;
    char* fname = malloc(sizeof(char) * len);
    snprintf(fname, len, "%s.xml", c.fontPrefix);
    FILE* f = fopen(fname, "w");
    if (f == NULL) {
      fprintf(stderr, "Cannot write to file %s!", fname);
      free(fname);
      stat = -1;
      goto end;
    }
    writeXMLHeader(f, face, &c);
    writeXMLPageData(f, &c, &b);
    writeXMLGlyphData(f, glyphCount, glyphs, &c);
    writeXMLFooter(f);
    fclose(f);
    free(fname);
  }
  end:
  cleanConfig(&c);
  if (glyphs != NULL) free(glyphs);
  if (face != NULL) FT_Done_Face(face);
  if (ft != NULL) FT_Done_FreeType(ft);
  if (b.data != NULL) free(b.data);
  return stat;
}

int initializeBuffer(Buffer* b) {
  b->page = 0;
  uint8_t* data = malloc(IMAGE_PIXELS * sizeof(uint8_t));
  if (data == NULL) return -1;
  b->data = data;
  return 0;
}

void flipPage(Buffer* b, Config* c) {
  b->page++;
  memset(b->data, 0, IMAGE_PIXELS * sizeof(uint8_t));
  b->x = 1;
  b->y = IMAGE_DIM - c->vMargin - c->vShift;
}

int shouldGoUp(Buffer* b, int width) {
  return IMAGE_DIM - b->x < width;
}

int shouldFlip(Buffer* b, Config* c, int height) {
  return b->y < height - c->vShift;
}

void goUp(Buffer* b, int height) {
  b->y -= height;
  b->x = 1;
}

void advance(Buffer* b, int width) {
  b->x += width;
}

#include "dds.h"

int writePage(Buffer* b, Config* c) {
  size_t len = snprintf(NULL, 0, "%s_%d.dds", c->fontPrefix, b->page) + 1;
  char* fname = malloc(sizeof(char) * len);
  snprintf(fname, len, "%s_%d.dds", c->fontPrefix, b->page);
  FILE* f = fopen(fname, "wb");
  if (f == NULL) {
    fprintf(stderr, "File %s is unwritable\n", fname);
    free(fname);
    return -1;
  }
  fwrite(DDS_HEADER, DDS_HEADER_SIZE, 1, f);
  fwrite(b->data, IMAGE_PIXELS * sizeof(uint8_t), 1, f);
  free(fname);
  fclose(f);
  return 0;
}

void blit(Buffer* b, FT_Face face, Glyph* gp, int tw, int th, int size) {
  Glyph g = *gp;
  g.page = b->page;
  g.left = b->x;
  g.bottom = b->y;
  g.right = b->x + tw;
  g.top = b->y - th;
  int sx = g.left + face->glyph->bitmap_left;
  int base = g.bottom + size * face->descender / face->units_per_EM;
  int sy = base - face->glyph->bitmap_top;
  int w = face->glyph->bitmap.width;
  int h = face->glyph->bitmap.rows;
  uint8_t* iwl = b->data + sx + sy * IMAGE_DIM;
  uint8_t* grl = face->glyph->bitmap.buffer;
  // This is hardly the fastest way, but let's focus on being correct first.
  for (int i = 0; i < h; ++i) {
    memcpy(iwl, grl, w);
    iwl += IMAGE_DIM;
    grl += w;
  }
  *gp = g;
}

int blitAndAdvance(Buffer* b, FT_Face face, Glyph* gp, Config* c) {
  int tw = (face->glyph->advance.x >> 6) + c->hMargin;
  int th = c->vMargin + c->size * face->height / face->units_per_EM;
  if (shouldGoUp(b, tw))
    goUp(b, th);
  if (shouldFlip(b, c, th)) {
    int stat = writePage(b, c);
    if (stat != 0) return stat;
    flipPage(b, c);
  }
  blit(b, face, gp, tw, th, c->size);
  advance(b, tw);
  return 0;
}

const char* xmlFontPropertyFormatter =
  "<GlyphisFont Version=\"1\" "
  "FaceName=\"%s\" "
  "PtSize=\"%d\" "
  "Weight=\"%d\" "
  "Bold=\"%d\" "
  "Italic=\"%d\" "
  "Outline=\"0\" "
  "OutputFormat=\"DDS_A8\" "
  "InvalidGlyph=\"127\">\n"
  ;
void writeXMLHeader(FILE* f, FT_Face face, Config* c) {
  fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
  fprintf(f, xmlFontPropertyFormatter,
    face->family_name,
    c->size * 5,
    700,
    !!(face->style_flags & FT_STYLE_FLAG_BOLD),
    !!(face->style_flags & FT_STYLE_FLAG_ITALIC)
  );
}

void writeXMLPageData(FILE* f, Config* c, Buffer* b) {
  int totalPages = b->page + 1;
  fprintf(f, "\t\t<Pages Count=\"%d\">\n", totalPages);
  char* proper = strrchr(c->fontPrefix, '/');
  if (proper == NULL) proper = c->fontPrefix;
  else ++proper;
  for (int i = 0; i < totalPages; ++i) {
    fprintf(f, "\t\t\t\t<Page FileName=\"%s_%d.dds\" Width=\"%d\" Height=\"%d\" />\n",
        proper, i, IMAGE_DIM, IMAGE_DIM);
  }
  fprintf(f, "\t\t</Pages>\n");
}

const char* xmlGlyphEntryFormatter =
  "\t\t\t\t"
  "<Glyph ID=\"%d\" "
  "A=\"%d\" "
  "B=\"%d\" "
  "C=\"%d\" "
  "Page=\"%d\" "
  "Top=\"%d\" "
  "Bottom=\"%d\" "
  "Left=\"%d\" "
  "Right=\"%d\" />\n"
  ;
#include <time.h>
#include <math.h>
void writeXMLGlyphData(FILE* f, int glyphCount, Glyph* glyphs, Config* c) {
  srand(time(NULL));
  fprintf(f, "\t\t<Glyphs Count=\"%d\">\n", glyphCount);
  for (int i = 0; i < glyphCount; ++i) {
    Glyph g = glyphs[i];
    fprintf(f, xmlGlyphEntryFormatter,
      g.id,
      0,
      g.right - g.left - 2,
      0,
      g.page,
      g.top + c->vShift,
      g.bottom + c->vShift,
      g.left + c->hShift,
      g.right - c->hMargin + c->hShift
    );
  }
  fprintf(f, "\t\t</Glyphs>\n");
}

void writeXMLFooter(FILE* f) {
  fprintf(f, "</GlyphisFont>\n");
}

int readConfig(Config* c, int argc, char** argv) {
  int pos = 0;
  int ignoreOpts = 0;
  int status = 0;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-' && !ignoreOpts) {
      if (argv[i][1] == '-') {
        char* opt = argv[i] + 2;
        if (*opt == '\0') ignoreOpts = 1;
        else if (!strcmp(opt, "help")) {
          printHelp();
          status = 1;
          break;
        } else if (!strcmp(opt, "horizontal-margin")) {
          if (i >= argc - 1) {
            fprintf(stderr, "--horizontal-margin requires a parameter.\n");
            status = -1;
            break;
          }
          c->hMargin = atoi(argv[++i]);
        } else if (!strcmp(opt, "vertical-margin")) {
          if (i >= argc - 1) {
            fprintf(stderr, "--vertical-margin requires a parameter.\n");
            status = -1;
            break;
          }
          c->vMargin = atoi(argv[++i]);
        } else if (!strcmp(opt, "horizontal-shift")) {
          if (i >= argc - 1) {
            fprintf(stderr, "--horizontal-shift requires a parameter.\n");
            status = -1;
            break;
          }
          c->hShift = atoi(argv[++i]);
        } else if (!strcmp(opt, "vertical-shift")) {
          if (i >= argc - 1) {
            fprintf(stderr, "--vertical-shift requires a parameter.\n");
            status = -1;
            break;
          }
          c->vShift = atoi(argv[++i]);
        } else {
          fprintf(stderr, "Invalid option %s.\n", argv[i]);
          status = -1;
          break;
        }
      } else {
        for (char* f = argv[i] + 1; *f != '\0'; ++f) {
          switch (*f) {
          case 'h':
            printHelp();
            status = 1;
            goto end;
          default:
            fprintf(stderr, "Invalid option -%c.\n", *f);
            status = -1;
            goto end;
          }
        }
      }
    }
    else {
      switch (pos) {
      case 0:
        c->fontFileName = argv[i];
        break;
      case 1:
        c->fontPrefix = argv[i];
        break;
      case 2: {
        FILE* f = fopen(argv[i], "r");
        if (f == NULL) {
          fprintf(stderr, "File %s is missing or unreadable\n", argv[i]);
          status = -1;
          goto end;
        }
        // Slurp the source
      	// Thanks http://stackoverflow.com/questions/14002954/c-programming-how-to-read-the-whole-file-contents-into-a-buffer
      	fseek(f, 0, SEEK_END);
      	long fsize = ftell(f);
      	fseek(f, 0, SEEK_SET);
      	char* string = (char*) malloc(fsize + 1);
      	size_t br = fread(string, fsize, 1, f);
        if (br < 1) {
          status = -2;
          goto end;
        }
      	fclose(f);
        c->targets = string;
        break;
      }
      case 3:
        c->size = atoi(argv[i]);
        if (c->size <= 0) {
          fprintf(stderr, "Size too small!\n");
          status = -1;
          goto end;
        }
        if (c->size > 144) {
          fprintf(stderr, "Size too big (max is 144)!\n");
          status = -1;
          goto end;
        }
        break;
      default:
        status = -1;
        goto end;
      }
      ++pos;
    }
  }
  end:
  if (status == 0 && (c->fontFileName == NULL || c->fontPrefix == NULL ||
      c->targets == NULL || c->size <= 0))
    status = -1;
  if (c->vShift > c->vMargin || c->vShift < -c->vMargin ||
      c->hShift < 0 || c->hShift > c->hMargin)
    fprintf(stderr, "Warn: shift exceeds margin bounds.\n");
  if (status == -1) printUsage();
  if (status == -2) fprintf(stderr, "Unexpected error; please tell kyarei.\n");
  return status;
}

void cleanConfig(Config* c) {
  free(c->targets);
}

static const char* DESCRIPTION =
  "sanstop - generates xml and dds files from font files for Wizard101.\n"
  "Version 0.1.\n"
  ;

static const char* USAGE =
  "Usage:\n"
  "  sanstop [OPTIONS] <font-file> <font-prefix> <targets> <size>\n"
  "  font-file: the source font file (ttf / otf)\n"
  "  font-prefix: the prefix used for output files\n"
  "  targets: file containing list of characters to be used\n"
  "  size: the height to use for glyphs\n"
  "  OPTIONS -\n"
  "    -h --help display this help message\n"
  "    -- don't treat future arguments starting with - as flags\n"
  "    --horizontal-margin <amt> specify how much extra space to leave\n"
  "        between glyphs (default: 4). Useful for dealing with\n"
  "        ill-behaved fonts.\n"
  "    --vertical-margin <amt> specify how much extra space to leave\n"
  "        between rows (default: 8). Useful for dealing with\n"
  "        ill-behaved fonts.\n"
  "    --horizontal-shift <amt> specify how much to the left (-) or\n"
  "        right (+) the bounding boxes should be adjusted (default: 0).\n"
  "        Useful for dealing with ill-behaved fonts.\n"
  "    --vertical-shift <amt> specify how much up (-) or down (+)\n"
  "        the bounding boxes should be adjusted (default: 0).\n"
  "        Useful for dealing with ill-behaved fonts.\n"
  "Return codes:\n"
  "  0: everything is fine\n"
  "  1: help was displayed\n"
  "  -1: usage error\n"
  "  -2: please tell kyarei!\n"
  ;

void printUsage() {
  fprintf(stderr, USAGE);
}

void printHelp() {
  fprintf(stderr, DESCRIPTION);
  printUsage();
}
