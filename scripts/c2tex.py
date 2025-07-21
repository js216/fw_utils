# SPDX-License-Identifier: MIT
"""
c2tex.py - Convert C source files to LaTeX documentation with syntax
highlighting

This program converts C source files into LaTeX source that includes
syntax-highlighted code listings and LaTeX-formatted documentation extracted
from special comments.

Author: Jakob Kastelic
Copyright (c) 2025 Stanford Research Systems, Inc.
"""

import sys
import re
import os
from typing import List, Tuple, Optional


class C2TexConverter:
    """Main converter class for C to LaTeX conversion."""

    def __init__(self):
        # Doxygen-style tokens that get converted to LaTeX commands
        self.tokens = {
            '@file': r'\file',
            '@brief': r'\brief',
            '@author': r'\author',
            '@func': r'\begin{function}',
            '@endfunc': r'\end{function}',
            '@param': r'\param',
            '@return': r'\return',
            '@subsection': r'\subsection',
            '@subsubsection': r'\subsubsection*',
            '@license': r'\license',
            '@paragraph': r'\paragraph',
            '@api': r'\api',
            '@begin': r'\begin',
            '@end': r'\end',
            '@item': r'\item',
            '@allfunc': r'',  # Special token - will be replaced with accumulated code
        }

        # LaTeX special characters that need escaping
        self.latex_special_chars = {
            '\\': r'\textbackslash{}',
            '{': r'\{',
            '}': r'\}',
            '$': r'\$',
            '&': r'\&',
            '%': r'\%',
            '#': r'\#',
            '^': r'\textasciicircum{}',
            '_': r'\_',
            '~': r'\textasciitilde{}',
        }

        # Formatting defaults
        self.allfunc_code_margin = "0ex"
        self.comment_code_margin = "5ex"

        # State for @func/@endfunc accumulation
        self.inside_func_block = False
        self.accumulated_func_code = []
        self.allfunc_placeholders = []  # List of (index, content) for replacement

    def escape_latex(self, text: str) -> str:
        """Escape LaTeX special characters in text."""
        result = text
        for char, replacement in self.latex_special_chars.items():
            result = result.replace(char, replacement)
        return result

    def process_inline_code(self, text: str) -> str:
        """Convert inline code marked with backticks, *emph*, or **bold**."""
        def apply_inline_format(text, pattern, latex_cmd, escape=False):
            def replacer(m):
                content = m.group(1)
                if escape:
                    content = self.escape_latex(content)
                return f'\\{latex_cmd}{{{content}}}'
            return re.sub(pattern, replacer, text)

        text = apply_inline_format(text, r'(?<!`)`([^`]+)`(?!`)', 'texttt', escape=True)
        text = apply_inline_format(text, r'(?<!\*)\*\*([^*]+)\*\*(?!\*)', 'textbf')
        text = apply_inline_format(text, r'(?<!\*)\*([^*]+)\*(?!\*)', 'emph')

        return text

    def process_comment_content(self, content: str, result_lines_ref: list = None) -> str:
        """Process documentation comment content into LaTeX."""
        lines = content.split('\n')
        base_indent = self._get_base_indentation(lines)
        result_lines = []
        i = 0

        while i < len(lines):
            i = self._process_single_line(lines, i, base_indent, result_lines, result_lines_ref)

        return '\n'.join(result_lines)

    def _get_base_indentation(self, lines: list) -> int:
        """Determine the base indentation level from non-empty lines."""
        base_indent = None
        for line in lines:
            if line.strip():  # Skip empty lines
                indent = len(line) - len(line.lstrip())
                if base_indent is None or indent < base_indent:
                    base_indent = indent
        return base_indent if base_indent is not None else 0

    def _process_single_line(self, lines: list, i: int, base_indent: int, result_lines: list, result_lines_ref: list = None) -> int:
        """Process a single line and return the next index to process."""
        original_line = lines[i]
        line = original_line.strip()

        # Handle blank lines
        if not line:
            result_lines.append('')
            return i + 1

        # Calculate relative indentation
        line_indent = len(original_line) - len(original_line.lstrip())
        relative_indent = line_indent - base_indent

        # Check for tokens first
        token_match = self._find_token_match(line)
        if token_match:
            return self._process_token(lines, i, token_match, result_lines, result_lines_ref)

        # Check for code blocks
        if relative_indent >= 4:
            return self._process_code_block(lines, i, base_indent, result_lines)

        # Regular text line
        escaped_line = line
        processed_line = self.process_inline_code(escaped_line)
        result_lines.append(processed_line)
        return i + 1

    def _find_token_match(self, line: str) -> tuple:
        """Find if line starts with a Doxygen token, return (token, latex_cmd) or None."""
        for token, latex_cmd in self.tokens.items():
            if line.startswith(token):
                return (token, latex_cmd)
        return None

    def _process_token(self, lines: list, i: int, token_match: tuple, result_lines: list, result_lines_ref: list = None) -> int:
        """Process a Doxygen token and its content."""
        token, latex_cmd = token_match
        line = lines[i].strip()
        token_content = line[len(token):].strip()

        # Handle special @func/@endfunc/@allfunc tokens
        if token == '@func':
            self.inside_func_block = True
        elif token == '@endfunc':
            self.inside_func_block = False
        elif token == '@allfunc':
            # Store placeholder for later replacement
            if result_lines_ref is not None:
                placeholder_index = len(result_lines_ref) + len(result_lines)
                self.allfunc_placeholders.append(placeholder_index)
            result_lines.append('__ALLFUNC_PLACEHOLDER__')
            return i + 1

        # Collect multi-line content
        full_content = [token_content] if token_content else []
        i += 1

        while i < len(lines):
            next_line = lines[i].strip()
            if not next_line:  # Blank line ends token content
                break

            # Check if next line starts with another token
            if self._is_token_line(next_line):
                i -= 1  # Back up to process this token
                break

            full_content.append(next_line)
            i += 1

        # Format the token content
        content_text = ' '.join(full_content)
        processed_content = self.process_inline_code(content_text)
        result_lines.append(f'{latex_cmd}{{{processed_content}}}\n')
        return i + 1

    def _is_token_line(self, line: str) -> bool:
        """Check if line starts with any Doxygen token."""
        return any(line.startswith(token) for token in self.tokens.keys())

    def _process_code_block(self, lines: list, i: int, base_indent: int, result_lines: list) -> int:
        """Process a code block (indented 4+ spaces from base)."""
        code_lines = []

        while i < len(lines):
            current_line = lines[i]
            current_stripped = current_line.strip()

            if not current_stripped:
                code_lines.append('')
                i += 1
                continue

            current_indent = len(current_line) - len(current_line.lstrip())
            current_relative = current_indent - base_indent

            if current_relative >= 4:
                code_content = self._extract_code_content(current_line, base_indent)
                code_lines.append(code_content)
            else:
                break

            i += 1

        self._add_code_block_to_result(code_lines, result_lines)
        return i

    def _extract_code_content(self, line: str, base_indent: int) -> str:
        """Extract code content by removing base indent + 4 spaces."""
        if len(line) > base_indent + 4:
            return line[base_indent + 4:]
        return ''

    def _add_code_block_to_result(self, code_lines: list, result_lines: list) -> None:
        """Clean up code block and add to result if it has content."""
        # Remove trailing blank lines
        while code_lines and not code_lines[-1].strip():
            code_lines.pop()

        if not code_lines:
            return

        # Remove leading blank lines
        while code_lines and not code_lines[0].strip():
            code_lines.pop(0)

        # Remove trailing blank lines again
        while code_lines and not code_lines[-1].strip():
            code_lines.pop()

        if code_lines:  # Only add if there's actual content left
            result_lines.append(f'\\begin{{lstlisting}}[style=C99,xleftmargin={self.comment_code_margin}]')
            result_lines.extend(code_lines)
            result_lines.append('\\end{lstlisting}')

    def parse_file(self, filepath: str) -> str:
        """Parse a single C file and convert to LaTeX."""
        # Reset state for each file
        self.inside_func_block = False
        self.accumulated_func_code = []
        self.allfunc_placeholders = []

        content = self._read_file_content(filepath)
        lines = content.split('\n')
        result = []
        i = 0

        while i < len(lines):
            i = self._process_line(lines, i, result)

        # Replace @allfunc placeholders with accumulated function code
        result_str = '\n'.join(result)
        if self.allfunc_placeholders and self.accumulated_func_code:
            func_code_listing = self._generate_func_code_listing()
            result_str = result_str.replace('__ALLFUNC_PLACEHOLDER__', func_code_listing)
        else:
            # Remove placeholders if no accumulated code
            result_str = result_str.replace('__ALLFUNC_PLACEHOLDER__', '')

        return result_str

    def _generate_func_code_listing(self) -> str:
        """Generate LaTeX listing for accumulated function code."""
        if not self.accumulated_func_code:
            return ''

        # Remove trailing blank lines
        trimmed_code = self.accumulated_func_code[:]
        while trimmed_code and not trimmed_code[-1].strip():
            trimmed_code.pop()

        if not trimmed_code:
            return ''

        result = [f'\\begin{{lstlisting}}[style=C99,xleftmargin={self.allfunc_code_margin}]']
        result.extend(trimmed_code)
        result.append('\\end{lstlisting}')
        return '\n'.join(result)

    def _read_file_content(self, filepath: str) -> str:
        """Read file content with error handling."""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                return f.read()
        except IOError as e:
            print(f"Error reading file {filepath}: {e}", file=sys.stderr)
            sys.exit(1)

    def _process_line(self, lines: list, i: int, result: list) -> int:
        """Process a single line and return next index."""
        line = lines[i]
        current_line_num = i + 1

        if '/**' in line:
            return self._handle_block_comment(lines, i, current_line_num, result)
        elif line.strip().startswith('///'):
            return self._handle_line_comment(lines, i, result)
        else:
            return self._handle_regular_code(lines, i, result)

    def _handle_block_comment(self, lines: list, i: int, current_line_num: int, result: list) -> int:
        """Handle /** ... */ block comments."""
        line = lines[i]
        start_idx = line.find('/**')

        # Extract any code before the comment
        if start_idx > 0:
            code_before = line[:start_idx]
            self._process_code_before_comment(code_before, current_line_num, result)

        # Extract comment content
        comment_lines = []
        current_line_content = line[start_idx + 3:]  # Skip /**

        # Check if comment ends on same line
        if '*/' in current_line_content:
            return self._handle_single_line_block_comment(
                current_line_content, line, current_line_num, comment_lines, result, i
            )
        else:
            return self._handle_multi_line_block_comment(
                lines, i, current_line_content, comment_lines, result
            )

    def _process_code_before_comment(self, code_before: str, current_line_num: int, result: list) -> None:
        """Process code that appears before a comment."""
        code_before = code_before.rstrip()
        if not code_before:
            return

        # Accumulate if inside func block
        if self.inside_func_block:
            self.accumulated_func_code.append(code_before)

        code_before_lines = code_before.split('\n')
        actual_code_start_line_offset = 0
        for l in code_before_lines:
            if not l.strip():
                actual_code_start_line_offset += 1
            else:
                break

        # Only add listing if there's actual code content
        trimmed_code_before = '\n'.join(line for line in code_before_lines if line.strip())
        if trimmed_code_before:
            result.append(f'\\begin{{lstlisting}}[style=C99, numbers=left, firstnumber={current_line_num + actual_code_start_line_offset}]')
            result.append(code_before)
            result.append('\\end{lstlisting}')

    def _handle_single_line_block_comment(self, current_line_content: str, original_line: str,
                                        current_line_num: int, comment_lines: list, 
                                        result: list, i: int) -> int:
        """Handle block comment that starts and ends on the same line."""
        end_idx = current_line_content.find('*/')
        comment_content = current_line_content[:end_idx].strip()
        if comment_content:
            comment_lines.append(comment_content)

        # Check for code after comment on same line
        remaining = current_line_content[end_idx + 2:].strip()
        if remaining:
            result.append(self.process_comment_content('\n'.join(comment_lines), result))

            # Accumulate if inside func block
            if self.inside_func_block:
                self.accumulated_func_code.append(original_line)

            # Code after comment on the same line, needs new listing with correct number
            result.append(f'\\begin{{lstlisting}}[style=C99, numbers=left, firstnumber={current_line_num}]')
            result.append(original_line)  # Add the entire original line
            result.append('\\end{lstlisting}')

            return i + 1  # Move to next line and continue

        # Process comment if no code after
        if comment_lines:
            result.append(self.process_comment_content('\n'.join(comment_lines), result))

        return i + 1

    def _handle_multi_line_block_comment(self, lines: list, i: int, current_line_content: str,
                                       comment_lines: list, result: list) -> int:
        """Handle multi-line block comment."""
        # Multi-line comment
        if current_line_content.strip():
            comment_lines.append(current_line_content)  # Keep original spacing

        i += 1
        while i < len(lines):
            line = lines[i]
            if '*/' in line:
                return self._handle_multi_line_comment_end(line, i, comment_lines, result)
            else:
                clean_line = self._clean_multi_line_comment_line(line)
                comment_lines.append(clean_line)
            i += 1

        # Process comment if no end found (shouldn't happen with valid code)
        if comment_lines:
            result.append(self.process_comment_content('\n'.join(comment_lines), result))

        return i

    def _handle_multi_line_comment_end(self, line: str, i: int, comment_lines: list, result: list) -> int:
        """Handle the end of a multi-line block comment."""
        end_idx = line.find('*/')
        comment_part = line[:end_idx]

        # Remove leading * but preserve indentation after it
        comment_part = self._clean_comment_part(comment_part)
        if comment_part or not comment_part.strip():  # Include blank lines
            comment_lines.append(comment_part)

        # Check for code after comment
        remaining = line[end_idx + 2:].strip()
        if remaining:
            result.append(self.process_comment_content('\n'.join(comment_lines), result))

            # Accumulate if inside func block
            if self.inside_func_block:
                self.accumulated_func_code.append(line)

            # Code after multi-line comment, needs new listing with correct number
            result.append(f'\\begin{{lstlisting}}[style=C99, numbers=left, firstnumber={i+1}]')
            result.append(line)
            result.append('\\end{lstlisting}')

            return i + 1  # Move to next line and continue

        # Process the collected comment
        if comment_lines:
            result.append(self.process_comment_content('\n'.join(comment_lines), result))

        return i + 1

    def _clean_multi_line_comment_line(self, line: str) -> str:
        """Clean a line inside a multi-line comment."""
        clean_line = line
        if line.strip().startswith('*'):
            star_idx = line.find('*')
            if star_idx != -1 and star_idx + 1 < len(line):
                after_star = line[star_idx + 1:]
                if after_star.startswith(' '):
                    clean_line = after_star[1:]  # Remove the single space after *
                else:
                    clean_line = after_star
            else:
                clean_line = ''
        return clean_line

    def _clean_comment_part(self, comment_part: str) -> str:
        """Clean comment part by removing leading * but preserving indentation."""
        if comment_part.strip().startswith('*'):
            star_idx = comment_part.find('*')
            if star_idx != -1 and star_idx + 1 < len(comment_part):
                after_star = comment_part[star_idx + 1:]
                if after_star.startswith(' '):
                    return after_star[1:]  # Remove the single space after *
                else:
                    return after_star
            else:
                return comment_part.strip()
        return comment_part

    def _handle_line_comment(self, lines: list, i: int, result: list) -> int:
        """Handle /// line comments."""
        comment_lines = []

        while i < len(lines) and lines[i].strip().startswith('///'):
            comment_content = lines[i].strip()[3:].strip()
            comment_lines.append(comment_content)
            i += 1

        i -= 1  # Back up one since we'll increment at end of loop

        # Process the collected comments
        if comment_lines:
            processed_comment = self.process_comment_content('\n'.join(comment_lines), result)
            result.append(processed_comment)

        return i + 1

    def _handle_regular_code(self, lines: list, i: int, result: list) -> int:
        """Handle regular code lines."""
        code_lines = []
        code_start_line = i + 1  # Tentative start line

        # Collect consecutive non-documentation lines
        while i < len(lines):
            current = lines[i]
            # Stop if we hit a documentation comment
            if ('/**' in current or current.strip().startswith('///')):
                break
            code_lines.append(current)
            i += 1

        i -= 1  # Back up one since we'll increment at end of loop

        # If inside func block, accumulate instead of outputting
        if self.inside_func_block:
            self._accumulate_func_code(code_lines)

        self._add_code_block_with_trimming(code_lines, code_start_line, result)

        return i + 1

    def _accumulate_func_code(self, code_lines: list) -> None:
        """Accumulate code lines for later @allfunc output."""
        # Count leading blank lines
        leading_blank_lines = 0
        for l in code_lines:
            if not l.strip():
                leading_blank_lines += 1
            else:
                break

        # Trim leading blank lines
        trimmed_code_lines = code_lines[leading_blank_lines:]

        # Trim trailing blank lines
        while trimmed_code_lines and not trimmed_code_lines[-1].strip():
            trimmed_code_lines.pop()

        # Add to accumulated code
        self.accumulated_func_code.extend(trimmed_code_lines)

    def _add_code_block_with_trimming(self, code_lines: list, code_start_line: int, result: list) -> None:
        """Add code block after trimming leading and trailing blank lines."""
        # Count leading blank lines
        leading_blank_lines = 0
        for l in code_lines:
            if not l.strip():
                leading_blank_lines += 1
            else:
                break

        # Trim leading blank lines
        trimmed_code_lines = code_lines[leading_blank_lines:]
        actual_code_start_line = code_start_line + leading_blank_lines

        # Trim trailing blank lines
        while trimmed_code_lines and not trimmed_code_lines[-1].strip():
            trimmed_code_lines.pop()

        if trimmed_code_lines:
            result.append(f'\\begin{{lstlisting}}[style=C99, numbers=left, firstnumber={actual_code_start_line}]')
            result.extend(trimmed_code_lines)
            result.append('\\end{lstlisting}')

    def convert_files(self, filepaths: List[str]) -> str:
        """Convert multiple C files to LaTeX."""
        results = []

        for filepath in filepaths:
            if not os.path.exists(filepath):
                print(f"Error: File {filepath} does not exist", file=sys.stderr)
                sys.exit(1)

            # Add file header
            results.append(f'% File: {filepath}')
            results.append('')

            # Convert file content
            file_result = self.parse_file(filepath)
            results.append(file_result)
            results.append('')  # Add blank line between files

        return '\n'.join(results)


def print_usage():
    """Print usage information."""
    print("Usage: python3 c2tex.py file1.c file2.h ...", file=sys.stderr)
    print("", file=sys.stderr)
    print("Convert C source files to LaTeX documentation with syntax highlighting.", file=sys.stderr)
    print("", file=sys.stderr)
    print("Features:", file=sys.stderr)
    print("  - Syntax-highlighted code listings with original line numbers", file=sys.stderr)
    print("  - Documentation from /** ... */ and /// comments", file=sys.stderr)
    print("  - Doxygen-style tokens (@file, @brief, @author, @func, @param, @return)", file=sys.stderr)
    print("  - Inline code with backticks and code blocks with 4-space indent", file=sys.stderr)
    print("  - @allfunc token to output accumulated code from @func/@endfunc blocks", file=sys.stderr)
    print("", file=sys.stderr)
    print("LaTeX requirements:", file=sys.stderr)
    print("  \\usepackage{listings}", file=sys.stderr)
    print("  \\usepackage{xcolor}", file=sys.stderr)


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)

    # Print LaTeX setup comment
    print("% Generated by c2tex.py")
    print("% Requires: \\usepackage{listings} and \\usepackage{xcolor}")
    print("% Configure with: \\lstset{basicstyle=\\ttfamily\\small, frame=none}")
    print("")

    filepaths = sys.argv[1:]
    converter = C2TexConverter()

    try:
        latex_output = converter.convert_files(filepaths)
        print(latex_output)
    except Exception as e:
        print(f"Error during conversion: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
