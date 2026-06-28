# Emu68 Code Style Guide

Over many years of development the source code of Emu68 got really chaotic and needs a proper
cleaning up. In future, all the code either contributed or written by me shall follow this coding
style guide.

## Naming Conventions

### File Names
Use **snake_case** for both source and header files.
```cpp
// File: return_stack.hpp
class ReturnStack { };

// File: return_stack.cpp
#include "return_stack.h"
```

**Exception:** For single-class-per-file scenarios, you may match the class name in PascalCase if
preferred, but snake_case is recommended for consistency.

### Directory Structure
Header files should preferably be organized in directories (using **lowercase**) that reflect the namespace hierarchy.

```cpp
// Namespace: Emu68
// Directory: include/emu68/
// File: include/emu68/Node.hpp

// Namespace: Emu68::PPC
// Directory: include/emu68/ppc/
// File: include/emu68/ppc/Translator.hpp

// Namespace: Emu68::M68k
// Directory: include/emu68/m68k/
// File: include/emu68/m68k/Decoder.hpp
```

### Namespaces
All namespaces shall use the PascalCase convention
```cpp
namespace Emu68 { }
namespace Emu68::PPC { }
```

All Emu68 generic c++ code should belong to ``Emu68`` namespace, PowerPC related code shall go into
``Emu68::PPC`` namespace, M68k relevant code shall go into ``Emu68::M68k`` namespace.

### Classes and Structs
Use **PascalCase** for class and struct names.
```cpp
struct TranslatorContext { };
struct RegisterNode { };
```

### Enums
Use **PascalCase** for enum names (both regular enums and enum classes).
```cpp
enum class CPUState { };
enum XMsgType { };
```

### Methods
Use **camelCase** for method names.
```cpp
class Node {
    Node *next();
    void setPrev(Node *p);
};
```

### Free Functions
Use **camelCase** for non-class (free) functions.
```cpp
void initializeHardware();
uint32_t readRegister(uint32_t address);
bool validateConfiguration();
```

### Variables
Use **snake_case** for variable names (including function parameters and class members).
```cpp
uint32_t timer_value;
int pin_number;
bool is_initialized;

class Device {
    uint32_t register_address;
    bool device_ready;
};
```

**Exception:** Private class/struct fields may use a prefix (either standard `m_` or a prefix derived from the class name) to avoid naming conflicts with accessor methods or getters of the same name.

```cpp
class Node {
    Node* n_prev;  // Prefix derived from class name
    Node* n_next;

public:
    Node* prev() { return n_prev; }  // Getter has clean name
    Node* next() { return n_next; }
    void setPrev(Node* p) { n_prev = p; }
    void setNext(Node* n) { n_next = n; }
};

class List {
    Node l_head;  // Prefix derived from class name
    Node l_tail;

public:
    Node* head() { return &l_head; }
    Node* tail() { return &l_tail; }
};

// Alternative with m_ prefix
class Config {
    int m_value;
    bool m_enabled;

public:
    int value() const { return m_value; }
    bool enabled() const { return m_enabled; }
};
```

### Constants, Macros, and Enum Values
Use **SCREAMING_SNAKE_CASE** for:
- Preprocessor macros
- Compile-time constants
- `constexpr` values
- Enum values
- Static inline functions considered as macro alternatives
```cpp
#define MAX_BUFFER_SIZE 1024
#define ENABLE_DEBUG_MODE

const uint32_t SYSTEM_CLOCK_HZ = 48000000;
constexpr int MAX_CHANNELS = 8;

enum StatusCode {
    STATUS_OK,
    STATUS_ERROR,
    STATUS_TIMEOUT
};

enum class PinMode {
    INPUT,
    OUTPUT,
    ALTERNATE_FUNCTION
};
```

### Extension-less Headers
You may add extension-less headers known from c++ but it is not mandatory. If such files are used,
keep them a single line including the header with extension:

```cpp
// File: include/cpp/lists
#include "lists.hpp"
```

## Summary Table

| Element | Convention | Example |
|---------|------------|---------|
| Namespaces | PascalCase | `Hardware`, `Gpio` |
| Classes/Structs | PascalCase | `TimerController`, `GpioConfig` |
| Enums | PascalCase | `StatusCode`, `PinMode` |
| Methods | camelCase | `startTimer()`, `getValue()` |
| Free Functions | camelCase | `initialize()`, `readRegister()` |
| Variables | snake_case | `timer_value`, `pin_number` |
| Constants/Macros/Enum Values | SCREAMING_SNAKE_CASE | `MAX_SIZE`, `STATUS_OK` |

## Formatting Rules

### Indentation
**Use spaces only, never tabs.** Indentation shall be **4 spaces** per level.

```cpp
// Correct
class Example {
    void method() {
        if (condition) {
            doSomething();
        }
    }
};

// Wrong - using tabs or inconsistent spacing
class Example {
	void method() {
	  if (condition) {
	      doSomething();
	  }
	}
};
```

#### Namespace Indentation
Do not indent the content of namespaces to save horizontal space.

```cpp
// Correct
namespace Emu68 {

class MyClass {
    void myMethod();
};

}  // namespace Emu68

// Wrong - unnecessary indentation
namespace Emu68 {

    class MyClass {
        void myMethod();
    };

}  // namespace Emu68
```

#### Switch Statement Indentation
Indent case labels one level above the switch statement, and indent case bodies one level deeper.

```cpp
switch (value) {
    case OPTION_A:
        handleA();
        break;
    case OPTION_B:
        handleB();
        break;
    default:
        handleDefault();
        break;
}
```

### Curly Braces

#### Placement
Use **K&R style** (opening brace on the same line) for classes, control statements, and other blocks.

**Exception:** For **large blocks**, you may place the opening brace on a new line (Allman style) if 
it improves clarity.

```cpp
// Correct - function definition with brace on new line
void function()
{
    if (condition) {
        doSomething();
    } else {
        doSomethingElse();
    }
}

// Correct - class with K&R style if clarity is improved
class MyClass {
    int value;
public:
    void method()
    {
        // implementation
    }
};
```

#### Always Use Braces
Always use braces for control statements (if, else, for, while, do-while), even for single-statement blocks.

```cpp
// Correct
if (condition) {
    singleStatement();
}

for (int i = 0; i < 10; i++) {
    process(i);
}

// Wrong - omitting braces
if (condition)
    singleStatement();

for (int i = 0; i < 10; i++)
    process(i);
```

#### Empty Blocks
For empty blocks, place both braces on the same line.

```cpp
// Correct
void emptyFunction() {}
class EmptyClass {};

// Also acceptable for better visibility
void emptyFunction()
{
}
```

### Line Length
Keep lines to a maximum of **120 characters** when possible. This is a soft limit; readability takes precedence.

### Spacing

#### Operators
Use spaces around binary operators and after commas.

```cpp
// Correct
int result = a + b * c;
function(arg1, arg2, arg3);
for (int i = 0; i < count; i++) {
}

// Wrong
int result=a+b*c;
function(arg1,arg2,arg3);
for (int i=0;i<count;i++) {
}
```

#### Pointers and References
Place the `*` and `&` next to the type, not the variable name.

```cpp
// Correct
int* pointer;
int& reference;
void function(const std::string& str);

// Wrong
int *pointer;
int &reference;
void function(const std::string &str);
```

**Note:** When declaring multiple pointers, prefer separate lines for clarity.
```cpp
// Preferred
int* ptr1;
int* ptr2;

// Acceptable but less clear
int* ptr1, *ptr2;
```

#### Control Statements
Use a space after control statement keywords, but not after function names.

```cpp
// Correct
if (condition) {
}
while (running) {
}
function(args);

// Wrong
if(condition) {
}
while(running) {
}
function (args);
```

### Header Include Order
Organize includes in the following order, with a blank line between groups:

1. Corresponding header (for .cpp files)
2. C system headers
3. C++ standard library headers
4. Third-party library headers
5. Project headers

```cpp
// Example in my_class.cpp
#include "my_class.hpp"

#include <stdint.h>
#include <string.h>

#include <vector>
#include <memory>

#include "third_party/library.h"

#include "project_header.hpp"
#include "another_header.hpp"
```

### Header Guards
Use `#pragma once` instead of traditional include guards for simplicity.

```cpp
// Correct
#pragma once

class MyClass {
    // ...
};

// Avoid - traditional guards (unless needed for specific compatibility)
#ifndef MY_CLASS_HPP
#define MY_CLASS_HPP

class MyClass {
    // ...
};

#endif  // MY_CLASS_HPP
```

### Comments

#### Single-line Comments
Use `//` for single-line comments. Place a space after the slashes.

```cpp
// This is a single-line comment
int value;  // Inline comment explaining the variable
```

#### Multi-line Comments
Use `//` for consecutive lines or `/* */` for block comments.

```cpp
// This is a multi-line comment
// that spans several lines
// and explains something complex

/*
 * Alternative block comment style
 * for longer documentation
 */
```

#### Documentation Comments
For public APIs, use Doxygen-style comments when appropriate.

```cpp
/**
 * Calculates the checksum of a data buffer.
 *
 * @param data Pointer to the data buffer
 * @param length Length of the buffer in bytes
 * @return Calculated checksum value
 */
uint32_t calculateChecksum(const uint8_t* data, size_t length);
```

### Const Correctness
Use `const` wherever possible to indicate immutability.

```cpp
// Const member functions
class Config {
    int getValue() const { return value; }  // Doesn't modify object
    void setValue(int v) { value = v; }     // Modifies object
private:
    int value;
};

// Const parameters
void process(const std::string& input);
void modify(std::string& output);

// Const pointers
const int* ptr;        // Pointer to const int
int* const ptr;        // Const pointer to int
const int* const ptr;  // Const pointer to const int
```

### Namespace Usage
Avoid `using namespace` in header files. In source files, avoid `using namespace std;` but specific using declarations are acceptable.

```cpp
// Header file - wrong
using namespace Emu68;

// Header file - correct
namespace Emu68 {

void function(const std::vector<int>& data);

}
```
