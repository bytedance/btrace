# Contributing to btrace

Welcome to the btrace project. Read on to learn more about our development process and how to propose bug fixes and improvements.

## Issues

We use GitHub issues to track public bugs and feature requests. Before creating an issue, please note the following:

1. Please search existing issues before creating a new one.
2. Please ensure your description is clear and has sufficient instructions to be able to reproduce the issue. The more information the better.


## Branch Management

There are 2 main branches:

1. `master` branch

    * It's the latest (pre-)release branch. We use `master` for tags.
    * **Please do NOT submit any PR on `master` branch.**

2. `dev` branch

    * It's our stable developing branch.
    * Once `dev` has passed our internal tests, it will be merged to `master` branch for the next release.
    * **Please always submit PR on `dev` branch for latest bug fixes and improvements.**

3. `release/2.0` branch

    * It's the legacy branch for the 2.0 release.
    * If you have any bug fixes or improvements for the 2.0 release, please submit PR on this branch.


## Pull Requests

Please make sure the following is done when submitting a pull request:

1. Fork the repo and create your branch from `master`.
2. Add the copyright notice to the top of any new files you've added.
3. Try your best to test your code.
4. Squash all of your commits into one meaningful commit.

## Code Style Guide

1. 4 spaces for indentation rather than tabs.

## License

By contributing to btrace, you agree that your contributions will be licensed under its [Apache LICENSE](LICENSE).