## C-Shell

### Overview

A custom Unix shell implementation in C providing basic shell functionality:

* Command execution
* I/O redirection
* Pipelines
* Background processes
* Job control
* Built-in commands
* Advanced features: command history, process management, signal handling.

---

### 🌟 Key Features                        

* **Interactive CLI**: Custom prompt showing `user@hostname:directory`
* **Command Parsing**: Supports pipes, redirections, operators
* **Built-in Commands**: `hop`, `reveal`, `log`, `activities`, `ping`, `fg`, `bg`
* **I/O Redirection**: `<`, `>`, `>>`
* **Pipelines**: `|`
* **Background Processes**: `&`
* **Job Control**: Foreground/background job management
* **Signal Handling**: Handles `Ctrl+C`, `Ctrl+Z`, `Ctrl+D`
* **Command History**: Persistent history with execution

---

### 📂 Project Structure

```bash
C-Shell/
├── Makefile              # Build configuration
├── include/              # Header files
│   ├── activities.h
│   ├── execute.h
│   ├── fg_bg.h
│   ├── hop.h
│   ├── log.h
│   ├── parser.h
│   ├── ping.h
│   ├── prompt.h
│   ├── reveal.h
│   └── signals.h
└── src/                  # Source files
    ├── activities.c
    ├── execute.c
    ├── fg_bg.c
    ├── hop.c
    ├── log.c
    ├── main.c
    ├── parser.c
    ├── ping.c
    ├── prompt.c
    ├── reveal.c
    └── signals.c
```

---

### ⚙️ Build System

```bash
make all       # Build the shell
make clean     # Clean generated files
```

---

### 💡 Usage Examples

#### Basic Commands

```bash
<user@hostname:~> ls
<user@hostname:~> hop /home/user/documents
<user@hostname:~/documents> reveal -la
<user@hostname:~/documents> hop -
<user@hostname:~>
```

#### I/O Redirection

```bash
<user@hostname:~> echo "Hello World" > output.txt
<user@hostname:~> cat output.txt
Hello World
<user@hostname:~> echo "Second line" >> output.txt
<user@hostname:~> cat < output.txt
Hello World
Second line
```

#### Pipelines

```bash
<user@hostname:~> ls -la | grep txt | wc -l
3
<user@hostname:~> cat /etc/passwd | grep bash | cut -d: -f1
root
user
```

#### Background Processes

```bash
<user@hostname:~> sleep 30 &
[1] 12345
<user@hostname:~> activities
[12345] : sleep 30 & - Running
<user@hostname:~> fg 1
sleep 30
^Z
[1] Stopped sleep 30
```

#### Command History

```bash
<user@hostname:~> echo "test command"
test command
<user@hostname:~> ls -la
. ..
<user@hostname:~> log
ls -la
echo "test command"
<user@hostname:~> log execute 1
. ..
```

#### Signal Handling

```bash
<user@hostname:~> sleep 10
^C
<user@hostname:~> sleep 10 &
[1] 12346
<user@hostname:~> ping 12346 15
Sent signal 15 to process with pid 12346
sleep 10 with pid 12346 exited normally
```

---

### ⚡ Technical Details

#### Memory Management

* Dynamic allocation for command structures
* Cleanup with `free_group_list()`
* Circular buffer for command history
* Automatic cleanup of completed background jobs

#### Process Groups

* Background jobs have their own process groups
* Foreground processes form process groups
* Proper signal delivery
* Terminal control management

#### Error Handling

* Comprehensive system call checks
* Graceful invalid command handling
* Proper cleanup on errors
* User-friendly error messages

#### File Descriptor Management

* Proper pipe and redirection handling
* Prevent descriptor leaks

#### POSIX Compliance

* POSIX system calls
* Signal handling via `sigaction`
* Standard file permissions (0644)
* Compatible with Unix utilities

---
