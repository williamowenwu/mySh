# About

This is a basic implementation of a Unix-like shell. A shell is a user interface that allows users to interact with an operating system by executing commands. The given shell is relatively simple and serves various different functionalities.

# Features:

1.  Interactive Mode: Run the shell interactively by executing ./mysh. In this mode, you can enter commands one by one. 


1. Batch Mode: Run the shell in batch mode by providing a series of commands as arguments, like ./mysh "command1 | command2" "command3 > output.txt". It will execute the provided commands sequentially.

1. Piping Commands: You can use the | (pipe) symbol to chain commands together. For example, command1 | command2 pipes the output of command1 as input to command2.

1. Input/Output Redirection: The shell supports input redirection using < and output redirection using >. For instance, sort < input.txt > output.txt reads input from input.txt and writes the output to output.txt.

1. Built-In Commands: The shell includes some built-in commands:
    - cd: Change the current directory.
    - pwd: Print the current working directory.
exit: Exit the shell.
    - exit: Exits the shell

1. Running Paths to Executables: You can execute commands with their full paths. For example, /your/working/directory/test will run the executable located at that path.

<br>

# Usage

To compile the shell, use the command `make`. This will create an executable named mysh.

To run the shell interactively, execute `./mysh`. You can then enter commands one by one and treat it as a regular shell.

To run the shell in batch mode, provide a series of commands as arguments to the executable. For example:

```C
./mysh "command1 | command2" "command3 > output.txt"
```

<br>

# Example Commands

Run a single command:
```
ls
```

Run a single command with args:
```
ls -l
```

Change the current directory:
```
cd /path/to/directory
```

Print the current directory:
```
pwd
```

Run a path to an executable:

```
./your/working/dir/test
```

Pipe commands together:
```
ls | grep.txt
```

Redirect input and output:
```
sort < input.txt > output.txt
```

Exit the shell:
```
exit
```

Run a single command in batch mode with args:

```
./mysh ls -l
```

Redirect input and output using batch mode:
```
./mysh sort < input.txt > output.txt
```

Count the number of lines in a file:
```
wc -l < input.txt
```


Show the last 10 lines of a file:

```
tail -n 10 input.txt
```

List files in the current directory sorted by modification time:

```
ls -lt
```

Create a new file:
```
touch newfile.txt
```

Even use git!:
```
git -commands
```

