# So you want to contribute to PCSX2? Great

As a first step, please review these links as they'll help you understand how the development of the PCSX2 works. These pages are all found on the github [wiki](https://github.com/PCSX2/pcsx2/wiki). You should probably also check out the [forums](http://forums.pcsx2.net/index.php).

*   [Just Starting Out](#just-starting-out)
*   [Issue Reportding](#issue-reportding)
*   [Pull Request Guidelines](#pull-request-guidelines)
*   [General Documentation And Coding Strategies](#general-documentation-and-coding-strategies)
*   [Tasks](#tasks)

## Just Starting Out

*   If you're unfamilar with git, check out this [brief introduction to Git](https://github.com/PCSX2/pcsx2/wiki/Git-survival-guide)
*   [How to build PCSX2 for Windows](https://github.com/PCSX2/pcsx2/wiki/Setting-up-the-PCSX2-repository-on-Windows-\(WIP---maybe-more-useful-as-a-manpage-instead%3F\))
*   [How to build PCSX2 for Linux](https://github.com/PCSX2/pcsx2/wiki/Installing-on-Linux)

## Issue Reportding

*   [How to write a useful issue](https://github.com/PCSX2/pcsx2/wiki/How-to-create-useful-and-valid-issues)

## Pull Request Guidelines

The following is a list of *general* style recommendations that will make reviewing and merging easier:

*   Commit Messages
    *   Please try to prefix your commit message, indicating what area of the project was modified.
        *   For example `gsdx: message...`.
        *   Looking at the project's commit history will help with keeping prefixes consistent overtime, *there is no strictly enforced list*.
    *   Try to keep messages brief and informative
    *   Remove unnecessary commits and squash commits together when appropriate.
        *   If you are not familiar with rebasing with git, check out the following resources:
            *   CLI - https://thoughtbot.com/blog/git-interactive-rebase-squash-amend-rewriting-history
            *   GUI (SourceTree) - https://www.atlassian.com/blog/sourcetree/interactive-rebase-sourcetree
*   Code Styling and Formatting
    *   [Consult the style guide](https://github.com/tadanokojin/pcsx2/blob/coding-guide/pcsx2/Docs/Coding_Guidelines.md)
    *   Run `clang-format` using the configuration file in the root of the repository
        *   Visual Studio Setup - https://devblogs.microsoft.com/cppblog/clangformat-support-in-visual-studio-2017-15-7-preview-1/
        *   IMPORTANT - if you are running `clang-format` on unrelated changes (ie. formatting an entire file), please do so in a separate commit.
            *   If you cannot scope your `clang-format` to just your changes and do not want to format unrelated code.  Try your best to stick with the existing formatting already established in the file in question.

## General Documentation And Coding Strategies

*   [Commenting Etiquette](https://github.com/PCSX2/pcsx2/wiki/Commenting-Etiquette)
*   [Coding style](https://github.com/PCSX2/pcsx2/wiki/Code-Formatting-Guidelines)
    *   [More comprehensive style-guide (Currently in Draft)](https://github.com/tadanokojin/pcsx2/blob/coding-guide/pcsx2/Docs/Coding_Guidelines.md)
*   [wxWidgets coding stategies](https://github.com/PCSX2/pcsx2/wiki/wxWidgets-Coding-Strategies)

## Tasks

*   [Todo List](https://github.com/PCSX2/pcsx2/wiki/Contributing-\(TODO-List\))
*   [Issues](https://github.com/PCSX2/pcsx2/issues)
*   [A collection of ideas to improve GSdx OGL](https://github.com/PCSX2/pcsx2/wiki/Todo-List)
