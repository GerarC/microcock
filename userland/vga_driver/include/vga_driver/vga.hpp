/**
 * @file vga.hpp
 * @brief Text mode VGA output of the VGA driver (Ring 3), 80x25 cells with colors.
 *
 * The driver maps the physical text buffer into its own address space with a physical
 * `mmap` (needs the matching capability) and writes cells directly. All state is static
 * and the class is not thread safe; only the VGA server uses it.
 */
#ifndef VGA_SERVER_HPP
#define VGA_SERVER_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::vga {

/** Physical address of the VGA text buffer. */
static constexpr uintptr_t VGA_PHYSICAL_ADDRESS = 0xB8000;
/** Fixed virtual address where the text buffer is mapped in the driver. */
static constexpr uintptr_t VGA_VIRTUAL_TARGET   = 0xA0000000;
/** Columns of the text screen. */
static constexpr size_t    VGA_WIDTH            = 80;
/** Rows of the text screen. */
static constexpr size_t    VGA_HEIGHT           = 25;
/** Size of a page in bytes; the text buffer fits in one page. */
static constexpr size_t    PAGE_SIZE            = 4096;

/** Character that fills blank cells. */
static constexpr char      CHAR_EMPTY           = ' ';
/** Character that moves to the start of the next line. */
static constexpr char      CHAR_NEWLINE         = '\n';
/** Character that erases the previous cell on the same line. */
static constexpr char      CHAR_BACKSPACE       = '\b';
/** Default foreground color (a `VGAColor` value, light blue). */
static constexpr uint8_t   DEFAULT_FG           = 9;
/** Default background color (black). */
static constexpr uint8_t   DEFAULT_BG           = 0;

/** The 16 colors of the VGA text palette; the value is the 4 bit hardware code. */
enum class VGAColor : uint8_t {
	BLACK = 0,
	BLUE = 1,
	GREEN = 2,
	CYAN = 3,
	RED = 4,
	MAGENTA = 5,
	BROWN = 6,
	LIGHT_GRAY = 7,
	DARK_GRAY = 8,
	LIGHT_BLUE = 9,
	LIGHT_GREEN = 10,
	LIGHT_CYAN = 11,
	LIGHT_RED = 12,
	LIGHT_MAGENTA = 13,
	LIGHT_BROWN = 14,
	WHITE = 15
};

/** Text screen writer with a cursor, scrolling and a current color. */
class VgaServer {
public:
    /**
     * @brief Maps the VGA text buffer at `VGA_VIRTUAL_TARGET` (physical `mmap`).
     * @return true when mapped; false when the kernel refused (no capability for the
     *         memory), in which case no output call may be used.
     */
    static bool init();
    /**
     * @brief Writes one character at the cursor with the current color.
     *
     * A newline moves to the start of the next line; a backspace erases the previous
     * cell of the line (nothing at column zero); any other byte is shown as a cell and
     * wraps at the end of the line. The screen scrolls up one line after the last row.
     * Requires a successful `init`.
     * @param c Character to write.
     */
    static void putChar(char c);
    /**
     * @brief Writes a block of characters, one `putChar` each.
     * @param data Characters to write.
     * @param size Number of characters.
     */
    static void write(const char* data, size_t size);
    /** @brief Fills the screen with blanks in the current color and moves the cursor to the top left. */
    static void clear();
    /**
     * @brief Sets the colors used by later output; already written cells keep theirs.
     * @param fg Foreground color.
     * @param bg Background color.
     */
    static void setColor(VGAColor fg, VGAColor bg);

private:
    /** @brief Moves the cursor to the start of the next line, scrolling at the bottom. */
    static void newline();
    /** @brief Moves every line up by one and blanks the last line. */
    static void scroll();
    /**
     * @brief Builds a screen cell.
     * @param uc Character code.
     * @param color Attribute byte (background in the high nibble, foreground in the low).
     * @return The 16 bit cell value.
     */
    static uint16_t vga_entry(unsigned char uc, uint8_t color);

    /** Cursor row. */
    static size_t row;
    /** Cursor column. */
    static size_t column;
    /** Attribute byte used for new cells. */
    static uint8_t current_color;
    /** Mapped text buffer, null before `init`. */
    static volatile uint16_t* buffer;
};

} // namespace cock::vga

#endif
