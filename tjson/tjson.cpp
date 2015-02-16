/*
	Copyright (c) 2014 Zhang li

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/*
	Author zhang li
	Email zlvbvbzl@gmail.com
*/

#include "tjson.h"
#include <stdlib.h>
#include <vector>
#include <stdint.h>

using namespace tjson;
using namespace tjson::internal;

#define ARRAY_INIT_SIZE 64
#define MAP_INIT_SIZE 32
#define STRING_INIT_SIZE 128
#define STACK_MAX_SIZE 500
#define DEBUG_LEX 0
#define DEBUG_MEM 0
#define PREALLOC 1

tjson::Value tjson::Value::Null;

struct tjException
{
    tjException(size_t _pos):pos(_pos){}
    size_t pos;
};

enum syntex_type
{
    S_START,
    S_WORD,
    S_NUMBER,
    S_FLOAT,
    S_NUMBER_E,
    S_FLOAT_E,
    S_STRING,
    S_SPACE_END,
    S_SYMBOL,
    S_END,
};

enum ga_type
{
    G_START,
    G_ARRAY,
    G_ARRAYSEP,
    G_DICT,
    G_DICTSEP,
    G_KEY,
    G_KEYSEP,
    G_ELEMENT,            
    G_VALUE,    
};
struct parse_state;

void throw_error(parse_state *state);
template <int SIZE>
struct parse_stack
{
    parse_stack():size(0){}
    enum {max_size = SIZE};
    ga_type data[SIZE];
    size_t size;
    parse_state *state;
    ga_type &top()
    {
        return data[size-1];
    }

    void push(ga_type g)
    {
        if (size >= STACK_MAX_SIZE)
        {
            throw_error(state);
        }
        data[size++] = g;
    }

    bool empty() const
    {
        return size == 0;
    }

    void pop()
    {
        size--;
    }
};

struct parse_score
{
    size_t size;
    const char *buff;

    char operator[](size_t i) const
    {
        if (i < size)
        {
            return buff[i];            
        }
        return EOF;
    }
};

struct parse_state
{    
    parse_score score;
    size_t score_pos;
    parse_stack<STACK_MAX_SIZE> G;
    char current_syntex[65536];
    size_t syntex_len;
    Value *curval;
    syntex_type S;            
    char string_begin;                
};

void throw_error(parse_state *state)
{
    throw tjException(state->score_pos);
}

#if DEBUG_LEX
void debug_print(parse_state *s, const char *type)
{
    s->current_syntex[s->syntex_len] = 0;
    printf("[%s]\t%s\n", type, s->current_syntex);
}

void debug_print(char c)
{
    printf("[symbol]\t%c\n", c);
}
#else
#define debug_print(...)
#endif

static inline void clear_syntex(parse_state *state)
{
    state->syntex_len = 0;
}

static inline void change_syntex(parse_state *state, syntex_type t)
{
    clear_syntex(state);
    state->S = t;
}

static inline bool is_EOF(char c)
{
    return c == EOF;
}

static inline bool is_other(char c)
{
    return true;
}

static inline bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

static inline bool is_symbol(char c)
{
    return c == ',' || c == ':' || c == '{' || c == '[' || c == ']' || c == '}';
}

static inline void append_syntex(char c, parse_state *state)
{
    if (state->syntex_len < sizeof(state->current_syntex) - 1)
    {
        state->current_syntex[state->syntex_len] = c;
        state->syntex_len++;
        return;
    }

    throw_error(state);
}

static inline bool match_symbol(char c, char expect)
{
    return c == expect;
}

static inline void match_string(parse_state *state)
{
    state->G.pop();
    state->curval->internal_build_string(state->current_syntex, state->syntex_len);
}

static inline void match_number(parse_state *state)
{
    state->G.pop();
    state->current_syntex[state->syntex_len]= 0;
    state->curval->internal_build_integer(state->current_syntex);
}

static inline void match_float(parse_state *state)
{
    state->G.pop();
    state->current_syntex[state->syntex_len]= 0;
    state->curval->internal_build_float(state->current_syntex);
}

static inline void match_dict(parse_state *state)
{
    state->G.top() = G_KEY;
    state->curval->internal_build_object();
}

static inline void match_array(parse_state *state)
{
    state->G.top() = G_ELEMENT;
    state->curval->internal_build_array();
}

static inline void match_element_number(parse_state *state)
{
    state->G.top() = G_ARRAYSEP;    
    Value *element = state->curval->internal_add();
    assert(element);
    state->current_syntex[state->syntex_len]= 0;
    element->internal_build_integer(state->current_syntex);
}

static inline void match_element_float(parse_state *state)
{
    state->G.top() = G_ARRAYSEP;
    Value *element = state->curval->internal_add();
    assert(element);
    state->current_syntex[state->syntex_len]= 0;
    element->internal_build_float(state->current_syntex);
}

static inline void match_element_string(parse_state *state)
{
    state->G.top() = G_ARRAYSEP;
    Value *element = state->curval->internal_add();
    assert(element);
    element->internal_build_string(state->current_syntex, state->syntex_len);
}

static inline void match_element_dict(parse_state *state)
{
    state->G.top() = G_ARRAYSEP;
    state->G.push(G_KEY);
    Value *element = state->curval->internal_add();
    assert(element);
    element->internal_build_object();
    element->internal_parent = state->curval;
    state->curval = element;
    assert(state->curval);
}

static inline void match_element_array(parse_state *state)
{
    state->G.top() = G_ARRAYSEP;
    state->G.push(G_ELEMENT);
    Value *element = state->curval->internal_add();
    assert(element);
    element->internal_build_array();
    element->internal_parent = state->curval;
    state->curval = element;
    assert(state->curval);
}

static inline void match_element_null(parse_state *state)
{
    state->G.top() = G_ARRAYSEP;
    Value *element = state->curval->internal_add();
    assert(element);
    (void)element;
}

static inline void match_element_bool(parse_state *state, bool v)
{
    state->G.top() = G_ARRAYSEP;
    Value *element = state->curval->internal_add();
    assert(element);
    element->internal_build_bool(v);
    (void)element;
}

static inline void match_value_number(parse_state *state)
{
    state->G.top() = G_DICTSEP;
    state->current_syntex[state->syntex_len]= 0;
    state->curval->internal_build_integer(state->current_syntex);
    state->curval = state->curval->internal_parent;
    assert(state->curval);
}

static inline void match_value_float(parse_state *state)
{
    state->G.top() = G_DICTSEP;
    state->current_syntex[state->syntex_len]= 0;
    state->curval->internal_build_float(state->current_syntex);
    state->curval = state->curval->internal_parent;
    assert(state->curval);
}

static inline void match_value_null(parse_state *state)
{
    state->G.top() = G_DICTSEP;
    state->current_syntex[state->syntex_len]= 0;
    state->curval = state->curval->internal_parent;
    assert(state->curval);
}

static inline void match_value_bool(parse_state *state, bool v)
{
    state->G.top() = G_DICTSEP;
    state->current_syntex[state->syntex_len]= 0;
    state->curval->internal_build_bool(v);
    state->curval = state->curval->internal_parent;
    assert(state->curval);
}

static inline void match_value_string(parse_state *state)
{
    state->G.top() = G_DICTSEP;
    state->curval->internal_build_string(state->current_syntex, state->syntex_len);
    state->curval = state->curval->internal_parent;
    assert(state->curval);
}

static inline void match_value_dict(parse_state *state)
{
    state->G.top() = G_DICTSEP;
    state->G.push(G_KEY);
    state->curval->internal_build_object();
}

static inline void match_value_array(parse_state *state)
{
    state->G.top() = G_DICTSEP;
    state->G.push(G_ELEMENT);
    state->curval->internal_build_array();
}

static inline void match_array_end(parse_state *state)
{
    state->G.pop();
    state->curval = state->curval->internal_parent; 
}

static inline void match_dict_end(parse_state *state)
{
    state->G.pop();
    state->curval = state->curval->internal_parent;
}


static inline void match_key(parse_state *state)
{
    state->G.top() = G_KEYSEP;
    Value *new_value = state->curval->internal_add_key(state->current_syntex, state->syntex_len);
    assert(new_value);
    new_value->internal_parent = state->curval;
    state->curval = new_value;
}

static void get_word(parse_state *state)
{
    debug_print(state, "word");
    switch (state->G.top())
    {
    case G_KEY:
        match_key(state);
        break;
    case G_ELEMENT:
        if (state->syntex_len == 4)
        {
            if (memcmp(state->current_syntex, "null", 4) == 0)
            {
                match_element_null(state);
                break;
            }
            else if (memcmp(state->current_syntex, "true", 4) == 0)
            {
                match_element_bool(state, true);
                break;
            }
        }
        else if (state->syntex_len == 5)
        {
            if (memcmp(state->current_syntex, "false", 5) == 0)
            {
                match_element_bool(state, false);
                break;
            }
        }

        match_element_string(state);
        break;
    case G_VALUE:
        if (state->syntex_len == 4)
        {
            if (memcmp(state->current_syntex, "null", 4) == 0)
            {
                match_value_null(state);
                break;
            }
            else if (memcmp(state->current_syntex, "true", 4) == 0)
            {
                match_value_bool(state, true);
                break;
            }
        }
        else if (state->syntex_len == 5)
        {
            if (memcmp(state->current_syntex, "false", 5) == 0)
            {
                match_value_bool(state, false);
                break;
            }
        }
        match_value_string(state);
        break;
    default:
        throw_error(state);
    }
}

void get_number(parse_state *state)
{
    debug_print(state, "integer");
    switch (state->G.top())
    {
    case G_ELEMENT:
        match_element_number(state);
        break;
    case G_KEY:
        match_key(state);
        break;
    case G_VALUE:
        match_value_number(state);
        break;
    case G_START:
        match_number(state);
        break;
    default:
        throw_error(state);
    }
}

void get_float(parse_state *state)
{
    debug_print(state, "float");
    switch (state->G.top())
    {
    case G_ELEMENT:
        match_element_float(state);
        break;
    case G_KEY:
        match_key(state);
        break;
    case G_VALUE:
        match_value_float(state);
        break;
    case G_START:
        match_float(state);
        break;
    default:
        throw_error(state);
    }
}

void get_string(parse_state *state)
{
    debug_print(state, "string");
    switch (state->G.top())
    {
    case G_ELEMENT:
        match_element_string(state);
        break;
    case G_KEY:
        match_key(state);
        break;
    case G_VALUE:
        match_value_string(state);
        break;
    case G_START:
        match_string(state);
        break;
    default:
        throw_error(state);
    }
}

void get_symbol(parse_state *state, char c)
{
    debug_print(c);
    switch (state->G.top())
    {
    case G_ELEMENT:
        if (match_symbol(c, '{'))
        {
            match_element_dict(state);
        }
        else if (match_symbol(c, '['))
        {
            match_element_array(state);
        }
        else if (match_symbol(c, ']'))
        {
            match_array_end(state);                        
        }  
        else
        {
            throw_error(state);
        }
        break;
    case G_VALUE:
        if (match_symbol(c, '{'))
        {
            match_value_dict(state);
        }        
        else if (match_symbol(c, '['))
        {
            match_value_array(state);
        }
        else
        {
            throw_error(state);
        }
        break;
    case G_KEYSEP:
        if (!match_symbol(c, ':'))
        {
            throw_error(state);
        }
        state->G.top() = G_VALUE;
        break;
    case G_ARRAYSEP:
        if (match_symbol(c, ','))
        {
            state->G.top() = G_ELEMENT; 
        }
        else if (match_symbol(c, ']'))
        {                 
            match_array_end(state);
        }   
        else
        {
            throw_error(state);
        }         
        break;
    case G_DICTSEP:
        if (match_symbol(c, ','))
        {
            state->G.top() = G_KEY;                   
        }
        else if (match_symbol(c, '}'))
        { 
            match_dict_end(state);
        }
        else
        {
            throw_error(state);
        }
        break;
    case G_KEY:
        if (match_symbol(c, '}'))
        {
            match_dict_end(state);      
        }
        else
        {
            throw_error(state);
        }
        break;
    case G_START:
        if (match_symbol(c, '{'))
        {
            match_dict(state);
        }
        else if (match_symbol(c, '['))
        {
            match_array(state);
        }
        else
        {
            throw_error(state);
        }            
        break;
    default:
        throw_error(state);
    }
}

static inline void jump_space(parse_state *state)
{
    change_syntex(state, S_SPACE_END);
    parse_score &score = state->score;
    size_t &i = state->score_pos;
#ifdef _MSC_VER
    while (i < score.size - 32)
    {
        const char *space32 = "                                ";
        if (memcmp(score.buff+i, space32, 32) != 0)
        {
            break;
        }
        i += 32;
    }
    while (i < score.size - 16)
    {
        const char *space16 = "                ";
        if (memcmp(score.buff+i, space16, 16) != 0)
        {
            break;
        }
        i += 16;
    }
    while (i < score.size - 8)
    {
        const char *space8 = "        ";
        if (memcmp(score.buff+i, space8, 8) != 0)
        {
            break;
        }
        i += 8;
    }
    while (i < score.size - 4)
    {
        const char *space4 = "    ";
        if (memcmp(score.buff+i, space4, 4) != 0)
        {
            break;
        }
        i += 4;
    }
#endif
    while(is_space(score[i++]));
    state->score_pos--;
}

static void proc_word(parse_state *state)
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    char c = score[i++];

    for (;;)
    {
        if (is_space(c))
        {
            get_word(state);
            jump_space(state);   
            break;
        }
        else if (is_symbol(c))
        {
            get_word(state);
            change_syntex(state, S_SYMBOL);
            get_symbol(state, c);
            break;
        }
        else if (is_EOF(c))
        {
            get_word(state);
            break;
        }
        append_syntex(c, state);
        c = score[i++];
    }
}

static void proc_number(parse_state *state)
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    char c = score[i++];

    while (is_digit(c))
    {
        append_syntex(c, state);
        c = score[i++];
    }

    if (is_space(c))
    {
        get_number(state);
        jump_space(state);
    }
    else if(is_symbol(c))
    {
        get_number(state);
        change_syntex(state, S_SYMBOL);
        get_symbol(state, c);
    }
    else if (c == 'e' || c == 'E')
    {        
        append_syntex(c, state);
        c = score[i++];

        if (c == '-' || c == '+')
        {            
            append_syntex(c, state);
            c = score[i++];
            if (is_digit(c))
            {
                state->S = S_FLOAT_E;
            }   
            else
            {
                state->S = S_WORD;
            }
        }        
        else if (is_digit(c))
        {
            state->S = S_FLOAT_E;
        }
        else
        {
            state->S = S_WORD;
        }
        append_syntex(c, state);
    }
    else if (c == '.')
    {
        state->S = S_FLOAT;
        append_syntex(c, state);
    }
    else
    {
        state->S = S_WORD;
        append_syntex(c, state);
    }
}

static void proc_float(parse_state *state)
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    char c = score[i++];

L_FLOAT_E:       
    while (is_digit(c))
    {
        append_syntex(c, state);
        c = score[i++];
    }

    if (is_space(c))
    {
        get_float(state);
        jump_space(state);
    }
    else if (is_symbol(c))
    {
        get_float(state);
        change_syntex(state, S_SYMBOL);
        get_symbol(state, c);
    }
    else if ((c == 'e' || c == 'E') && state->S != S_FLOAT_E)
    {
        append_syntex(c, state);
        c = score[i++];
        if (c == '-' || c == '+')
        {            
            append_syntex(c, state);
            c = score[i++];
            if (is_digit(c))
            {
                state->S = S_FLOAT_E;
            }   
            else
            {
                state->S = S_WORD;
            }
        }        
        else if (is_digit(c))
        {
            state->S = S_FLOAT_E;
        }
        else
        {
            state->S = S_WORD;
        }
        append_syntex(c, state);   
        if (state->S == S_FLOAT_E)
        {
            goto L_FLOAT_E;
        }
    }
    else
    {
        state->S = S_WORD;
        append_syntex(c, state);
    }
}

static void proc_space_end(parse_state *state)
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    char c = score[i];

    if (is_symbol(c))
    {
        state->S = S_SYMBOL;
        get_symbol(state, c);
    }
    else if (is_digit(c) || c == '-')
    {
        state->S = S_NUMBER;
        append_syntex(c, state);
    }
    else if (c == '\"' || c == '\'')
    {
        state->S = S_STRING;
        state->string_begin = c;
    }
    else if(c == '.')
    {
        state->S = S_FLOAT;
        append_syntex(c, state);
    }
    else
    {
        state->S = S_WORD;
        append_syntex(c, state);
    }
    i++;
}

static void proc_start(parse_state *state)
{    
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    char c = score[i++];

    if( is_symbol(c))
    {
        state->S = S_SYMBOL;
        get_symbol(state, c);
    }
    else if (is_space(c))
    {
        jump_space(state);
    }
    else if (is_digit(c) || c == '-')
    {
        state->S = S_NUMBER;
        append_syntex(c, state);
    }
    else if(c == '.')
    {
        state->S = S_FLOAT;
        append_syntex(c, state);
    }
    else if (c == '\"' || c == '\'')
    {
        state->S = S_STRING;
        state->string_begin = c;
    }
    else
    {
        throw_error(state);
    }
}

static void proc_symbol(parse_state *state)
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    char c = score[i++];

    while (is_symbol(c))
    {
        get_symbol(state, c);
        c = score[i++];
    }

    if (is_space(c))
    {
        jump_space(state);
    }
    else if (is_digit(c) || c == '-')
    {
        state->S = S_NUMBER;
        append_syntex(c, state);
    }
    else if (c == '.')
    {
        state->S = S_FLOAT;
        append_syntex(c, state);
    }
    else if (c == '\"' || c == '\'')
    {
        state->S = S_STRING;
        state->string_begin = c;
    }
    else
    {
        state->S = S_WORD;
        append_syntex(c, state);
    }
}

static void codePointToUTF8(parse_state *state, unsigned int cp)
{
    // based on description from http://en.wikipedia.org/wiki/UTF-8

    if (cp <= 0x7f) 
    {
        append_syntex(static_cast<char>(cp), state);
    } 
    else if (cp <= 0x7FF) 
    {
        append_syntex(static_cast<char>(0xC0 | (0x1f & (cp >> 6))), state);
        append_syntex(static_cast<char>(0x80 | (0x3f & cp)), state);
    } 
    else if (cp <= 0xFFFF) 
    {
        append_syntex(0xE0 | static_cast<char>((0xf & (cp >> 12))), state);
        append_syntex(0x80 | static_cast<char>((0x3f & (cp >> 6))), state);
        append_syntex(static_cast<char>(0x80 | (0x3f & cp)), state);                
    }
    else if (cp <= 0x10FFFF) 
    {
        append_syntex(static_cast<char>(0xF0 | (0x7 & (cp >> 18))), state);  
        append_syntex(static_cast<char>(0x80 | (0x3f & (cp >> 12))), state);  
        append_syntex(static_cast<char>(0x80 | (0x3f & (cp >> 6))), state); 
        append_syntex(static_cast<char>(0x80 | (0x3f & cp)), state); 
    }
}

static void decodeUnicodeEscapeSequence(parse_state *state, 
                                        unsigned int &unicode )
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    if ( score.size - i < 4 )
    {
        throw_error(state);
    }
    unicode = 0;
    for ( int index =0; index < 4; ++index )
    {
        char c = score[i++];
        unicode *= 16;
        if ( c >= '0'  &&  c <= '9' )
            unicode += c - '0';
        else if ( c >= 'a'  &&  c <= 'f' )
            unicode += c - 'a' + 10;
        else if ( c >= 'A'  &&  c <= 'F' )
            unicode += c - 'A' + 10;
        else
        {
            throw_error(state);
        }
    }
}

static void decodeUnicodeCodePoint(parse_state *state, 
                            unsigned int &unicode )
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;

    decodeUnicodeEscapeSequence( state, unicode );
    if (unicode >= 0xD800 && unicode <= 0xDBFF)
    {
        // surrogate pairs
        if (score.size - i < 6)
        {
            throw_error(state);
        }
        unsigned int surrogatePair;
        if (score[i++] == '\\' && score[i++] == 'u')
        {
            decodeUnicodeEscapeSequence( state, surrogatePair );
            unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
        } 
        else
        {
            throw_error(state);
        }
    }
}

static void proc_string(parse_state *state)
{
    parse_score &score = state->score;
    size_t &i = state->score_pos;
    char c = score[i++];

    while (c != state->string_begin)
    {
        if (c == '\\')
        {
            c = score[i++];
            if (c == 't')
            {
                c = '\t';
            }
            else if (c == 'n')
            {
                c = '\n';
            }
            else if (c == 'r')
            {
                c = '\r';
            }
            else if (c == '\'')
            {
                c = '\'';
            }
            else if (c == '\"')
            {
                c = '\"';
            }
            else if (c == '\\')
            {
                c = '\\';
            }
            else if (c == 'b')
            {
                c = '\b';
            }
            else if (c == 'f')
            {
                c = '\f';
            }
            else if (c == '/')
            {
                c = '/';
            }
            else if (c == 'u' && state->score.size - 1 > 4)
            {
                unsigned int unicode = 0;
                decodeUnicodeCodePoint( state, unicode );
                codePointToUTF8(state, unicode);
            }
            else
            {
                throw_error(state);
            }
        }
        append_syntex(c, state);
        c = score[i++];
    }

    c = score[i++];
    if (is_symbol(c))
    {
        get_string(state);
        change_syntex(state, S_SYMBOL);
        get_symbol(state, c);
    }
    else if (is_space(c))
    {
        get_string(state);
        jump_space(state);
    }
    else
    {
        get_string(state);
        change_syntex(state, S_WORD);
        append_syntex(c, state);
    }
}

static void _parse(const char *score, size_t len, Value *root)
{
    parse_state *state = new parse_state;
    state->S = S_START;
    state->score_pos = 0;
    state->syntex_len = 0;
    state->score.buff = score;
    state->score.size = len;
    state->curval = root;
    state->G.push(G_START);
    state->G.state = state;
    proc_start(state);
    while (state->score_pos < len)
    {
        switch(state->S)
        {       
        case S_STRING:
            proc_string(state);
            break;

        case S_SYMBOL:
            proc_symbol(state);
            break;

        case S_FLOAT:
            proc_float(state);
            break;

        case S_NUMBER:
            proc_number(state);
            break;

        case S_SPACE_END:
            proc_space_end(state);
            break;

        case S_WORD:
            proc_word(state);
            break;

        default:
            throw_error(state);
        } // switch
    }     

    if (!state->G.empty())
    {
        throw_error(state);
    }
}

size_t tjson::parse(const char *s, size_t len, Value *root)
{
    try {
        _parse(s, len, root);
        return 0;
    } catch(tjException &ex) {
        return ex.pos;
    }    
}


#if PREALLOC
#define mempool_init_count 191
#define pool_init_size 0
#define pool_increase 1
#else
#define mempool_init_count 0
#define pool_init_size 0
#define pool_increase 1
#endif

#define align_size 8
#define mem_align(d) (((d) + (align_size - 1)) & ~(align_size - 1))
static std::vector<std::vector<void *> > g_mem_list;
#if DEBUG_MEM
static std::vector<size_t> g_memdgb_list;
#endif
#if PREALLOC
#ifdef _MSC_VER
static const int memsizetable[] = {1,1336, 4,2669, 15,1386, 16,88, 17,86, 18,86, 19,54, 20,26, 21,4, 22,2, 159,435, 191,502};
#else
static const int memsizetable[] = {1,1336, 4,2669, 15,1386, 16,88, 17,86, 18,86, 19,54, 20,26, 21,4, 22,2, 159,435, 191,502};
#endif
#else
static const int memsizetable[2] = {-1,-1};
#endif
static int findmemtable(size_t idx)
{
    for (int i = 0; i < (int)(sizeof(memsizetable) / sizeof(int)); i += 2)
    {
        if ((int)idx == memsizetable[i])
        {
            return memsizetable[i+1];
        }        
    }
    return -1;
}

static struct mem_pool_init_util
{
    static void init_pool(size_t poolidx, size_t addsize)
    {
        for (size_t i = 0; i < addsize; i++)
        {
            g_mem_list[poolidx].push_back(malloc(align_size * (poolidx+1)));
        }
    }
    mem_pool_init_util()
    {
        g_mem_list.resize(mempool_init_count);
#if DEBUG_MEM
        g_memdgb_list.resize(mempool_init_count);
#endif
        for (size_t i = 1; i <= mempool_init_count; i++)
        {            
            init_pool(i-1, (findmemtable(i-1) != -1)?findmemtable(i-1):pool_init_size);
        }
    }

    ~mem_pool_init_util()
    {
        for (size_t i = 0; i < g_mem_list.size(); i++)
        {
            if (g_mem_list[i].size() != 0)
            {
#if DEBUG_MEM
                printf("%lu,%lu, ", i, g_mem_list[i].size());
#endif
                for (std::vector<void*>::iterator it = g_mem_list[i].begin();
                    it != g_mem_list[i].end(); ++it)
                {
                    free(*it);
                }
            }             
        }

#if DEBUG_MEM
        for (size_t i = 0; i < g_memdgb_list.size(); i++)
        {
            if (g_memdgb_list[i] != 0)
            {
                printf("leak %lu %lu\n", i, g_memdgb_list[i]);
            }            
        }
#endif
    }
}g_initutil;

void *tjson::internal::jsmalloc( size_t s )
{
    size_t pool_idx = mem_align(s) / align_size - 1;    
    if (pool_idx >= g_mem_list.size())
    {
        g_mem_list.resize(pool_idx + 1);    
#if DEBUG_MEM
        g_memdgb_list.resize(pool_idx + 1);
#endif
    }

    if (g_mem_list[pool_idx].empty())
    {
#if DEBUG_MEM
        printf("%lu increase %d\n", pool_idx, pool_increase);        
#endif
        mem_pool_init_util::init_pool(pool_idx, pool_increase);
    }

    void *p = g_mem_list[pool_idx].back();
    g_mem_list[pool_idx].pop_back();
#if DEBUG_MEM
    g_memdgb_list[pool_idx]++;
#endif
    return p;
}

void internal::jsfree( void *p, size_t s )
{
    size_t pool_idx = mem_align(s) / align_size - 1;      
    if (pool_idx >= g_mem_list.size())
    {
        free(p);
        return;
    }

    g_mem_list[pool_idx].push_back(p);
#if DEBUG_MEM
    g_memdgb_list[pool_idx]--;
#endif
}

void tjson::Value::internal_build_string( const char *s, size_t l )
{
    assert(m_type == JT_NULL);
    assert(!m_strval);
    m_type = JT_STRING;
    m_strval = new String(s, l);
}

void tjson::Value::internal_build_object()
{
    assert(m_type == JT_NULL);
    assert(!m_dict);
    m_type = JT_OBJECT;
    m_dict = new Map;
}

void tjson::Value::internal_build_array()
{
    assert(m_type == JT_NULL);
    assert(!m_array);
    m_type = JT_ARRAY;
    m_array = new Vector;
}

void tjson::Value::internal_build_bool( bool v )
{
    assert(m_type == JT_NULL);
    assert(m_intval == 0);
    m_type = JT_BOOL;
    m_bool = v;
}

static double jsstrtod(const char *string, char **endPtr);
void tjson::Value::internal_build_float( const char *s )
{
    assert(m_type == JT_NULL);
    assert(m_intval == 0);
    m_type = JT_DOUBLE;
    m_fval = jsstrtod(s, NULL); //strtod(s, NULL);
}

static int64_t fs2i(const char* str);
void tjson::Value::internal_build_integer( const char *s )
{
    assert(m_type == JT_NULL);
    assert(m_intval == 0);
    m_type = JT_INTEGER;
    m_intval = fs2i(s);// strtoll(s, NULL, 10);
}

Value *tjson::Value::internal_add_key( const char *k, size_t l)
{
    assert(m_type == JT_OBJECT);
    assert(m_dict);
    return &(*m_dict)[String(k,l)];
}

Value *tjson::Value::internal_add()
{
    assert(m_type == JT_ARRAY);
    assert(m_array);
    Value *newV = m_array->push_back();
    assert(newV);
    return newV;
}

tjson::internal::VectorData::VectorData() 
    :ref(1)
    ,value_size(0)
    ,buff_capacity(ARRAY_INIT_SIZE)
{
    buff = (Value*)jsmalloc(sizeof(Value) * ARRAY_INIT_SIZE);
}

template <class T>
static void increase_capacity(T *&buff, size_t &old_capacity, size_t &old_size)
{
    size_t new_capacity = old_size * 2 + 1;
    T *newValues = (T *)jsmalloc(sizeof(T) * new_capacity);
    memcpy(newValues, buff, sizeof(T) * old_size); // direct copy memory!
    jsfree(buff, sizeof(T) * old_capacity); // no desconstruct!        
    buff = newValues;    
    old_capacity = new_capacity;
}

void tjson::internal::VectorData::increase_size()
{
    if (value_size + 1 > buff_capacity)
    {
        increase_capacity(buff, buff_capacity, value_size);
    }
    ::new(&buff[value_size++]) Value;
}

tjson::Value &tjson::internal::MapData::operator[](const tjson::internal::String &key)
{
    for (size_t i = 0; i < value_size; i++)
    {
        String &pair_key = buff[i].key;
        if (key.size() == pair_key.size() &&
            memcmp(key.c_str(), pair_key.c_str(), key.size()) == 0)
        {
            return buff[i].value;
        }
    }

    if (value_size + 1 > buff_capacity)
    {
        increase_capacity(buff, buff_capacity, value_size);
    }

    ::new(&buff[value_size].key) String(key);
    ::new(&buff[value_size].value) Value;
    return buff[value_size++].value;
}

tjson::internal::VectorData::~VectorData()
{
    assert(ref == 0);
    for (size_t i = 0; i < value_size; i++)
    {
        buff[i].~Value();
    }
    jsfree(buff, sizeof(Value) * buff_capacity);
}

internal::StringData &internal::StringData::operator=( const char *s )
{
    size_t newsize = strlen(s);
    if (newsize + 1 > buff_capacity)
    {
        size_t newcap = newsize * 2 + 1;

        char *newValues = (char *)jsmalloc(newcap);
        memcpy(newValues, s, newsize);
        newValues[newsize] = 0;
        jsfree(buff, buff_capacity); 

        buff = newValues;
        buff_capacity = newcap;        
    }
    else
    {
        memcpy(buff, s, newsize);
        buff[newsize] = 0;
    }
    value_size = newsize;

    return *this;
}

tjson::internal::StringData::StringData() 
    :ref(1)
    ,value_size(0)
    ,buff_capacity(STRING_INIT_SIZE)
{
    buff = (char *)jsmalloc(STRING_INIT_SIZE);
}

void internal::StringData::assign( const char *s, size_t len )
{
    if (len + 1 > buff_capacity)
    {
        size_t new_capacity = len + 1;
        jsfree(buff, buff_capacity); 
        buff = (char *)jsmalloc(new_capacity);
        buff_capacity = new_capacity;
    }

    value_size = len;
    memcpy(buff, s, len);
    buff[len] = 0;        
}


bool internal::String::operator<( const String &r ) const
{
    if (m_data)
    {
        if (r.m_data)
        {
            return strcmp(m_data->buff, r.m_data->buff) < 0;
        }
        return false;
    }
    else
    {
        return r.m_data != NULL;
    }
}

bool internal::String::operator==( const String &s ) const
{
    if (size() != s.size())
    {
        return false;
    }
    return strcmp(c_str(), s.c_str()) == 0;
}

const tjson::Value &tjson::Value::operator[](const char *k) const
{
    if (m_type == JT_OBJECT)
    {
        assert(m_dict);
        return m_dict->find(String(k,strlen(k)));
    }
    return Null;
}

tjson::Value &tjson::Value::operator[](const char *k)
{
    if (m_type == JT_OBJECT)
    {
        assert(m_dict);
        return (*m_dict)[String(k,strlen(k))];
    }
    static Value dummy;
    return dummy;
}

void tjson::Value::destroy()
{
    switch (m_type)
    {
    case JT_ARRAY:
        assert(m_array);
        delete m_array;
        break;
    case JT_OBJECT:
        assert(m_dict);
        delete m_dict;
        break;
    case JT_STRING:
        assert(m_strval);
        delete m_strval;
        break;
    default:
        break;
    }
    m_type = JT_NULL;
    m_intval = 0;
}

void tjson::Value::assign( const Value &v )
{
    using namespace internal;    
    switch (v.m_type)
    {
    case JT_ARRAY:            
        internal_build_array();
        *m_array = *v.m_array;
        assert(m_array);
        break;
    case JT_OBJECT:    
        assert(v.m_dict);
        internal_build_object();        
        *m_dict = *v.m_dict;
        assert(m_dict);
        break;
    case JT_STRING:     
        assert(v.m_strval);
        internal_build_string(v.m_strval->c_str(), v.m_strval->size());
        assert(m_strval);
        break;
    default:
        m_intval = v.m_intval;
        break;
    }
    m_type = v.m_type;
}

Value & tjson::Value::operator=( const Value &v )
{
    if (this == &v)
    {
        return *this;
    }

    if (m_type == JT_STRING)
    {
        if (m_strval == v.m_strval || m_strval->c_str() == v.m_strval->c_str())
        {
            return *this;
        }
    }
    else if (m_type == JT_ARRAY)
    {
        if (m_array == v.m_array || m_array->m_data == v.m_array->m_data)
        {
            return *this;
        }
    }
    else if (m_type == JT_OBJECT)
    {
        if (m_dict == v.m_dict || m_dict->m_data == v.m_dict->m_data)
        {
            return *this;
        }
    }

    destroy();
    assign(v);
    return *this;
}

internal::MapData::MapData() 
    :ref(1)
    ,value_size(0)
    ,buff_capacity(MAP_INIT_SIZE)
{
    buff = (pair*)jsmalloc(MAP_INIT_SIZE * sizeof(pair));
}


template <int SIZE, typename len_t>
struct StringUnit
{
    char  s[SIZE]; // content
    len_t l;       // content length
};

#define CONV_ONE_BYTE(index) \
    if ((c = str[index]) < '0' || c > '9') goto end; \
    val = val * 10 + (c ^ '0')

template<typename int_t>
static inline int_t fs2i(const char* str)
{
    bool neg = false;
    int c = 0;
    int_t val = 0;

    if ((c = str[0]) == '-')
        ++str, neg = true;
    else if (c == '+')
        ++str;

    CONV_ONE_BYTE(0);
    CONV_ONE_BYTE(1);
    CONV_ONE_BYTE(2);
    CONV_ONE_BYTE(3);
    CONV_ONE_BYTE(4);
    CONV_ONE_BYTE(5);
    CONV_ONE_BYTE(6);
    CONV_ONE_BYTE(7);
    CONV_ONE_BYTE(8);
    CONV_ONE_BYTE(9);
    CONV_ONE_BYTE(10);
    CONV_ONE_BYTE(11);
    CONV_ONE_BYTE(12);
    CONV_ONE_BYTE(13);
    CONV_ONE_BYTE(14);
    CONV_ONE_BYTE(15);
    CONV_ONE_BYTE(16);
    CONV_ONE_BYTE(17);
    CONV_ONE_BYTE(18);
    CONV_ONE_BYTE(19);

end:
    return neg ? -val : val;
}

static int64_t fs2i(const char* str) { return fs2i<int64_t>(str); }

static int maxExponent = 511;	/* Largest possible base 10 exponent.  Any
                                * exponent larger than this will already
                                * produce underflow or overflow, so there's
                                * no need to worry about additional digits.
                                */
static double powersOf10[] = {	/* Table giving binary powers of 10.  Entry */
    10.,			/* is 10^2^i.  Used to convert decimal */
    100.,			/* exponents into floating-point numbers. */
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};

const Value & tjson::internal::MapData::find( const String &key ) const
{
    for (size_t i = 0; i < value_size; i++)
    {
        String &buff_key = buff[i].key;
        if (key.size() == buff_key.size() && memcmp(key.c_str(), buff_key.c_str(), key.size()) == 0)
        {
            return buff[i].value;
        }
    }
    return Value::Null;
}

tjson::internal::MapData::~MapData()
{
    for (size_t i = 0; i < value_size; i++)
    {
        buff[i].~pair();
    }
    jsfree(buff, buff_capacity * sizeof(pair));
}


/*
*----------------------------------------------------------------------
*
* strtod --
*
*	This procedure converts a floating-point number from an ASCII
*	decimal representation to internal double-precision format.
*
* Results:
*	The return value is the double-precision floating-point
*	representation of the characters in string.  If endPtr isn't
*	NULL, then *endPtr is filled in with the address of the
*	next character after the last one that was part of the
*	floating-point number.
*
* Side effects:
*	None.
*
*----------------------------------------------------------------------
*/

static double jsstrtod(const char *string, char **endPtr)
{
    bool sign, expSign = false;
    double fraction, dblExp, *d;
    register const char *p;
    register int c;
    int exp = 0;		
    int fracExp = 0;	

    int mantSize;		
    int decPt;			

    const char *pExp;	

    p = string;
    if (*p == '-') {
        sign = true;
        p += 1;
    } else {
        if (*p == '+') {
            p += 1;
        }
        sign = false;
    }

    decPt = -1;
    for (mantSize = 0; ; mantSize += 1)
    {
        c = *p;
        if (!is_digit(c)) {
            if ((c != '.') || (decPt >= 0)) {
                break;
            }
            decPt = mantSize;
        }
        p += 1;
    }

    pExp  = p;
    p -= mantSize;
    if (decPt < 0) {
        decPt = mantSize;
    } else {
        mantSize -= 1;
    }
    if (mantSize > 18) {
        fracExp = decPt - 18;
        mantSize = 18;
    } else {
        fracExp = decPt - mantSize;
    }
    if (mantSize == 0) {
        fraction = 0.0;
        p = string;
        goto done;
    } else {
        int frac1, frac2;
        frac1 = 0;
        for ( ; mantSize > 9; mantSize -= 1)
        {
            c = *p;
            p += 1;
            if (c == '.') {
                c = *p;
                p += 1;
            }
            frac1 = 10*frac1 + (c - '0');
        }
        frac2 = 0;
        for (; mantSize > 0; mantSize -= 1)
        {
            c = *p;
            p += 1;
            if (c == '.') {
                c = *p;
                p += 1;
            }
            frac2 = 10*frac2 + (c - '0');
        }
        fraction = (1.0e9 * frac1) + frac2;
    }

    p = pExp;
    if ((*p == 'E') || (*p == 'e')) {
        p += 1;
        if (*p == '-') {
            expSign = true;
            p += 1;
        } else {
            if (*p == '+') {
                p += 1;
            }
            expSign = false;
        }
        if (!is_digit(*p)) {
            p = pExp;
            goto done;
        }
        while (is_digit(*p)) {
            exp = exp * 10 + (*p - '0');
            p += 1;
        }
    }
    if (expSign) {
        exp = fracExp - exp;
    } else {
        exp = fracExp + exp;
    }

    if (exp < 0) {
        expSign = true;
        exp = -exp;
    } else {
        expSign = false;
    }
    if (exp > maxExponent) {
        exp = maxExponent;
    }
    dblExp = 1.0;
    for (d = powersOf10; exp != 0; exp >>= 1, d += 1) {
        if (exp & 01) {
            dblExp *= *d;
        }
    }
    if (expSign) {
        fraction /= dblExp;
    } else {
        fraction *= dblExp;
    }

done:
    if (endPtr != NULL) {
        *endPtr = (char *) p;
    }

    if (sign) {
        return -fraction;
    }
    return fraction;
}
