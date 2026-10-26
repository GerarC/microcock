/**
 * @file vga.hpp
 * @brief Text mode VGA output of the VGA driver (Ring 3), 80x25 cells with colors.
 *
 * The driver maps the physical text buffer into its own address space with a physical
 * `mmap` (needs the matching capability) and writes cells directly. All state is static
 * and the class is not thread safe; only the VGA server uses it.
 *
 * There is one screen image for each terminal (`TERMINAL_COUNT`), so the text of one terminal never overwrites the
 * text of another. Writes go to the image of the target terminal and are mirrored on the hardware only while that
 * terminal is the one on the screen.
 */
#ifndef VGA_SERVER_HPP
#define VGA_SERVER_HPP

#include <cock/abi/ansi.hpp>
#include <cock/abi/console.hpp>
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
static constexpr size_t    TERMINAL_COUNT       = cock::abi::console::TERMINAL_COUNT;
static constexpr size_t    CELL_COUNT           = VGA_WIDTH * VGA_HEIGHT;

/** First port of the CRT controller (color mode): the index register. */
static constexpr uint16_t  CRTC_INDEX_PORT      = 0x3D4;
/** Data register of the CRT controller. */
static constexpr uint16_t  CRTC_DATA_PORT       = 0x3D5;
/** Number of ports the driver needs from the CRT controller. */
static constexpr uint32_t  CRTC_PORT_COUNT      = 2;
/** CRT controller register that holds the high byte of the cursor position. */
static constexpr uint8_t   CURSOR_LOCATION_HIGH = 0x0E;
/** CRT controller register that holds the low byte of the cursor position. */
static constexpr uint8_t   CURSOR_LOCATION_LOW  = 0x0F;
/** Bits in a byte. */
static constexpr unsigned  BYTE_BITS            = 8;
/** Mask of one byte. */
static constexpr uint16_t  BYTE_MASK            = 0xFF;

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

/** Bits to shift the background into the high nibble of an attribute byte. */
static constexpr unsigned  BACKGROUND_SHIFT     = 4;
/** Mask of the foreground color in an attribute byte. */
static constexpr uint8_t   FOREGROUND_MASK      = 0x0F;
/** Attribute byte of a cell with the default colors: background in the high nibble, foreground in the low. */
static constexpr uint8_t   DEFAULT_ATTRIBUTE    = static_cast<uint8_t>((DEFAULT_BG << BACKGROUND_SHIFT) | (DEFAULT_FG & FOREGROUND_MASK));
/** Mask of the colors a background can have (the high bit blinks, so it is never used). */
static constexpr uint8_t   BACKGROUND_MASK      = 0x07;
/** Added to a normal color to make it bright. */
static constexpr uint8_t   BRIGHT_BIT           = 0x08;
/** VGA code of each ANSI color number 0 to 7 (black, red, green, yellow, blue, magenta, cyan, white). */
static constexpr uint8_t   ANSI_TO_VGA[cock::abi::ansi::NORMAL_COLOR_COUNT] = {0, 4, 2, 6, 1, 5, 3, 7};

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

/** The cells and the cursor of the image of one terminal. */
struct Screen {
    /** The cells, row by row. */
    uint16_t cells[CELL_COUNT];
    /** Cursor row. */
    size_t row;
    /** Cursor column. */
    size_t column;
    /** Reader of the color sequences in the text written to this terminal. */
    cock::abi::ansi::SgrParser sgr;
    /** Attribute byte of the cells written next on this terminal. */
    uint8_t color = DEFAULT_ATTRIBUTE;
};

/** Text screen writer with a cursor, scrolling and a current color, for each terminal. */
class VgaServer {
public:
    /**
     * @brief Maps the VGA text buffer at `VGA_VIRTUAL_TARGET` (physical `mmap`).
     * @return true when mapped; false when the kernel refused (no capability for the
     *         memory), in which case no output call may be used.
     */
    static bool init();
    /**
     * @brief Asks for the CRT controller ports and puts the hardware cursor where the terminal on the screen has its
     *        cursor; from then on every output call and `show` keep it there.
     * @return true when the ports are held; false when the kernel refused (no `io` capability yet): the cursor then stays
     *         where it was and the call may be repeated when a capability arrives.
     */
    static bool initCursor();
    /**
     * @brief Chooses the terminal that the next `putChar`, `write` and `clear` are for.
     * @param terminal Terminal number; one that is out of range is ignored.
     */
    static void selectTarget(size_t terminal);
    /**
     * @brief Puts a terminal on the hardware screen by copying its image.
     * @param terminal Terminal number; one that is out of range is ignored. Requires a successful `init`.
     */
    static void show(size_t terminal);
    /**
     * @brief Writes one character at the cursor with the current color.
     *
     * A newline moves to the start of the next line; a backspace erases the previous
     * cell of the line (nothing at column zero); any other byte is shown as a cell and
     * wraps at the end of the line. The screen scrolls up one line after the last row.
     * The hardware cursor follows. A color sequence (`ESC [ ... m`) changes the color of the target terminal and shows
     * nothing; see `cock/abi/ansi.hpp`. Requires a successful `init`.
     * @param c Character to write.
     */
    static void putChar(char c);
    /**
     * @brief Writes a block of characters, one `putChar` each.
     * @param data Characters to write.
     * @param size Number of characters.
     */
    static void write(const char* data, size_t size);
    /** @brief Fills the screen with blanks in the current color of the target terminal and moves the cursor to the top left. */
    static void clear();
    /**
     * @brief Sets the colors used by later output of the target terminal; already written cells keep theirs.
     * @param fg Foreground color.
     * @param bg Background color.
     */
    static void setColor(VGAColor fg, VGAColor bg);

private:
    /** @brief Moves the cursor to the start of the next line, scrolling at the bottom. */
    static void newline();
    /**
     * @brief Writes one character at the cursor of the target image, as `putChar` does, without moving the hardware cursor.
     * @param c Character to write.
     */
    static void place(char c);
    /** @brief Puts the hardware cursor at the cursor of the terminal on the screen; nothing before `initCursor` succeeded. */
    static void moveCursor();
    /** @brief Moves every line of the target image up by one and blanks the last line. */
    static void scroll();
    /**
     * @brief Writes a cell of the target image, and on the hardware when the target is the terminal on the screen.
     * @param index Cell position.
     * @param entry Cell value.
     */
    static void store(size_t index, uint16_t entry);
    /** @brief Copies the image of the terminal on the screen to the hardware. */
    static void redraw();
    /**
     * @brief Turns the style chosen by the escape sequences into an attribute byte.
     * @param style Style read by the parser of a terminal.
     * @return Background in the high nibble, foreground in the low; the defaults where the style has none, and a bold
     *         normal foreground shown bright.
     */
    static uint8_t attributeOf(const cock::abi::ansi::Style& style);
    /**
     * @brief Builds a screen cell.
     * @param uc Character code.
     * @param color Attribute byte (background in the high nibble, foreground in the low).
     * @return The 16 bit cell value.
     */
    static uint16_t vga_entry(unsigned char uc, uint8_t color);

    /** The image of each terminal. */
    static Screen screens[TERMINAL_COUNT];
    /** Terminal the output calls are for. */
    static size_t target;
    /** Terminal on the hardware screen. */
    static size_t active;
    /** Mapped text buffer, null before `init`. */
    static volatile uint16_t* buffer;
    /** True once the CRT controller ports were obtained. */
    static bool cursorReady;
};

} // namespace cock::vga

#endif
