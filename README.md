# Blaissh - A Custom Shell Implementation

Blaissh is a custom shell implementation in C that provides basic shell functionality including command execution, piping, I/O redirection, and command history.

## Features

- Command execution
- Pipe support for command chaining
- Input/Output redirection
- Command history (last 15 commands)
- Built-in commands:
  - `cd`: Change directory
  - `cwd`: Print current working directory
  - `echo`: Print text to stdout
  - `history`: Show command history
  - `bye`: Exit the shell

## Building

To build blaissh, you need:
- GCC compiler
- Make
- UNIX-like operating system (Linux, macOS)

```bash
make
make all
```

## Usage
After buliding, run the shell:
```bash
./blaissh
```

Example commands:
```bash
# Basic command execution
ls -l

# Piping commands
ls | grep .txt

# I/O redirection
ls > output.txt
cat < input.txt

# Show command history
history
```

## Development
The shell is implemented in two main files:
- src/blaissh.c: Main shell implementation
- src/cmd_parse.h: Command parsing definitions and structures

## License
MIT License
