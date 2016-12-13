#ifndef SCAFFOLD_CORE_BOOST_EXTENSIONS_VA_INCLUDED
#define SCAFFOLD_CORE_BOOST_EXTENSIONS_VA_INCLUDED


#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/arithmetic/sub.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/control/expr_if.hpp>
#include <boost/preprocessor/control/while.hpp>
#include <boost/preprocessor/facilities/expand.hpp>
#include <boost/preprocessor/logical/bool.hpp>
#include <boost/preprocessor/logical/not.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>

#define BOOST_PP_VA_APPLY(f, ...) f(__VA_ARGS__)

#define BOOST_PP_VA_ARGS(...) __VA_ARGS__

#define BOOST_PP_VA_ARRAY(...) (BOOST_PP_VA_SIZE(__VA_ARGS__), (__VA_ARGS__))

#define BOOST_PP_VA_CAT(...) BOOST_PP_VA_FOLD_RIGHT(BOOST_PP_CAT, , __VA_ARGS__)

#define BOOST_PP_VA_CAT1(...) BOOST_PP_VA_CAT1_I(__VA_ARGS__)
#define BOOST_PP_VA_CAT1_I(x, ...) x ## __VA_ARGS__

#define BOOST_PP_VA_EAT(...)

#define BOOST_PP_VA_ELEM(i, ...) \
    BOOST_PP_VA_HEAD(BOOST_PP_VA_SKIP_N(i, __VA_ARGS__))

#define BOOST_PP_VA_FILTER(pred, ...) \
    BOOST_PP_VA_FOLD_LEFT_I(BOOST_PP_VA_FILTER_OP, (pred), , __VA_ARGS__)

#define BOOST_PP_VA_FILTER_OP(elem, pred, ...) \
    pred                                             \
    BOOST_PP_VA_LEFT_COMMA(__VA_ARGS__)              \
    BOOST_PP_VA_LEFT_COMMA(BOOST_PP_EXPR_IF(pred(elem), elem))

#define BOOST_PP_VA_FIRST_N(n, ...) \
    BOOST_PP_VA_WHILE_HEAD_REM                                               \
    (   BOOST_PP_VA_FIRST_N_PRED, BOOST_PP_VA_FIRST_N_OP, (), n, __VA_ARGS__ \
    )

#define BOOST_PP_VA_FIRST_N_PRED(result, n, ...) n

#define BOOST_PP_VA_FIRST_N_OP(result, n, elem, ...) \
    (BOOST_PP_VA_RIGHT_COMMA result elem), BOOST_PP_DEC(n), __VA_ARGS__

#define BOOST_PP_VA_FOLD_LEFT(op, state, ...) \
    BOOST_PP_VA_FOLD_LEFT_I(BOOST_PP_VA_FOLD_LEFT_OP, (op), state, __VA_ARGS__)

#define BOOST_PP_VA_FOLD_LEFT_OP(elem, op, ...) op, op(elem, __VA_ARGS__)

#define BOOST_PP_VA_FOLD_LEFT_I(op, state, first, ...) \
    BOOST_PP_VA_SKIP_N                                             \
    (   BOOST_PP_VA_SIZE state,                                    \
        BOOST_PP_VA_WHILE_HEAD_REM                                 \
        (   BOOST_PP_VA_FOLD_LEFT_I_PRED,                          \
            BOOST_PP_VA_FOLD_LEFT_I_OP,                            \
            (BOOST_PP_VA_RIGHT_COMMA state first), op, __VA_ARGS__ \
        )                                                          \
    )

#define BOOST_PP_VA_FOLD_LEFT_I_PRED(state, op, ...) \
    BOOST_PP_VA_IS_NOT_EMPTYM(__VA_ARGS__)

#define BOOST_PP_VA_FOLD_LEFT_I_OP(state, op, elem, ...) \
    (BOOST_PP_VA_APPLY(op, elem, BOOST_PP_VA_REM(state))), op, __VA_ARGS__

#define BOOST_PP_VA_FOLD_RIGHT(op, state, ...) \
    BOOST_PP_VA_FOLD_LEFT(op, state, BOOST_PP_VA_REVERSE(__VA_ARGS__))

#define BOOST_PP_VA_FOR(pred, op, macro, ...) \
    BOOST_PP_VA_WHILE_HEAD_REM             \
    (   BOOST_PP_VA_FOR_PRED,              \
        BOOST_PP_VA_FOR_OP,                \
        (), pred, op, macro, (__VA_ARGS__) \
    )                                      \

#define BOOST_PP_VA_FOR_PRED(result, pred, op, macro, state) pred state

#define BOOST_PP_VA_FOR_OP(result, pred, op, macro, state) \
    (BOOST_PP_VA_REM(result) macro state), pred, op, macro, (op state)

#define BOOST_PP_VA_FOR_EACH(macro, ...) \
    BOOST_PP_VA_FOLD_LEFT_I(BOOST_PP_VA_FOR_EACH_OP, (macro), , __VA_ARGS__)

#define BOOST_PP_VA_FOR_EACH_OP(elem, macro, ...) macro, __VA_ARGS__ macro(elem)

#define BOOST_PP_VA_FOR_EACH_I(macro, ...) \
    BOOST_PP_VA_FOLD_LEFT_I(BOOST_PP_VA_FOR_EACH_I_OP, (macro, 0),, __VA_ARGS__)

#define BOOST_PP_VA_FOR_EACH_I_OP(elem, macro, i, ...) \
    macro, BOOST_PP_INC(i), __VA_ARGS__ macro(i, elem)

#define BOOST_PP_VA_HEAD(...) BOOST_PP_VA_HEAD_I(__VA_ARGS__)
#define BOOST_PP_VA_HEAD_I(head, ...) head

#define BOOST_PP_VA_INSERT(i, elem, ...) \
    BOOST_PP_VA_FIRST_N(i, __VA_ARGS__), \
    elem                                 \
    BOOST_PP_VA_LEFT_COMMA(BOOST_PP_VA_SKIP_N(i, __VA_ARGS__))

#define BOOST_PP_VA_IS_EMPTY(...)                    \
    BOOST_PP_VA_HEAD                                 \
    (   BOOST_PP_VA_CAT1                             \
        (   BOOST_PP_VA_IS_EMPTY_,                   \
            BOOST_PP_VA_CAT1                         \
            (   BOOST_PP_VA_IS_EMPTY_,               \
                BOOST_PP_VA_IS_EMPTY_EAT __VA_ARGS__ \
            ) ()                                     \
        )                                            \
   )

#define BOOST_PP_VA_IS_EMPTY_EAT(...) BOOST_PP_VA_IS_EMPTY_EATEN
#define BOOST_PP_VA_IS_EMPTY_BOOST_PP_VA_IS_EMPTY_EAT() \
    BOOST_PP_VA_IS_EMPTY_EMPTY
#define BOOST_PP_VA_IS_EMPTY_BOOST_PP_VA_IS_EMPTY_EATEN \
    BOOST_PP_VA_IS_EMPTY_NOT_EMPTY
#define BOOST_PP_VA_IS_EMPTY_BOOST_PP_VA_IS_EMPTY_EMPTY                    1,
#define BOOST_PP_VA_IS_EMPTY_BOOST_PP_VA_IS_EMPTY_NOT_EMPTY                0,
#define BOOST_PP_VA_IS_EMPTY_BOOST_PP_VA_IS_EMPTY_BOOST_PP_VA_IS_EMPTY_EAT 0,

#define BOOST_PP_VA_IS_EMPTYM(...) \
    BOOST_PP_VA_HEAD                                                \
    (   BOOST_PP_VA_IS_EMPTYM_I(BOOST_PP_VA_IS_EMPTYM_ __VA_ARGS__) \
    )

#define BOOST_PP_VA_IS_EMPTYM_I(...) \
    BOOST_PP_VA_IS_EMPTYM_ ## __VA_ARGS__ ## BOOST_PP_VA_IS_EMPTYM_

#define BOOST_PP_VA_IS_EMPTYM_BOOST_PP_VA_IS_EMPTYM_                       0,
#define BOOST_PP_VA_IS_EMPTYM_BOOST_PP_VA_IS_EMPTYM_BOOST_PP_VA_IS_EMPTYM_ 1,

#define BOOST_PP_VA_IS_NOT_EMPTYM(...) \
    BOOST_PP_NOT(BOOST_PP_VA_IS_EMPTYM(__VA_ARGS__))

#define BOOST_PP_VA_IS_TUPLE(...) \
    BOOST_PP_VA_HEAD                                                  \
    (   BOOST_PP_VA_IS_TUPLE_I(BOOST_PP_VA_IS_TUPLE_TEST __VA_ARGS__) \
    )

#define BOOST_PP_VA_IS_TUPLE_I(...) \
    BOOST_PP_VA_CAT1(BOOST_PP_VA_IS_TUPLE_, __VA_ARGS__)

#define BOOST_PP_VA_IS_TUPLE_TEST(...) BOOST_PP_VA_IS_TUPLE_YES

#define BOOST_PP_VA_IS_TUPLE_BOOST_PP_VA_IS_TUPLE_YES  1,
#define BOOST_PP_VA_IS_TUPLE_BOOST_PP_VA_IS_TUPLE_TEST 0,

#define BOOST_PP_VA_LAST_N(n, ...) \
    BOOST_PP_VA_SKIP_N                                              \
    (   BOOST_PP_SUB(BOOST_PP_VA_SIZE(__VA_ARGS__), n), __VA_ARGS__ \
    )

#define BOOST_PP_VA_LEFT_COMMA(...) \
    BOOST_PP_COMMA_IF(BOOST_PP_VA_IS_NOT_EMPTYM(__VA_ARGS__)) __VA_ARGS__

#define BOOST_PP_VA_LIST(...) \
    BOOST_PP_VA_FOLD_RIGHT(BOOST_PP_VA_TUPLE, BOOST_PP_NIL, __VA_ARGS__)

#define BOOST_PP_VA_POP_BACK(...) \
    BOOST_PP_VA_FIRST_N                                          \
    (   BOOST_PP_DEC(BOOST_PP_VA_SIZE(__VA_ARGS__)), __VA_ARGS__ \
    )

#define BOOST_PP_VA_PUSH(elem, ...) elem BOOST_PP_VA_LEFT_COMMA(__VA_ARGS__)

#define BOOST_PP_VA_PUSH_BACK(elem, ...) \
    BOOST_PP_VA_RIGHT_COMMA(__VA_ARGS__) elem

#define BOOST_PP_VA_REM(tuple) BOOST_PP_VA_REM_I tuple
#define BOOST_PP_VA_REM_I(...) __VA_ARGS__

#define BOOST_PP_VA_REMOVE(i, ...) \
    BOOST_PP_VA_FIRST_N(i, __VA_ARGS__) \
    BOOST_PP_VA_LEFT_COMMA(BOOST_PP_VA_SKIP_N(BOOST_PP_INC(i), __VA_ARGS__))

#define BOOST_PP_VA_REPEAT(count, macro, ...) \
    BOOST_PP_REPEAT(count, BOOST_PP_VA_REPEAT_OP, (macro, __VA_ARGS__))

#define BOOST_PP_VA_REPEAT_OP(z, n, data) \
    BOOST_PP_VA_APPLY(BOOST_PP_VA_INSERT(1, n, BOOST_PP_VA_REM(data)))

#define BOOST_PP_VA_REPLACE(i, elem, ...) \
    BOOST_PP_VA_INSERT(i, elem, BOOST_PP_VA_REMOVE(i, __VA_ARGS__))

#define BOOST_PP_VA_REVERSE(...)  \
    BOOST_PP_VA_FOLD_LEFT_I(BOOST_PP_VA_ARGS, (), __VA_ARGS__)

#define BOOST_PP_VA_RIGHT_COMMA(...) \
    __VA_ARGS__ BOOST_PP_COMMA_IF(BOOST_PP_VA_IS_NOT_EMPTYM(__VA_ARGS__))

#define BOOST_PP_VA_SEQ(...) \
    BOOST_PP_VA_FOR_EACH(BOOST_PP_VA_TUPLE, __VA_ARGS__)

#define BOOST_PP_VA_SIZE(...) \
    BOOST_PP_VA_WHILE                         \
    (   BOOST_PP_VA_SIZE_PRED,                \
        BOOST_PP_VA_SIZE_OP,                  \
        0 BOOST_PP_VA_LEFT_COMMA(__VA_ARGS__) \
    )

#define BOOST_PP_VA_SIZE_PRED(n, ...) BOOST_PP_VA_IS_NOT_EMPTYM(__VA_ARGS__)

#define BOOST_PP_VA_SIZE_OP(n, elem, ...) \
    BOOST_PP_INC(n) BOOST_PP_VA_LEFT_COMMA(__VA_ARGS__)

#define BOOST_PP_VA_SLICE(start, len, ...) \
    BOOST_PP_VA_FIRST_N(len, BOOST_PP_VA_SKIP_N(start, __VA_ARGS__))

#define BOOST_PP_VA_SKIP_N(n, ...) \
    BOOST_PP_VA_TAIL               \
    (   BOOST_PP_VA_WHILE          \
        (   BOOST_PP_VA_HEAD,      \
            BOOST_PP_VA_SKIP_N_OP, \
            n, __VA_ARGS__         \
        )                          \
    )

#define BOOST_PP_VA_SKIP_N_OP(n, ...) \
    BOOST_PP_DEC(n), BOOST_PP_VA_TAIL(__VA_ARGS__)

#define BOOST_PP_VA_TAIL(...) BOOST_PP_VA_TAIL_I(__VA_ARGS__)
#define BOOST_PP_VA_TAIL_I(head, ...) __VA_ARGS__

#define BOOST_PP_VA_TRANSFORM(op, ...) \
    BOOST_PP_VA_FOLD_LEFT_I(BOOST_PP_VA_TRANSFORM_OP, (op), , __VA_ARGS__)

#define BOOST_PP_VA_TRANSFORM_OP(elem, macro, ...) \
    macro, BOOST_PP_VA_RIGHT_COMMA(__VA_ARGS__) macro(elem)

#define BOOST_PP_VA_TUPLE(...) (__VA_ARGS__)

#define BOOST_PP_VA_WHILE(pred, op, ...)  \
    BOOST_PP_VA_REM                       \
    (   BOOST_PP_EXPAND                   \
        (   BOOST_PP_VA_HEAD              \
            BOOST_PP_WHILE                \
            (   BOOST_PP_VA_WHILE_PRED,   \
                BOOST_PP_VA_WHILE_OP,     \
                ((__VA_ARGS__), pred, op) \
            )                             \
        )                                 \
    )

#define BOOST_PP_VA_WHILE_OP(d, state) BOOST_PP_VA_WHILE_OP_I state
#define BOOST_PP_VA_WHILE_OP_I(state, pred, op) ((op state), pred, op)

#define BOOST_PP_VA_WHILE_PRED(d, state) BOOST_PP_VA_WHILE_PRED_I state
#define BOOST_PP_VA_WHILE_PRED_I(state, pred, op) pred state

#define BOOST_PP_VA_WHILE_HEAD_REM(...) \
    BOOST_PP_VA_REM(BOOST_PP_VA_HEAD(BOOST_PP_VA_WHILE(__VA_ARGS__)))


#define TEST1_OP(x, y) BOOST_PP_DEC(x), y
#define TEST2_OP(x, ...) BOOST_PP_DEC(x), __VA_ARGS__,__VA_ARGS__
#define TEST3_PRED(...) 0
#define TEST4_OP(x) a ## x

/*
    BOOST_PP_VA_IS_TUPLE(()) === 1
    BOOST_PP_VA_IS_TUPLE(x) === 0
    BOOST_PP_VA_IS_EMPTY(a) === 0
    BOOST_PP_VA_IS_EMPTY() === 1
    BOOST_PP_VA_IS_EMPTY(,) === 0
    BOOST_PP_VA_IS_EMPTY(()) === 0
    BOOST_PP_VA_IS_EMPTY(()()) === 0
    BOOST_PP_VA_IS_EMPTY(a) === 0
    BOOST_PP_VA_IS_EMPTY(a, b) === 0
    BOOST_PP_VA_IS_EMPTYM() === 1
    BOOST_PP_VA_IS_EMPTYM(BOOST_PP_CAT) === 0
    BOOST_PP_VA_IS_EMPTYM(a) === 0
    BOOST_PP_VA_IS_EMPTYM(a, b) === 0
    BOOST_PP_VA_WHILE(BOOST_PP_VA_HEAD, TEST1_OP, 1, 4) === 0, 4
    BOOST_PP_VA_WHILE(TEST3_PRED, BOOST_PP_VA_NIL, a, b) === a, b
    BOOST_PP_VA_FOLD_LEFT_I(BOOST_PP_CAT, (), a, b, c) === cba
    BOOST_PP_VA_TRANSFORM(TEST4_OP, 1, 2, 3) === a1, a2, a3
    BOOST_PP_VA_SEQ(a, b, c) === (a)(b)(c)
    BOOST_PP_VA_FILTER(BOOST_PP_VA_ARGS, 0, 1, 2, 0, 3, 0) === 1, 2, 3
    BOOST_PP_VA_FILTER(BOOST_PP_VA_ARGS, 1, 2, 0, 3) === 1, 2, 3
    BOOST_PP_VA_REPLACE(1, x, a, b, c) === a, x, c
    BOOST_PP_VA_INSERT(1, x, a, b, c) === a, x, b, c
    BOOST_PP_VA_REMOVE(1, a, b, c) === a, c
    BOOST_PP_VA_POP_BACK(a, b, c, d) === a, b, c
    BOOST_PP_VA_LAST_N(3, a, b, c, d, e) === c, d, e
    BOOST_PP_VA_SLICE(1, 2, a, b, c, d, e) === b, c
    BOOST_PP_VA_LIST(a, b, c) === (a, (b, (c, BOOST_PP_NIL)))
    BOOST_PP_VA_REVERSE(a, b, c) === c, b, a
    BOOST_PP_VA_FOR(BOOST_PP_VA_HEAD, TEST2_OP, BOOST_PP_VA_TAIL, 3, a) === \
    a a,a a,a,a,a
    BOOST_PP_VA_CAT(a, b, c) === abc
    BOOST_PP_VA_FOLD_LEFT(BOOST_PP_NIL, 0) === 0
    BOOST_PP_VA_ARRAY(a, b, c) === (3, (a, b, c))
    BOOST_PP_VA_ARRAY() === (0, ())
*/

#endif //SCAFFOLD_CORE_BOOST_EXTENSIONS_VA_INCLUDED
