# C Fundamentals – Exercise Solutions

This repository contains my solutions to the exercises from the [C Fundamentals](https://frontendmasters.com/courses/c-fundamentals/) course on Frontend Masters.

## Usage

Run `make <target>` to execute a command. Use `make help` to see all available targets.

### Common Targets

- **help** – Show available make targets and their descriptions
- **check** – Verify if `clang` and `lldb` are installed
- **create-dir** – Create the `bin` directory if it doesn’t exist
- **build-main** – Compile the main program (`src/main.c` → `bin/main`)
- **build-test** – Compile the test program (`src/test.c` → `bin/test`)
- **run** – Build and run the main program
- **test** – Build and run the test program
- **debug** – Run the main program in the debugger (`lldb`)
- **debug-test** – Run the test program in the debugger (`lldb`)
- **clean** – Remove the `bin` directory and all compiled binaries
