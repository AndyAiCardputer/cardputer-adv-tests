# CardKeyBoard Key Codes Reference

**Module:** CardKeyBoard (I2C Keyboard)  
**Address:** 0x5F  
**Protocol:** I2C (slave device)  
**Last Updated:** 2025-11-28

## Overview

CardKeyBoard sends key codes via I2C depending on pressed modifiers:
- **Key** - regular keys (no modifiers)
- **Sym+Key** - symbols (Sym pressed)
- **Shift+Key** - uppercase letters (Shift pressed)
- **Fn+Key** - function keys (Fn pressed)

## Key Layout

```
Line 1: ESC  1  2  3  4  5  6  7  8  9  0  Back  Up
Line 2: TAB  q  w  e  r  t  y  u  i  o  p  Fn    Down
Line 3: Shift a  s  d  f  g  h  j  k  l  Enter Left
Line 4: Sym   z  x  c  v  b  n  m  ,  .  SPACE Right
```

## Key Codes Table

### Table 1: Key (Default - No Modifiers)

| Key | ESC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | Back | Up |
|-----|-----|---|---|---|---|---|---|---|---|---|---|------|-----|
| **Hex** | 0x1B | 0x31 | 0x32 | 0x33 | 0x34 | 0x35 | 0x36 | 0x37 | 0x38 | 0x39 | 0x30 | 0x08 | 0xB5 |
| **Dec** | 27 | 49 | 50 | 51 | 52 | 53 | 54 | 55 | 56 | 57 | 48 | 8 | 181 |

| Key | TAB | q | w | e | r | t | y | u | i | o | p | Fn | Down |
|-----|-----|---|---|---|---|---|---|---|---|---|---|-----|------|
| **Hex** | 0x09 | 0x71 | 0x77 | 0x65 | 0x72 | 0x74 | 0x79 | 0x75 | 0x69 | 0x6F | 0x70 | NULL | 0xB6 |
| **Dec** | 9 | 113 | 119 | 101 | 114 | 116 | 121 | 117 | 105 | 111 | 112 | - | 182 |

| Key | Shift | a | s | d | f | g | h | j | k | l | Enter | Left |
|-----|-------|---|---|---|---|---|---|---|---|---|--------|------|
| **Hex** | NULL | 0x61 | 0x73 | 0x64 | 0x66 | 0x67 | 0x68 | 0x6A | 0x6B | 0x6C | 0x0D | 0xB4 |
| **Dec** | - | 97 | 115 | 100 | 102 | 103 | 104 | 106 | 107 | 108 | 13 | 180 |

| Key | Sym | z | x | c | v | b | n | m | , | . | SPACE | Right |
|-----|-----|---|---|---|---|---|---|---|---|-----|--------|-------|
| **Hex** | NULL | 0x7A | 0x78 | 0x63 | 0x76 | 0x62 | 0x6E | 0x6D | 0x2C | 0x2E | 0x20 | 0xB7 |
| **Dec** | - | 122 | 120 | 99 | 118 | 98 | 110 | 109 | 44 | 46 | 32 | 183 |

### Table 2: Sym+Key (Symbol Mode)

| Key | ESC | ! | @ | # | $ | % | ^ | & | * | ( | ) | Back | Up |
|-----|-----|---|---|---|---|---|---|---|---|---|---|------|-----|
| **Hex** | 0x1B | 0x21 | 0x40 | 0x23 | 0x24 | 0x25 | 0x5E | 0x26 | 0x2A | 0x28 | 0x29 | 0x08 | 0xB5 |
| **Dec** | 27 | 33 | 64 | 35 | 36 | 37 | 94 | 38 | 42 | 40 | 41 | 8 | 181 |

| Key | TAB | { | } | [ | ] | / | \ | \| | ~ | ' | " | Fn | Down |
|-----|-----|---|---|---|---|---|---|-----|---|---|---|---|-----|------|
| **Hex** | 0x09 | 0x7B | 0x7D | 0x5B | 0x5D | 0x2F | 0x5C | 0x7C | 0x7E | 0x27 | 0x22 | NULL | 0xB6 |
| **Dec** | 9 | 123 | 125 | 91 | 93 | 47 | 92 | 124 | 126 | 39 | 34 | - | 182 |

| Key | Shift | : | ; | ` | + | - | _ | = | ? | NULL | Enter | Left |
|-----|-------|---|---|---|---|---|---|---|---|-------|--------|------|
| **Hex** | NULL | 0x3A | 0x3B | 0x60 | 0x2B | 0x2D | 0x5F | 0x3D | 0x3F | NULL | 0x0D | 0xB4 |
| **Dec** | - | 58 | 59 | 96 | 43 | 45 | 95 | 61 | 63 | - | 13 | 180 |

| Key | Sym | NULL | NULL | NULL | NULL | NULL | NULL | NULL | < | > | SPACE | Right |
|-----|-----|-------|-------|-------|-------|-------|-------|-------|---|---|--------|-------|
| **Hex** | NULL | NULL | NULL | NULL | NULL | NULL | NULL | NULL | 0x3C | 0x3E | 0x20 | 0xB7 |
| **Dec** | - | - | - | - | - | - | - | - | 60 | 62 | 32 | 183 |

### Table 3: Shift+Key (Uppercase Mode)

| Key | ESC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | Del | Up |
|-----|-----|---|---|---|---|---|---|---|---|---|---|-----|-----|
| **Hex** | 0x1B | 0x31 | 0x32 | 0x33 | 0x34 | 0x35 | 0x36 | 0x37 | 0x38 | 0x39 | 0x30 | 0x7F | 0xB5 |
| **Dec** | 27 | 49 | 50 | 51 | 52 | 53 | 54 | 55 | 56 | 57 | 48 | 127 | 181 |

| Key | TAB | Q | W | E | R | T | Y | U | I | O | P | Fn | Down |
|-----|-----|---|---|---|---|---|---|---|---|---|---|-----|------|
| **Hex** | 0x09 | 0x51 | 0x57 | 0x45 | 0x52 | 0x54 | 0x59 | 0x55 | 0x49 | 0x4F | 0x50 | NULL | 0xB6 |
| **Dec** | 9 | 81 | 87 | 69 | 82 | 84 | 89 | 85 | 73 | 79 | 80 | - | 182 |

| Key | Shift | A | S | D | F | G | H | J | K | L | Enter | Left |
|-----|-------|---|---|---|---|---|---|---|---|---|--------|------|
| **Hex** | NULL | 0x41 | 0x53 | 0x44 | 0x46 | 0x47 | 0x48 | 0x4A | 0x4B | 0x4C | 0x0D | 0xB4 |
| **Dec** | - | 65 | 83 | 68 | 70 | 71 | 72 | 74 | 75 | 76 | 13 | 180 |

| Key | Sym | Z | X | C | V | B | N | M | , | . | SPACE | Right |
|-----|-----|---|---|---|---|---|---|---|---|-----|--------|-------|
| **Hex** | NULL | 0x5A | 0x58 | 0x43 | 0x56 | 0x42 | 0x4E | 0x4D | 0x2C | 0x2E | 0x20 | 0xB7 |
| **Dec** | - | 90 | 88 | 67 | 86 | 66 | 78 | 77 | 44 | 46 | 32 | 183 |

### Table 4: Fn+Key (Function Mode)

| Key | ESC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | Back | Up |
|-----|-----|---|---|---|---|---|---|---|---|---|---|------|-----|
| **Hex** | 0x80 | 0x81 | 0x82 | 0x83 | 0x84 | 0x85 | 0x86 | 0x87 | 0x88 | 0x89 | 0x8A | 0x8B | 0x90 |
| **Dec** | 128 | 129 | 130 | 131 | 132 | 133 | 134 | 135 | 136 | 137 | 138 | 139 | 144 |

| Key | TAB | Q | W | E | R | T | Y | U | I | O | P | Fn | Down |
|-----|-----|---|---|---|---|---|---|---|---|---|---|-----|------|
| **Hex** | 0x8C | 0x8D | 0x8E | 0x8F | 0x90 | 0x91 | 0x92 | 0x93 | 0x94 | 0x95 | 0x96 | NULL | 0xA4 |
| **Dec** | 140 | 141 | 142 | 143 | 144 | 145 | 146 | 147 | 148 | 149 | 150 | - | 164 |

| Key | Shift | A | S | D | F | G | H | J | K | L | Enter | Left |
|-----|-------|---|---|---|---|---|---|---|---|---|--------|------|
| **Hex** | NULL | 0x9A | 0x9B | 0x9C | 0x9D | 0x9E | 0x9F | 0xA0 | 0xA1 | 0xA2 | 0xA3 | 0x98 |
| **Dec** | - | 154 | 155 | 156 | 157 | 158 | 159 | 160 | 161 | 162 | 163 | 152 |

| Key | Sym | Z | X | C | V | B | N | M | , | . | SPACE | Right |
|-----|-----|---|---|---|---|---|---|---|---|-----|--------|-------|
| **Hex** | NULL | 0xA6 | 0xA7 | 0xA8 | 0xA9 | 0xAA | 0xAB | 0xAC | 0xAD | 0xAE | 0xAF | 0xA5 |
| **Dec** | - | 166 | 167 | 168 | 169 | 170 | 171 | 172 | 173 | 174 | 175 | 165 |

## Special Keys Summary

### Control Keys

| Key | Code (Hex) | Code (Dec) | Description |
|-----|------------|------------|-------------|
| ESC | 0x1B | 27 | Escape |
| TAB | 0x09 | 9 | Tab |
| Enter | 0x0D | 13 | Enter / Return |
| Backspace | 0x08 | 8 | Backspace |
| Delete | 0x7F | 127 | Delete (Shift+Back) |
| SPACE | 0x20 | 32 | Space |

### Arrow Keys

| Key | Code (Hex) | Code (Dec) | Description |
|-----|------------|------------|-------------|
| Up | 0xB5 | 181 | Arrow Up |
| Down | 0xB6 | 182 | Arrow Down |
| Left | 0xB4 | 180 | Arrow Left |
| Right | 0xB7 | 183 | Arrow Right |

### Modifier Keys

| Key | Code | Description |
|-----|------|-------------|
| Shift | NULL | Modifier (not sent alone) |
| Sym | NULL | Modifier (not sent alone) |
| Fn | NULL | Modifier (not sent alone) |

### Function Keys (Fn+Key)

Fn+Key combinations send codes in range **0x80-0xAF**:
- **0x80-0x8B**: Fn+ESC, Fn+1-0, Fn+Back
- **0x8C-0x96**: Fn+TAB, Fn+Q-P
- **0x9A-0xA3**: Fn+A-L, Fn+Enter
- **0xA4-AF**: Fn+Down, Fn+Right, Fn+Z-M, Fn+,, Fn+., Fn+SPACE

## Usage in Code

### Basic Processing

```cpp
unsigned char key = readI2CKeyboard();

if (key == 0) {
    return;  // No key pressed
}

// Control keys
if (key == 13) {  // Enter
    // Execute command
} else if (key == 8 || key == 127) {  // Backspace/Delete
    // Delete character
} else if (key == 9) {  // Tab
    // Insert spaces
} else if (key == 27) {  // ESC
    // Clear input
}

// Arrow keys
if (key >= 180 && key <= 183) {
    // Handle arrow keys (180=Left, 181=Up, 182=Down, 183=Right)
}

// Function keys (Fn+Key)
if (key >= 128 && key <= 175) {
    // Handle function keys (optional)
}

// Printable characters (32-126)
if (key >= 32 && key <= 126) {
    // Add to input buffer
    inputBuffer += (char)key;
}
```

### Handling Modifiers

CardKeyBoard automatically sends correct codes depending on pressed modifiers:
- **Sym+Key** → symbols (!, @, #, $, %, ^, &, *, (, ), {, }, [, ], /, \, |, ~, ', ", :, ;, `, +, -, _, =, ?, <, >)
- **Shift+Key** → uppercase letters (A-Z) and Delete (0x7F)
- **Fn+Key** → function codes (0x80-0xAF)

No need to check modifier state separately - CardKeyBoard already sends the correct code!

## Notes

1. **NULL values**: Modifiers (Shift, Sym, Fn) don't send codes by themselves - they modify codes of other keys.

2. **Arrow keys**: Arrows always send the same codes regardless of modifiers (0xB4-0xB7).

3. **Function keys**: Fn+Key combinations send special codes (0x80-0xAF) that can be used for special functions in application.

4. **Sym mode**: Sym+Key allows access to symbols that usually require Shift on standard keyboard.

5. **Case sensitivity**: Shift automatically converts letters to uppercase - no need to do it manually.

---

**Source:** CardKeyBoard I2C Protocol Documentation  
**Tested On:** M5Stack Cardputer-Adv (ESP32-S3)  
**Last Updated:** 2025-11-28

