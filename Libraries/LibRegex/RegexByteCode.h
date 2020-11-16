/*
 * Copyright (c) 2020, Emanuel Sprung <emanuel.sprung@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RegexMatch.h"
#include "RegexOptions.h"

#include <AK/Forward.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/OwnPtr.h>
#include <AK/Traits.h>
#include <AK/Types.h>
#include <AK/Vector.h>

namespace regex {

using ByteCodeValueType = size_t;

#define ENUMERATE_OPCODES                          \
    __ENUMERATE_OPCODE(Compare)                    \
    __ENUMERATE_OPCODE(Jump)                       \
    __ENUMERATE_OPCODE(ForkJump)                   \
    __ENUMERATE_OPCODE(ForkStay)                   \
    __ENUMERATE_OPCODE(SaveLeftCaptureGroup)       \
    __ENUMERATE_OPCODE(SaveRightCaptureGroup)      \
    __ENUMERATE_OPCODE(SaveLeftNamedCaptureGroup)  \
    __ENUMERATE_OPCODE(SaveRightNamedCaptureGroup) \
    __ENUMERATE_OPCODE(CheckBegin)                 \
    __ENUMERATE_OPCODE(CheckEnd)                   \
    __ENUMERATE_OPCODE(Exit)

enum class OpCodeId : ByteCodeValueType {
#define __ENUMERATE_OPCODE(x) x,
    ENUMERATE_OPCODES
#undef __ENUMERATE_OPCODE

        First
    = Compare,
    Last
    = Exit,
};

#define ENUMERATE_CHARACTER_COMPARE_TYPES         \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(Undefined) \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(Inverse)   \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(AnyChar)   \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(Char)      \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(String)    \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(CharClass) \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(CharRange) \
    __ENUMERATE_CHARACTER_COMPARE_TYPE(RangeExpressionDummy)

enum class CharacterCompareType : ByteCodeValueType {
#define __ENUMERATE_CHARACTER_COMPARE_TYPE(x) x,
    ENUMERATE_CHARACTER_COMPARE_TYPES
#undef __ENUMERATE_CHARACTER_COMPARE_TYPE
};

#define ENUMERATE_CHARACTER_CLASSES    \
    __ENUMERATE_CHARACTER_CLASS(Alnum) \
    __ENUMERATE_CHARACTER_CLASS(Cntrl) \
    __ENUMERATE_CHARACTER_CLASS(Lower) \
    __ENUMERATE_CHARACTER_CLASS(Space) \
    __ENUMERATE_CHARACTER_CLASS(Alpha) \
    __ENUMERATE_CHARACTER_CLASS(Digit) \
    __ENUMERATE_CHARACTER_CLASS(Print) \
    __ENUMERATE_CHARACTER_CLASS(Upper) \
    __ENUMERATE_CHARACTER_CLASS(Blank) \
    __ENUMERATE_CHARACTER_CLASS(Graph) \
    __ENUMERATE_CHARACTER_CLASS(Punct) \
    __ENUMERATE_CHARACTER_CLASS(Xdigit)

enum class CharClass : ByteCodeValueType {
#define __ENUMERATE_CHARACTER_CLASS(x) x,
    ENUMERATE_CHARACTER_CLASSES
#undef __ENUMERATE_CHARACTER_CLASS
};

struct CharRange {
    const char from;
    const char to;

    CharRange(size_t value)
        : from(value >> 8)
        , to(value & 0xFF)
    {
    }

    CharRange(char from, char to)
        : from(from)
        , to(to)
    {
    }

    operator ByteCodeValueType() const { return (from << 8) | to; }
};

struct CompareTypeAndValuePair {
    CharacterCompareType type;
    ByteCodeValueType value;
};

class OpCode;

class ByteCode : public Vector<ByteCodeValueType> {
public:
    ByteCode() = default;
    virtual ~ByteCode() = default;

    void insert_bytecode_compare_values(Vector<CompareTypeAndValuePair>&& pairs)
    {
        ByteCode bytecode;

        bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::Compare));
        bytecode.empend(pairs.size()); // number of arguments

        ByteCode arguments;
        for (auto& value : pairs) {
            ASSERT(value.type != CharacterCompareType::RangeExpressionDummy);
            ASSERT(value.type != CharacterCompareType::Undefined);
            ASSERT(value.type != CharacterCompareType::String);

            arguments.append((ByteCodeValueType)value.type);
            if (value.type != CharacterCompareType::Inverse && value.type != CharacterCompareType::AnyChar)
                arguments.append(move(value.value));
        }

        bytecode.empend(arguments.size()); // size of arguments
        bytecode.append(move(arguments));

        append(move(bytecode));
    }

    void insert_bytecode_compare_string(StringView view, size_t length)
    {
        ByteCode bytecode;

        bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::Compare));
        bytecode.empend(1); // number of arguments

        ByteCode arguments;

        arguments.empend(static_cast<ByteCodeValueType>(CharacterCompareType::String));
        arguments.empend(reinterpret_cast<ByteCodeValueType>(view.characters_without_null_termination()));
        arguments.empend(length);

        bytecode.empend(arguments.size()); // size of arguments
        bytecode.append(move(arguments));

        append(move(bytecode));
    }

    void insert_bytecode_group_capture_left(size_t capture_groups_count)
    {
        empend(static_cast<ByteCodeValueType>(OpCodeId::SaveLeftCaptureGroup));
        empend(capture_groups_count);
    }

    void insert_bytecode_group_capture_left(const StringView& name)
    {
        empend(static_cast<ByteCodeValueType>(OpCodeId::SaveLeftNamedCaptureGroup));
        empend(reinterpret_cast<ByteCodeValueType>(name.characters_without_null_termination()));
        empend(name.length());
    }

    void insert_bytecode_group_capture_right(size_t capture_groups_count)
    {
        empend(static_cast<ByteCodeValueType>(OpCodeId::SaveRightCaptureGroup));
        empend(capture_groups_count);
    }

    void insert_bytecode_group_capture_right(const StringView& name)
    {
        empend(static_cast<ByteCodeValueType>(OpCodeId::SaveRightNamedCaptureGroup));
        empend(reinterpret_cast<ByteCodeValueType>(name.characters_without_null_termination()));
        empend(name.length());
    }

    void insert_bytecode_alternation(ByteCode&& left, ByteCode&& right)
    {

        // FORKSTAY _ALT
        // REGEXP ALT1
        // JUMP  _END
        // LABEL _ALT
        // REGEXP ALT2
        // LABEL _END

        ByteCode byte_code;

        empend(static_cast<ByteCodeValueType>(OpCodeId::ForkJump));
        empend(left.size() + 2); // Jump to the _ALT label

        for (auto& op : left)
            append(move(op));

        empend(static_cast<ByteCodeValueType>(OpCodeId::Jump));
        empend(right.size()); // Jump to the _END label

        // LABEL _ALT = bytecode.size() + 2

        for (auto& op : right)
            append(move(op));

        // LABEL _END = alterantive_bytecode.size
    }

    void insert_bytecode_repetition_min_max(ByteCode& bytecode_to_repeat, size_t minimum, Optional<size_t> maximum)
    {
        ByteCode new_bytecode;
        new_bytecode.insert_bytecode_repetition_n(bytecode_to_repeat, minimum);

        if (maximum.has_value()) {
            if (maximum.value() > minimum) {
                auto diff = maximum.value() - minimum;
                new_bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkStay));
                new_bytecode.empend(diff * (bytecode_to_repeat.size() + 2)); // Jump to the _END label

                for (size_t i = 0; i < diff; ++i) {
                    new_bytecode.append(bytecode_to_repeat);
                    new_bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkStay));
                    new_bytecode.empend((diff - i - 1) * (bytecode_to_repeat.size() + 2)); // Jump to the _END label
                }
            }
        } else {
            // no maximum value set, repeat finding if possible
            new_bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkJump));
            new_bytecode.empend(-bytecode_to_repeat.size() - 2); // Jump to the last iteration
        }

        bytecode_to_repeat = move(new_bytecode);
    }

    void insert_bytecode_repetition_n(ByteCode& bytecode_to_repeat, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            append(bytecode_to_repeat);
    }

    void insert_bytecode_repetition_min_one(ByteCode& bytecode_to_repeat, bool greedy)
    {
        // LABEL _START = -bytecode_to_repeat.size()
        // REGEXP
        // FORKJUMP _START  (FORKSTAY -> Greedy)

        if (greedy)
            bytecode_to_repeat.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkStay));
        else
            bytecode_to_repeat.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkJump));

        bytecode_to_repeat.empend(-(bytecode_to_repeat.size() + 1)); // Jump to the _START label
    }

    void insert_bytecode_repetition_any(ByteCode& bytecode_to_repeat, bool greedy)
    {
        // LABEL _START
        // FORKSTAY _END  (FORKJUMP -> Greedy)
        // REGEXP
        // JUMP  _START
        // LABEL _END

        // LABEL _START = m_bytes.size();
        ByteCode bytecode;

        if (greedy)
            bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkJump));
        else
            bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkStay));

        bytecode.empend(bytecode_to_repeat.size() + 2); // Jump to the _END label

        for (auto& op : bytecode_to_repeat)
            bytecode.append(move(op));

        bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::Jump));
        bytecode.empend(-bytecode.size() - 1); // Jump to the _START label
        // LABEL _END = bytecode.size()

        bytecode_to_repeat = move(bytecode);
    }

    void insert_bytecode_repetition_zero_or_one(ByteCode& bytecode_to_repeat, bool greedy)
    {
        // FORKSTAY _END (FORKJUMP -> Greedy)
        // REGEXP
        // LABEL _END
        ByteCode bytecode;

        if (greedy)
            bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkJump));
        else
            bytecode.empend(static_cast<ByteCodeValueType>(OpCodeId::ForkStay));

        bytecode.empend(bytecode_to_repeat.size()); // Jump to the _END label

        for (auto& op : bytecode_to_repeat)
            bytecode.append(move(op));
        // LABEL _END = bytecode.size()

        bytecode_to_repeat = move(bytecode);
    }

    OpCode* get_opcode(MatchState& state) const;

private:
    ALWAYS_INLINE OpCode* get_opcode_by_id(OpCodeId id) const;
    static HashMap<u32, OwnPtr<OpCode>> s_opcodes;
};

#define ENUMERATE_EXECUTION_RESULTS                          \
    __ENUMERATE_EXECUTION_RESULT(Continue)                   \
    __ENUMERATE_EXECUTION_RESULT(Fork_PrioHigh)              \
    __ENUMERATE_EXECUTION_RESULT(Fork_PrioLow)               \
    __ENUMERATE_EXECUTION_RESULT(Failed)                     \
    __ENUMERATE_EXECUTION_RESULT(Failed_ExecuteLowPrioForks) \
    __ENUMERATE_EXECUTION_RESULT(Succeeded)

enum class ExecutionResult : u8 {
#define __ENUMERATE_EXECUTION_RESULT(x) x,
    ENUMERATE_EXECUTION_RESULTS
#undef __ENUMERATE_EXECUTION_RESULT
};

const char* execution_result_name(ExecutionResult result);
const char* opcode_id_name(OpCodeId opcode_id);
const char* character_compare_type_name(CharacterCompareType result);
const char* execution_result_name(ExecutionResult result);

class OpCode {
public:
    OpCode(ByteCode& bytecode)
        : m_bytecode(&bytecode)
    {
    }

    virtual ~OpCode() = default;

    virtual OpCodeId opcode_id() const = 0;
    virtual size_t size() const = 0;
    virtual ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const = 0;

    ALWAYS_INLINE ByteCodeValueType argument(size_t offset) const
    {
        ASSERT(state().instruction_position + offset <= m_bytecode->size());
        return m_bytecode->at(state().instruction_position + 1 + offset);
    }

    ALWAYS_INLINE const char* name() const;
    static const char* name(const OpCodeId);

    ALWAYS_INLINE OpCode* set_state(MatchState& state)
    {
        m_state = &state;
        return this;
    }

    ALWAYS_INLINE OpCode* set_bytecode(ByteCode& bytecode)
    {
        m_bytecode = &bytecode;
        return this;
    }

    ALWAYS_INLINE void reset_state() { m_state.clear(); }

    ALWAYS_INLINE const MatchState& state() const
    {
        ASSERT(m_state.has_value());
        return *m_state.value();
    }

    const String to_string() const
    {
        return String::format("[0x%02X] %s", opcode_id(), name(opcode_id()));
    }

    virtual const String arguments_string() const = 0;

    ALWAYS_INLINE const ByteCode& bytecode() const { return *m_bytecode; }

protected:
    ByteCode* m_bytecode;
    Optional<MatchState*> m_state;
};

class OpCode_Exit final : public OpCode {
public:
    OpCode_Exit(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::Exit; }
    ALWAYS_INLINE size_t size() const override { return 1; }
    const String arguments_string() const override { return ""; }
};

class OpCode_Jump final : public OpCode {
public:
    OpCode_Jump(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::Jump; }
    ALWAYS_INLINE size_t size() const override { return 2; }
    ALWAYS_INLINE ssize_t offset() const { return argument(0); }
    const String arguments_string() const override
    {
        return String::format("offset=%i [&%lu]", offset(), state().instruction_position + size() + offset());
    }
};

class OpCode_ForkJump final : public OpCode {
public:
    OpCode_ForkJump(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::ForkJump; }
    ALWAYS_INLINE size_t size() const override { return 2; }
    ALWAYS_INLINE ssize_t offset() const { return argument(0); }
    const String arguments_string() const override
    {
        return String::format("offset=%i [&%lu], sp: %lu", offset(), state().instruction_position + size() + offset(), state().string_position);
    }
};

class OpCode_ForkStay final : public OpCode {
public:
    OpCode_ForkStay(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::ForkStay; }
    ALWAYS_INLINE size_t size() const override { return 2; }
    ALWAYS_INLINE ssize_t offset() const { return argument(0); }
    const String arguments_string() const override
    {
        return String::format("offset=%i [&%lu], sp: %lu", offset(), state().instruction_position + size() + offset(), state().string_position);
    }
};

class OpCode_CheckBegin final : public OpCode {
public:
    OpCode_CheckBegin(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::CheckBegin; }
    ALWAYS_INLINE size_t size() const override { return 1; }
    const String arguments_string() const override { return ""; }
};

class OpCode_CheckEnd final : public OpCode {
public:
    OpCode_CheckEnd(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::CheckEnd; }
    ALWAYS_INLINE size_t size() const override { return 1; }
    const String arguments_string() const override { return ""; }
};

class OpCode_SaveLeftCaptureGroup final : public OpCode {
public:
    OpCode_SaveLeftCaptureGroup(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::SaveLeftCaptureGroup; }
    ALWAYS_INLINE size_t size() const override { return 2; }
    ALWAYS_INLINE size_t id() const { return argument(0); }
    const String arguments_string() const override { return String::format("id=%lu", id()); }
};

class OpCode_SaveRightCaptureGroup final : public OpCode {
public:
    OpCode_SaveRightCaptureGroup(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::SaveRightCaptureGroup; }
    ALWAYS_INLINE size_t size() const override { return 2; }
    ALWAYS_INLINE size_t id() const { return argument(0); }
    const String arguments_string() const override { return String::format("id=%lu", id()); }
};

class OpCode_SaveLeftNamedCaptureGroup final : public OpCode {
public:
    OpCode_SaveLeftNamedCaptureGroup(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::SaveLeftNamedCaptureGroup; }
    ALWAYS_INLINE size_t size() const override { return 3; }
    ALWAYS_INLINE StringView name() const { return { reinterpret_cast<char*>(argument(0)), length() }; }
    ALWAYS_INLINE size_t length() const { return argument(1); }
    const String arguments_string() const override
    {
        return String::format("name=%s, length=%lu", name().to_string().characters(), length());
    }
};

class OpCode_SaveRightNamedCaptureGroup final : public OpCode {
public:
    OpCode_SaveRightNamedCaptureGroup(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::SaveRightNamedCaptureGroup; }
    ALWAYS_INLINE size_t size() const override { return 3; }
    ALWAYS_INLINE StringView name() const { return { reinterpret_cast<char*>(argument(0)), length() }; }
    ALWAYS_INLINE size_t length() const { return argument(1); }
    const String arguments_string() const override
    {
        return String::format("name=%s, length=%lu", name().to_string().characters(), length());
    }
};

class OpCode_Compare final : public OpCode {
public:
    OpCode_Compare(ByteCode& bytecode)
        : OpCode(bytecode)
    {
    }
    ExecutionResult execute(const MatchInput& input, MatchState& state, MatchOutput& output) const override;
    ALWAYS_INLINE OpCodeId opcode_id() const override { return OpCodeId::Compare; }
    ALWAYS_INLINE size_t size() const override { return arguments_size() + 3; }
    ALWAYS_INLINE size_t arguments_count() const { return argument(0); }
    ALWAYS_INLINE size_t arguments_size() const { return argument(1); }
    const String arguments_string() const override;
    const Vector<String> variable_arguments_to_string(Optional<MatchInput> input = {}) const;

private:
    ALWAYS_INLINE static void compare_char(const MatchInput& input, MatchState& state, u32 ch1, bool inverse, bool& inverse_matched);
    ALWAYS_INLINE static bool compare_string(const MatchInput& input, MatchState& state, const char* str, size_t length);
    ALWAYS_INLINE static void compare_character_class(const MatchInput& input, MatchState& state, CharClass character_class, char ch, bool inverse, bool& inverse_matched);
    ALWAYS_INLINE static void compare_character_range(const MatchInput& input, MatchState& state, char from, char to, char ch, bool inverse, bool& inverse_matched);
};

template<typename T>
bool is(const OpCode&);

template<typename T>
ALWAYS_INLINE bool is(const OpCode&)
{
    return false;
}

template<typename T>
ALWAYS_INLINE bool is(const OpCode* opcode)
{
    return is<T>(*opcode);
}

template<>
ALWAYS_INLINE bool is<OpCode_ForkStay>(const OpCode& opcode)
{
    return opcode.opcode_id() == OpCodeId::ForkStay;
}

template<>
ALWAYS_INLINE bool is<OpCode_Exit>(const OpCode& opcode)
{
    return opcode.opcode_id() == OpCodeId::Exit;
}

template<>
ALWAYS_INLINE bool is<OpCode_Compare>(const OpCode& opcode)
{
    return opcode.opcode_id() == OpCodeId::Compare;
}

template<typename T>
ALWAYS_INLINE const T& to(const OpCode& opcode)
{
    ASSERT(is<T>(opcode));
    return static_cast<const T&>(opcode);
}

template<typename T>
ALWAYS_INLINE T* to(OpCode* opcode)
{
    ASSERT(is<T>(opcode));
    return static_cast<T*>(opcode);
}

template<typename T>
ALWAYS_INLINE const T* to(const OpCode* opcode)
{
    ASSERT(is<T>(opcode));
    return static_cast<const T*>(opcode);
}

template<typename T>
ALWAYS_INLINE T& to(OpCode& opcode)
{
    ASSERT(is<T>(opcode));
    return static_cast<T&>(opcode);
}

}