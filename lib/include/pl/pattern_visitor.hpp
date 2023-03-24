#pragma once

namespace pl {

    namespace ptrn {

        class PatternArrayDynamic;
        class PatternArrayStatic;
        class PatternBitfield;
        class PatternBitfieldField;
        class PatternBitfieldArray;
        class PatternBoolean;
        class PatternCharacter;
        class PatternEnum;
        class PatternFloat;
        class PatternPadding;
        class PatternPointer;
        class PatternSigned;
        class PatternString;
        class PatternStruct;
        class PatternUnion;
        class PatternUnsigned;
        class PatternWideCharacter;
        class PatternWideString;

    }

    class PatternVisitor
    {
    public:
        virtual void visit(ptrn::PatternArrayDynamic& pattern) = 0;
        virtual void visit(ptrn::PatternArrayStatic& pattern) = 0;
        virtual void visit(ptrn::PatternBitfield& pattern) = 0;
        virtual void visit(ptrn::PatternBitfieldField& pattern) = 0;
        virtual void visit(ptrn::PatternBitfieldArray& pattern) = 0;
        virtual void visit(ptrn::PatternBoolean& pattern) = 0;
        virtual void visit(ptrn::PatternCharacter& pattern) = 0;
        virtual void visit(ptrn::PatternEnum& pattern) = 0;
        virtual void visit(ptrn::PatternFloat& pattern) = 0;
        virtual void visit(ptrn::PatternPadding& pattern) = 0;
        virtual void visit(ptrn::PatternPointer& pattern) = 0;
        virtual void visit(ptrn::PatternSigned& pattern) = 0;
        virtual void visit(ptrn::PatternString& pattern) = 0;
        virtual void visit(ptrn::PatternStruct& pattern) = 0;
        virtual void visit(ptrn::PatternUnion& pattern) = 0;
        virtual void visit(ptrn::PatternUnsigned& pattern) = 0;
        virtual void visit(ptrn::PatternWideCharacter& pattern) = 0;
        virtual void visit(ptrn::PatternWideString& pattern) = 0;
    };
}