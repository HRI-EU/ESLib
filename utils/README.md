# Event System Offline Analysis and Visualization Tools

This directory contains several tools for performing static, off-line, analysis of the Event System in a project. Included are tools for gathering data about event usage, tools for validating event usage (e.g. to verify at build time that no events are used with conflicting type signatures in different locations), and tools for visualizing event usage (location of publishers, subscribers, etc).

## Script Usage

### Prerequisites:
To run the scripts (including via CMake) you must have:
* Python 3
* llvm 6+
* matching `libclang`, `libclang-dev`, and the python package for libclang (`clang`). These scripts have only been verified against libclan 6.0.
* Additional Python packages: `tqdm`, `colorama`
* CMake 3.5+
 
Before running the scripts, you also need to ensure libclang is in your `LD_LIBRARY_PATH`. Run the command `llvm-config --libdir` and add the result to your `LD_LIBRARY_PATH`.

### Usage Details:
There are three python scripts (usage subject to change, but likely not by much):

#### event-scan.py
Analyzes the codebase searching for calls to subscribe, publish, and registerEvent. Produces a JSON file with the details for each.

`python event-scan.py <path to dir containing compile_comands.json> <path to output file> [<path to source root>] [<file1;file2;file3>]`

CMake will generate the `compile_commands.json` file in your build directory (e.g. `/build/rcs/`). The script expects the DIRECTORY, not the file path.

The third argument is the path to the root of your source tree. Any files outside this directory will be ignored, even if they are `#include`d from something in your project. This is primarily to prevent the parser from running through stdlib, boost, etc., but could also be used to restrict the evaluation to a specific portion of a project. If not specified, defaults to `/` and will parse *everything* (not recommended).

The fourth argument is an optional list of semicolon-separated filenames. If given, then ONLY these files will be searched (provided they also exist in compile_commands.json). If this list is not provided, then every file in `compile_commands.json` will be searched.

`scan` will parse all files in parallel, which saves a considerable amount of time, but at the moment it is unrestricted and will consume all the system resources it possibly can. A more friendly and intelligent work load organization will be handled in the future...

NOTE: If you do not see `compile_commands.json` in your build directory, configure cmake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=1`  There is a [known issue](https://gitlab.kitware.com/cmake/cmake/issues/16588) with CMake which can require CMake to be run twice before `compile_commands.json` is created.

NOTE: If your codebase includes any *generated* .cpp files, these will be included in `compile_commands.json` even if you have not yet run whatever part of your build process generates them. In this case, you should run a normal build before performing any event analysis in order to ensure that all relevant files exist before the system attempts to parse them.

#### event-check.py
Given the output from `event-scan.py`, checks for signature mismatches in event usage. For example: `subscribe<int>("MyEvent", ...)` and `publish<std::string>("MyEvent", ...)`

`python event-check.py <path to input file> [<path to git repo>]`

If the second parameter is given, it the script will not only produce a listing of mismatching event uses, but will also print information from `git blame` for each instance using the git repository specified. This should only be used when the contents of the given git repository are exactly the source files analyzed by `scan.py`.

#### event-graph.py
Given the output from `event-scan.py`, produces a [cytoscape](http://js.cytoscape.org/) graph compatible with the HTML visualization tool in [event_visualization](event_visualization).

`python event-graph.py <path to input file> <path to output file>`

To view the graph, open [event-graph.html](event_visualization/event-graph.html) in your browser, then drag and drop the output file produced by `out2js` into the browser window.

#### gen-event-page.py
Given the output from `event-graph.py` and a template HTML page, generates an output HTML page which contains the event graph and loads it at startup. This can be used to generate a standalone page for a specific graph, e.g. to include in documentation.

`python gen-event-page.py html_template_file graph_input_file html_output_file`

The script uses a very simple string templating scheme, which can be seen at the bottom of [event-graph.html](event_visualization/event-graph.html). Templated regions are described in special comment blocks, which are then uncommented in the output. This allows the page to be used as-is without applying any template parameters.

Template blocks are marked as:

```js
/** +-+
    {:Template:} Code goes here
    -+- **/
```

The first and last lines (containing the `+-+` and `-+-` markers) will be stripped from the output, so all templated code should be on the lines between them.
A page can contain any number of template blocks.

To include templated parameters in the page, place them inside `{:` and `:}` tags. For example: `{: graph_name :}`. Template parameters must be one word, but any amount of white space may appear between the parameter name and the opening/closing tags. 

Currently-supported template parameters:

| Parameter Name   | Result                                    |
| ---------------- | ----------------------------------------- |
| `graph`          | Graph JSON (produced by `event-graph.py`) |
| `graph_name`     | Filename graph was loaded from            |

To add additional parameters, you will need to edit `gen-event-page.py`. 


## CMake Integration
The scripts above can be used with CMake to verify event usage is consistent at build-time. This can be done over the entire codebase, or just for one specific target.

This requires CMake 3.5, which you can source from `/hri/sit/latest/External/CMake/3.5.2/BashSrc`

There is a global build target named `validate_all_events`, which will parse and compare ALL events found in ALL source files according to your current CMake configuration. To use it, simply run `make validate_all_events`

You can add a post-build step to validate the events just for a specific target with the command `target_validate_events( target_name )`. This will run the `event-scan` and `event-check` tools after compilation succeeds. If `event-check` detects any event mismatches, it will fail the build. This will also generate a `{target}_eventgraph.json` file in your build directory, which can be used with [event-graph.html](event_visualization/event-graph.html).

### Notes on using CMake to validate events:

* When analyzing a specific build target, we try to match the source filenames necessary to build that target against the list of source files and compile commands in the compile_commands.json file produced by CMake. If there's any mismatch between the two, we may unintentionally ignore some files. There is not a reliable built-in way to get this list in all possible configurations (as far as I can tell, at least), but if we keep our build system tidy it should be possible.
  * In the current implementation, CMake can only collect the dependencies for a given target if all the dependent targets have already been processed. This means that if `TestRobo` depends on `RcsWheelDemo`, and they are located in different directories each included in the project via `add_subdirectory()`, then the directory containing the `CMakeLists.txt` which explains `RcsWheelDemo` must be added BEFORE the directory containing `TestRobo`. If this is not done, then we will simply ignore `RcsWheelDemo`'s source files when searching for events.
  * Depending on how a `.cpp` file is added to the project, it might either be seen as `file.cpp` in `/directory/`, or as `/directory/file.cpp`. Almost all of the files in Rcs follow the first format, so we can access the filename through `SOURCES` and the directory through `SOURCE_DIR`, but if we attempt to do that for a file with the second format the result is `/directory//directory/file.cpp`. I don't think we can detect this in CMake and the event analysis scripts don't check for it, so these files will also not be parsed. I believe the use of `aux_source_directory()` in `RcsGraphics` causes this, but there may be other places doing something similar.
* When analyzing the entire Rcs codebase, no filtering of files is done and we will parse everything.
* When you use `target_validate_events()`, a JSON file named `target_eventdata.json` will be created in your build directory. This includes the details about every detected event call, including nearby comments, and can be used to verify the sanity of the events' usage or visualize them by passing them to `out2js.py`.

## Notes on Code Comments
`event-scan.py` will not only search for event usage, but will also attempt to extract comments from the nearby code. The primary goal of this is to allow for one to provide some documentation for `subscribe()` and `publish()` calls, which effectively form part of the interface to a module but are not able to be detected by the likes of Doxygen.

### Documenting event calls
Any call to `subscribe()`, `publish()`, or `registerEvent()` can include a comment on the line(s) above the call, and this comment will be extracted by `scan.py`. The comment will then be included in any output from `analyze.py` as well as in the event graph visualization. Both single- and multi-line comments are supported, but the last line of the comment must always be the line immediately preceding the call. Single-line comments on the same line as the call are also not currently extracted.

```cpp
// This comment will be extracted
subscribe("UpdateGraph", &onUpdateGraph);

/*
 * This comment will be extracted
 */
registerEvent<int>("PushButton");

/// This comment
/// will also
/// be extracted
publish("PushButton", 7);

subscribe("Jump", &onJump); // This comment will NOT be extracted

// This comment will also NOT be extracted

publish("Jump");
```

### Documenting subscriber callbacks
In addition to extracting comments near the **call** to `subscribe()`, the system will also attempt to extract comments from any non-lambda callback function **passed** to `subscribe()`. In this case, comments must either begin with `///` or be a clearly "documenting" multiline comment. They can appear either in the `.cpp` or `.h` file, but if a compatible comment exists in both then only the one in the `.cpp` file will be extracted.

```cpp
///
/// This comment will be extracted
///
void onJump();

/**********************************
 *   This comment wil be extracted
 *********************************/
void onPushButton(int btn);

// This comment will not be extracted
void onUpdateGraph(RcsGraph* graph);

/*
 * This comment will not be extracted
 */
void onPlanToAngle(double angle);
```


```cpp
// prog.h

///
/// This comment wil be extracted
///
void onJump();

----
// prog.cpp

///
/// This comment will be ignored
///
void onJump() { ... }
