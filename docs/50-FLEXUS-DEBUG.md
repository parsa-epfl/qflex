Dbg library documentation
====

Changelog
  - twenisch 23Aug03 Initial revision


Goals:
===
  The goals of the Scaffold debug facility are:
    - don't pay for what you don't use.  Debug statements should
      be free when they are shut off.
    - provide the maximum information with the minimum code
    - provide the best possible filtering capabilities so you see
      only the output you want

The Scaffold debugger filters debug output in 3 ways, compile-time,
entry-creation-time, and entry-processing-time. When a debug statement
is filtered at compile time, the text of the DBG_ macro is removed by
the preprocessor, and incurs almost no compile time and no runtime
overhead.  Entry-creation-time filtering is accomplished by if statements
around the entry-creation code.  entry-processing-time filtering is
accomplished by comparing fields in the debug entry against predefined
filters.  The runtime cost of these filters depends on the number of fields
(in particular, string fields) associated with the debug entry, and the
number and complexity of debug targets and their filter expressions.
Exclude filters short-circuit the filtering process, so you should try to
write filter expressions with exclude filters first.


"DBG_" Debug Entry Creation API
=======

Debug entries are created with the DBG_() and DBG_Assert() family of macros.
Each debug entry has an associated severity level, which controls compile-time
filtering.  The current MinimumSeverity setting determines whether DBG_ macros
expand to the code for creating an entry, or compile away.  DBG_Assert() macros
are always enabled, regardless of MinimumSeverity setting, unless specifically
disabled via "DBG_SetAssertions false" (see below).

Here is the syntax for the DBG_ family macro:

  DBG_( <severity>, <operations> ) ;
Creates a debug entry<severity> - Debug severity level, documented below.<operations> - Any of the debug operations,
  documented under "DBG Operations" below Ex : DBG_(Dev, (<< "Example debug entry at Development severity"));

DBG_Assert(<condition>[, <operations>]);
    Creates an assertion.  The assertion is triggered if condition is false.
    Except for compile time filtering, this is equivalent to:
    DBG_( Crit, Assert() Condition( !( <condition> ) ) <operations> ) ;
    <condition> - Condition controlling the assertion.Assertion is triggerred if the condition is FALSE.<operations> -
      Any of the debug operations,
      documented under "DBG Operations" below Ex : DBG_Assert((prt != 0), (<< "This assert fires when ptr == 0"));

    DBG_AssertSev(<severity>, <condition>[, <operations>]);
    Creates an assertion with a specified severity.  Note that severity is
    generally ignored for the purpose of compile time filtering of asserts, but
    can be used by the entry-processing-time filters to customize assert
    behavior.  Except for compile time filtering, this is equivalent to:
    DBG_( <severity>, Assert() Condition( !( <condition> ) ) <operations> ) ;
    <severity> - Debug severity level,
      documented below.<condition> -
        Condition controlling the assertion.Assertion is triggerred if the condition is FALSE.<operations> -
        Any of the debug operations,
      documented under "DBG Operations" below Ex
      : DBG_AssertSev(Verb, (prt != 0), (<< "This assert has Verbose severity"));

    PLANNED
    FEATURES(not yet available)
      : DBG_Invocation();
    Track the invocation of the current function, for creating stack backtraces
    for debug entries.
    Ex:
      DBG_Invocation();

DBG Operations
=======

  ( << <arguments> )
    Associates a message with a debug entry.  This text is available as the
    {Message} field of the entry.  Anything that can be written to an ostream
    can appear in the message.
      <arguments> - any expression that is valid for an ostream like std::cout
    Aliases:
      Message( << message )
      Msg( << message )
    Ex:
      DBG_( Crit, ( << " a complicated " << &std::hex << myVariable << " expression.") );

AddCategory(<cat - name> | <cat - name> | <cat - name>...) Associates the specified list of
  categories with the debug entry.This list ADDS TO the category list of the debug statement,
  so it appends onto any previous
      Category() operations.Categories must have been previously created using DBG_NewCategory(see below)
        .<cat - name> -
      the name of the category.A list of categories can be separated by
    | Aliases : AddCat(<cat - name> | ...) Ex : DBG_(Crit, Cat(MyCat1) AddCat(MyCat2 | MyCat3)(<< " some entry."));

  Address( <numeric-expr> )
    Sets the {Address} numeric field of the debug entry to the specified value.
    This is shorthand for SetNumeric( (Address) <numeric-expr> ).
      <numeric-expr> - any expression convertible to long long
    Aliases:
      Addr( <numeric-expr> )
    Ex:
      DBG_( Crit, Addr( address ) ( << " memory reference to specified addr.") );

  Assert() Marks a debug statement as an assertion.If you use a DBG_(Sev, Assert()) macro rather than DBG_Assert(),
    the assertion will be filtered at compile time like a normal debug statement
      .Assetions also have the Assert category added.Ex : DBG_(Crit, Assert()(<< "Almost the same as DBG_Assert()"));

  Category(<cat - name> | <cat - name> | <cat - name>...) Associates the specified list of
    categories with the debug entry.This list REPLACES the category list of the debug statement,
    so it overwrites any previous
        Category() operations.Categories must have been previously created using DBG_NewCategory(see below)
          .<cat - name> -
        the name of the category.A list of categories can be separated by
      | Aliases : Cat(<cat - name> | ...) Ex : DBG_(Crit, Cat(MyCat1 | MyCat2)(<< " some entry."));

  Component( <component-ref> )
    Attaches the specified scaffold component to this debug entry.  This makes
    the {ComponentName} {ComponentType} and {ScaffoldIdx} debug fields
    available.  This also adds a condition to the debug expression which calls
    <component-ref>.debugEnabled( <severity> ).  The default implementation
    of debugEnabled compares the debug statements severity against the severity
    specified for the component in the wiring file.
      <component-ref> - a reference to the component object
    Aliases:
      Comp( <component-ref> )
    Ex:
      DBG_( Crit, Comp( *this ) ( << " *this is a scaffold component.") );

  Condition( <boolean-expr> )
    Associates a condition with the DBG statement.  This condition filters
    the debug statement at entry-creation-time.  The entry is only created
    if the condition evaluates to TRUE.  The condition is available in the
    debug entry as the {Condition} field. Note that conditions are inverted for
    DBG_Assert( <boolean-expr) ...) statements, as assertions fire when the
    condition is FALSE.  If a debug statement has multiple conditions, they
    behave as if they were connected by && .
      <boolean-expr> - any boolean expression.
    Aliases:
      Cond( <boolean-expr>)
    Ex:
      DBG_( Crit, Cond( my_variable == 1 ) ( << " my_variable must equal 1.") );

  Core() Marks a debug statement as belonging to the Scaffold core
    .Core debug statements can be filtered out at compile
    time(core assertions are not filtered out unless all assertions are disabled)
    .This also gives the debug statement the Core category.Ex : DBG_(Crit, Core()(<< "A core debug statement"));

  NoDefaultOps( )
    Suppresses the addition of "default operations" to a particular debug
    statement.  This is useful , e.g, if the default operations would cause a
    compile error because they refer to a variable name that isn't in scope.
    See "Compile Time Control of the DBG_ system" below for more info.
    Ex:
      DBG_( Crit, NoDefaultOps() ( << "default operations suppressed" ) );

  Set( ( <field-name> ) << <arguments> )
    Creates a new string field in the debug entry, called {<field-name>}.
    Anything that can be written to an ostream can be written to the field.
      <field-name> - a name for the field
      <arguments> - any expression that is valid for an ostream like std::cout
    Ex:
      DBG_( Crit, ( (MyField) << " a value for {MyField}") );

  SetNumeric( ( <field-name> ) <value> )
    Creates a new numeric field in the debug entry, called {<field-name>}.
      <field-name> - a name for the field
      <value> - any expression that can be converted to long long
    Ex:
      DBG_( Crit, ( (MyNumericField) my_integer_variable );


Compile Time Control of the DBG_ system
====

Much of the behavior of debug statements (for example, if they should compile
away) can be controlled at compile time.  These settings can be changed at
any point in the code by setting some directives with #define, and then
#include DBG_Control().Note tht there are NO SEMICOLONS at the end of the
#define or #include directives.

#define DBG_NewCategories category, category, ...
#include DBG_Control()
    Creates new debug categories.  Categories should be separated by commas.
    This should only be done at global scope (outside of any namespace), or
    the categories will only be visible within that namespace.
    Ex:
#define DBG_NewCategories MyCat, MyCat2
#include DBG_Control()

#define DBG_Reset
#include DBG_Control()
    Clears the default operations set with SetDefaultOps. See DBG_SetDefaultOps
    for more info.

#define DBG_SetAssertions true or false
#include DBG_Control()
    Enable or disable assertions. This is true by default.
    Ex:
#define DBG_Assertions true
#include DBG_Control()

#define DBG_SetCoreDebugging true or false
#include DBG_Control()
    Enable or disable core debugging.  This needs to be set before the
    Scaffold core is included (ie at the top of wiring). This is false
    by default.
    Ex:
#define DBG_SetCoreDebugging true
#include DBG_Control()

#define DBG_SetDefaultOps <operations>
#include DBG_Control()
    Any debug operations after DBG_SetDefaultOps are automatically appended
    onto the end of every DBG_ and DBG_Assert statement, unless inhibited
    by NoDefaultOps().  These default operations are cleared by DBG_Reset.
    You should put a DBG_Reset at the bottom of any file that uses
    DBG_SetDefaultOps.
    Ex:
#define DBG_SetDefaultOps AddCategory(ProtocolEngine)
#include DBG_Control()
        ...
#define DBG_Reset
#include DBG_Control()

#define DBG_SetMinimumSeverity <severity>
#include DBG_Control()
    Set the severity level used for compile-time filtering of debug statements.
    Ex:
#define DBG_SetMinimumSeverity Dev
#include DBG_Control()


Run Time Configuration
====

  Currently, the debug system reads debug.cfg in the current directory on
startup to configure itself.  At some point, you will be able to load and
modify configurations from within Simics.

Here is the configuration file format:

target "target-name" {
    filter{<list of filters> } action
    {
<list of actions>
    }
}

Each filter expression and action statement ends in a semicolon.

Filters are either include filters or exclude filters.  A debug entry matches
a target if it matches at least one include filter, and no exclude filters.

  + <filter> & <filter> ... ;
    Include filter.  Matches if the debug entry matches each of the &-separated
    filters.

  - <filter> & <filter> ... ;
    Exculde filter. Matches if the debug entry matches each of the &-separated
    filters.

Here are filter supported by the parser:
  @ <numeric-value>
  @ 0x<hex value>
      Address Filter.  Filters by comaring for equality to the {Address} field.

  [ <numeric-value> ]
      ScaffoldIdx Filter.  Filters by comaring for equality to the {ScaffoldIdx}
      field.

  severity
  severity+
  severity-
      Severity filter.  Matches entries that are equal to, higher, or lower
      than the specified severity.  See the Severity section for the list of
      severities

  {FieldName} == value
  {FieldName} != value
  {FieldName} >= value
  {FieldName} <= value
  {FieldName} > value
  {FieldName} < value
  {FieldName} exists
      General filter on a field.  If value is in quotes, then string comparison
      is performed.  If value is a number, numeric comparison is performed.

  category
      Anything that can't be parsed as another filter is assumed to be a
      category filter.  Matches if the component has the specified category.

Here are the supported actions:

  log console "strings" {FieldName} ...
  log (filename) "strings" {FieldName} ...
      Write debug output using the specified format to the console or to a file.
  abort
      Call std::abort() to halt execution


Fields available in debug entries
===
{FilePath}        - complete path from __FILE__
{FileName}        - portion of __FILE__ after the last /
{Line}            - __LINE__
{Function}        - __FUNCTION__
{Categories}      - | separated list of categories
{Severity}        - Symbolic name of severity
{SeverityNumeric} - Numeric severity level
{GlobalCount}     - Debug entry number (these increment globally for all debug
                    entries)
{LocalCount}      - Number of times this particular DBG_ statement has been
                    logged
{Message}         - The message associated with the debug entry.
{Condition}       - The boolean expression associated with this debug entry
{ComponentName}   - The configuration name of the Scaffold component associated
                    with the debug statement
{ComponentType}   - The class name of the scaffold component.
{ScaffoldIdx}     - The index of the scaffold component.
{Address}         - The memory address from the Addr() operation.
<user defined>    - user defined entry created with SetStr or SetNum.  Expands
                    to <undefined> if no entry with the name exists

Not currently functional:
{Cycle}           - Current scaffold cycle
{Drive}           - The Scaffold drive interface that was last invoked, if any
{BackTrace}       - List of active functions that are tracked by DBG_Invocation()
