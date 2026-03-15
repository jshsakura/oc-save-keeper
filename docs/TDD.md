# TDD Workflow

## Rule

Every behavior change starts with a failing unit test.
Run tests and fixes in a single serial flow. Do not work on multiple failing tests at once.

## Loop

1. Write or update a test in `tests/`.
2. Run `make test`.
3. Confirm the new test fails.
4. Fix only that one failing test.
5. Run `make test` again until it passes.
6. Refactor with tests still green.
7. Move to the next failing case.
8. Run the Switch build only after tests are green.

## Commands

```sh
make test
docker run --rm -v "$PWD:/work" -w /work devkitpro/devkita64 make -j2
```

## Scope

- Put pure logic and formatting rules behind testable helpers.
- Keep platform-specific glue thin.
- When a bug is found, add a regression test first.
