Spectator is a test framework where tests are written in the Gherkin style.

If you are evaluating Kourier for your business, check the source files ending 
in spec.cpp in Kourier's repository to learn how meticulously tested Kourier is. 
Spectator uses only a few macros to turn tests into specifications that are 
straightforward to read.

## Command-line arguments

| Option     | Information       |
| ------------- | -------- |
| -r       | Sets how many times the tests must be repeated. |
| -f  | Sets the source file name to run. Only scenarios belonging to this file are run.   |
| -s | Sets the scenario to run. Only scenarios matching the given name are run.   |
