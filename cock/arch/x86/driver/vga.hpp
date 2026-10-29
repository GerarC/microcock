#ifndef ARCH_X86_DRIVER_VGA_HPP
#define ARCH_X86_DRIVER_VGA_HPP

/**
 * @file
 * @brief Kernel text console on the VGA text mode buffer (80x25).
 *
 * Writes 16 bit cells (character in the low byte, attribute in the high byte)
 * straight into the memory mapped text buffer at physical 0xB8000. The kernel
 * reaches it through the higher half direct map. Used only through the HAL
 * console (`hal/console.hpp`); the Ring 3 VGA driver is a separate program.
 */

#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Attribute byte of a text cell: low nibble foreground, high nibble
 * background. */
typedef uint8_t vcolor;
/** @brief One text cell: attribute in bits 8..15, ASCII code in bits 0..7. */
typedef uint16_t vchar;

/**
 * @brief The 16 colors of the VGA text palette (value is the hardware index).
 */
enum class VGAColor : vcolor {
	BLACK = 0,		   /**< Color index 0. */
	BLUE = 1,		   /**< Color index 1. */
	GREEN = 2,		   /**< Color index 2. */
	CYAN = 3,		   /**< Color index 3. */
	RED = 4,		   /**< Color index 4. */
	MAGENTA = 5,	   /**< Color index 5. */
	BROWN = 6,		   /**< Color index 6. */
	LIGHT_GRAY = 7,	   /**< Color index 7. */
	DARK_GRAY = 8,	   /**< Color index 8. */
	LIGHT_BLUE = 9,	   /**< Color index 9. */
	LIGHT_GREEN = 10,  /**< Color index 10. */
	LIGHT_CYAN = 11,   /**< Color index 11. */
	LIGHT_RED = 12,	   /**< Color index 12. */
	LIGHT_MAGENTA = 13, /**< Color index 13. */
	LIGHT_BROWN = 14,  /**< Color index 14 (yellow). */
	WHITE = 15,		   /**< Color index 15. */
};

/**
 * @brief Cursorless VGA text writer with scrolling.
 *
 * Touches only the text buffer memory; it does not program the CRT controller
 * ports, so the hardware cursor is not moved. Not thread safe: callers (the HAL
 * console) run it in Ring 0 and serialize access.
 */
class VGA final {
  public:
	/** @brief Columns of the text screen. */
	static constexpr size_t WIDTH = 80;
	/** @brief Rows of the text screen. */
	static constexpr size_t HEIGHT = 25;
	/** @brief Physical address of the text buffer. */
	static constexpr uintptr_t PHYSICAL_BUFFER = 0xB8000;
	/** @brief Kernel virtual address of the text buffer (direct map). */
	static constexpr uintptr_t BUFFER =
		core::hal::KERNEL_SPACE_BASE + PHYSICAL_BUFFER;

	/**
	 * @brief Builds a writer positioned at the top left corner with green on
	 * black text. Does not touch the screen; call clear() to blank it.
	 */
	constexpr VGA()
		: row(0), column(0), color(entryColor(VGAColor::GREEN, VGAColor::BLACK)) {}

	/**
	 * @brief Selects the attribute used by the following characters.
	 * @param fg Foreground color.
	 * @param bg Background color.
	 */
	void setColor(VGAColor fg, VGAColor bg);

	/**
	 * @brief Writes one character at the cursor and advances it.
	 *
	 * A newline moves to column zero of the next row; reaching the last column
	 * wraps to the next row; passing the last row scrolls the screen up one
	 * line. Other control characters are drawn as glyphs.
	 *
	 * @param c Character to print (treated as an unsigned byte).
	 */
	void putChar(char c);

	/**
	 * @brief Writes a byte string with putChar().
	 * @param data Pointer to at least `size` readable bytes.
	 * @param size Number of bytes to print.
	 */
	void write(const char *data, size_t size);

	/**
	 * @brief Fills the whole screen with blanks in the current attribute and
	 * moves the cursor to the top left corner.
	 */
	void clear();

  private:
	/** @brief Index of the first row. */
	static constexpr size_t FIRST_ROW = 0;
	/** @brief Index of the first column. */
	static constexpr size_t FIRST_COLUMN = 0;
	/** @brief Bit position of the background nibble in the attribute byte. */
	static constexpr unsigned COLOR_BACKGROUND_SHIFT = 4;
	/** @brief Bit position of the attribute byte in a text cell. */
	static constexpr unsigned CHARACTER_COLOR_SHIFT = 8;
	/** @brief Character that starts a new line. */
	static constexpr char NEWLINE = '\n';
	/** @brief Character used to blank a cell. */
	static constexpr char EMPTY = ' ';

	/** @brief Current cursor row. */
	size_t row;
	/** @brief Current cursor column. */
	size_t column;
	/** @brief Attribute applied to new characters. */
	vcolor color;

	/**
	 * @brief Packs foreground and background into an attribute byte.
	 * @param fg Foreground color.
	 * @param bg Background color.
	 * @return Attribute byte (`bg << 4 | fg`).
	 */
	static constexpr vcolor entryColor(VGAColor fg, VGAColor bg) {
		return static_cast<vcolor>(fg) |
			   (static_cast<vcolor>(bg) << COLOR_BACKGROUND_SHIFT);
	}

	/**
	 * @brief Packs a character and an attribute into a text cell.
	 * @param uc ASCII code.
	 * @param color Attribute byte.
	 * @return Cell value (`color << 8 | uc`).
	 */
	static constexpr vchar entry(unsigned char uc, vcolor color) {
		return static_cast<vchar>(uc) |
			   static_cast<vchar>(static_cast<vchar>(color)
								  << CHARACTER_COLOR_SHIFT);
	}

	/**
	 * @brief Gives access to the text buffer.
	 * @return Volatile pointer to cell (0, 0); stores are visible to the
	 * display hardware, so they must not be elided.
	 */
	static volatile vchar *buffer() {
		return reinterpret_cast<volatile vchar *>(BUFFER);
	}

	/** @brief Moves the cursor to the start of the next row, scrolling at the
	 * bottom. */
	void newline();
	/** @brief Copies every row one row up and blanks the last row. */
	void scroll();
};

} // namespace cock::arch::x86

#endif // !ARCH_X86_DRIVER_VGA_HPP
