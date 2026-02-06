# Coding Style

This project follows the **Linux kernel coding style** where practical, with a few small project-specific notes.

The goal is consistency and readability, not perfection. Some files (notably opcode tables and long format strings) may exceed 80 columns when breaking lines would hurt clarity.

## Quick rules

- **Indentation:** tabs, 8-column tab stops.
- **Line length:** aim for **<= 80 columns**.
  - Exceptions: large `switch` tables (CPU core), long string literals, and log/help text.
- **Braces:**
  - **Functions:** opening brace on the **next line**.

    ```c
    static int foo(int x)
    {
    	return x + 1;
    }
    ```

  - **Control blocks:** opening brace on the **same line**.

    ```c
    if (x > 0) {
    	bar();
    }
    ```

- **Spaces:**
  - `if (cond)` / `for (i = 0; i < n; i++)` (space after keyword)
  - spaces around binary operators: `a + b`, `x |= y`
- **Pointers:** `type *name` (asterisk binds to the name).
- **Comments:** prefer `/* ... */` for block comments; keep them short and factual.

## Formatting

No project-specific formatter is shipped. If you choose to auto-format locally, configure your editor to match the rules above (tabs, 8-wide, kernel-ish brace placement).

A `.clang-format` file is included for convenience; use it only if it produces acceptable results for the changed code.

## Tests

- Assertions SHOULD use Yoda-style comparisons: `expected == actual`.

## Automated checks

The repo includes a small style gate:

```sh
./tools/check_style.sh
```

It enforces a minimal subset:

- Fails on trailing whitespace
- Fails on `#pragma once` in headers (prefer include guards)
- Fails on `//` comments in headers (prefer `/* */`)
- Warns on lines > 80 columns (informational)
