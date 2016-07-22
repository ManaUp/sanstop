## sanstop

replace Comic Sans with an even worse font

more seriously, generates .dds and .xml files from a font for use in Wizard101

### Dependencies

[FreeType](https://www.freetype.org/), LibPNG, and ZLib.

### Usage:

    # to compile it:
    make
    # to magically convert a font:
    ./sanstop SomeFont.ttf WizardRegular targets.txt 48
    # targets.txt is a file with every character you want in your font
    # in this case this uses 48 as the height
