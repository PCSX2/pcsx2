# Documentation Artifacts

-   Source Directory: `/pcsx2/pcsx2/Docs`
-   Release Directory: `/pcsx2/bin/Docs`

Docs should be written in the source directory first in an easily editable format. Currently, Markdown is the preferred format due to its simple markup and easy portability. GitHub's built-in preview functions are a huge benefit as well.

Visual Studio Code is a cross platform text editor/development platform that can handle Markdown syntax, plus extensions are available to enable in-editor previewing and PDF generation. However, this is not a requirement since other editors will support Markdown, and worst case GitHub supports editing Markdown files in-browser.

## How to Generate

### Setup

To generate the documentation artifacts into the release directory, you will require the following:

-   `pandoc`
    - Converts from Markdown to PDF

-   `miktex` or something similar that provides `pdflatex`
    - this is what generates the PDF file

#### Linux / MacOS

Consult pandoc's official documentation - https://pandoc.org/installing.html

#### Windows

In a PowerShell shell, run the following:

```ps1
> iwr -useb get.scoop.sh | iex
> scoop install pandoc latex
```

If you prefer Chocolatey or using the installer, consult pandoc's official documentation - https://pandoc.org/installing.html

### Usage

Run the following, this assumes you have access to bash, either by virtue of running on linux or through something like `git-bash` on Windows.

```bash
> ./gen-docs.sh
```

> You can override the default output directory like so - `OUT_DIR=<PATH> ./gen-docs.sh`

### Customizing Output

For generating the PDF's this popular template is used and has many features that could be useful https://github.com/Wandmalfarbe/pandoc-latex-template#usage reference it's documentation to take advantage of those if desired.
