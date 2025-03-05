# Debug Library Documentation

## Changelog
- **twenisch 23Aug03**: Initial revision

## Goals
The goals of the Scaffold debug facility are:
- **Don't pay for what you don't use**: Debug statements should be free when they are shut off.
- **Provide the maximum information with the minimum code**.
- **Provide the best possible filtering capabilities** so you see only the output you want.

The Scaffold debugger filters debug output in three ways: **compile-time**, **entry-creation-time**, and **entry-processing-time**. When a debug statement is filtered at compile time, the text of the `DBG_` macro is removed by the preprocessor, incurring almost no compile time and no runtime overhead. Entry-creation-time filtering is accomplished by `if` statements around the entry-creation code. Entry-processing-time filtering is accomplished by comparing fields in the debug entry against predefined filters. The runtime cost of these filters depends on the number of fields (in particular, string fields) associated with the debug entry, and the number and complexity of debug targets and their filter expressions. Exclude filters short-circuit the filtering process, so you should try to write filter expressions with exclude filters first.

## "DBG_" Debug Entry Creation API
Debug entries are created with the `DBG_()` and `DBG_Assert()` family of macros. Each debug entry has an associated severity level, which controls compile-time filtering. The current `MinimumSeverity` setting determines whether `DBG_` macros expand to the code for creating an entry or compile away. `DBG_Assert()` macros are always enabled, regardless of the `MinimumSeverity` setting, unless specifically disabled via `DBG_SetAssertions false` (see below).

Here is the syntax for the `DBG_` family macro:


1. `DBG_( ,  )` ;
- **<severity>**: Debug severity level, documented below.
- **<operations>**: Any of the debug operations, documented under "DBG Operations" below.

**Example**:
`DBG_(Dev, (<< "Example debug entry at Development severity"));`

2. `DBG_Assert([, ]);`
Creates an assertion. The assertion is triggered if the condition is false. Except for compile-time filtering, this is equivalent to: `DBG_( Crit, Assert() Condition( !(  ) )  ) ;`

- **<condition>**: Condition controlling the assertion. Assertion is triggered if the condition is FALSE.
- **<operations>**: Any of the debug operations, documented under "DBG Operations" below.

**Example**:
`DBG_Assert((prt != 0), (<< "This assert fires when ptr == 0"));`

3. `DBG_AssertSev(, [, ]);`
Creates an assertion with a specified severity. Note that severity is generally ignored for the purpose of compile-time filtering of asserts, but can be used by the entry-processing-time filters to customize assert behavior. Except for compile-time filtering, this is equivalent to: `DBG_( , Assert() Condition( !(  ) )  ) ;`

- **<severity>**: Debug severity level, documented below.
- **<condition>**: Condition controlling the assertion. Assertion is triggered if the condition is FALSE.
- **<operations>**: Any of the debug operations, documented under "DBG Operations" below.

**Example**:
`DBG_AssertSev(Verb, (prt != 0), (<< "This assert has Verbose severity"));`


> Tom wrote more documentation about soon to be available features, which might never ever been implemented.

