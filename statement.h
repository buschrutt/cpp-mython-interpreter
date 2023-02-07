#pragma once

#include "runtime.h"

#include <functional>

namespace ast {

    using Statement = runtime::Executable;

    // Returns value of T, is used as constant creation base
    template <typename T>
    class ValueStatement : public Statement {
    public:
        explicit ValueStatement(T v): value_(std::move(v)) { }

        runtime::ObjectHolder Execute([[maybe_unused]] runtime::Closure& closure, [[maybe_unused]] runtime::Context& context) override {
            return runtime::ObjectHolder::Share(value_);
        }

    private:
        T value_;
    };

    using NumericConst = ValueStatement<runtime::Number>;
    using StringConst = ValueStatement<runtime::String>;
    using BoolConst = ValueStatement<runtime::Bool>;

    // Calculates the value of any chain of object field calls: id1.id2.id3.
    // An example circle.center.x - object field call chain in instruction: x = circle.center.x
    class VariableValue : public Statement {
    private:
        std::vector<std::string> dotted_ids_;

    public:
        explicit VariableValue(std::string  var_name);
        explicit VariableValue(std::vector<std::string>  dotted_ids);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Assigns the variable var to the value of the rv expression
    class Assignment : public Statement {
    private:
        std::string var_;
        std::unique_ptr<Statement> rv_;

    public:
        Assignment(std::string var, std::unique_ptr<Statement> rv);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Assigns field object.field_name to the value of the rv expression
    class FieldAssignment : public Statement {
    private:
        VariableValue object_;
        std::string field_name_;
        std::unique_ptr<Statement> rv_;

    public:
        FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Value None
    class None : public Statement {
    public:
        runtime::ObjectHolder Execute([[maybe_unused]] runtime::Closure& closure, [[maybe_unused]] runtime::Context& context) override {
        return {};
        }
    };

    // Print command
    class Print : public Statement {
    public:
        // Initializes the print command to print the value of the argument expression
        explicit Print(std::unique_ptr<Statement> argument);
        // Initializes the print command to output args values
        explicit Print(std::vector<std::unique_ptr<Statement>> args);

        // Initializes the print command to output name variable
        static std::unique_ptr<Print> Variable(const std::string& name);

        // While print execution output has to be into stream that returns from context.GetOutputStream()
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

    private:
        std::vector<std::unique_ptr<Statement>> args_;
    };

    // Calls object.method with list of args params
    class MethodCall : public Statement {
    private:
        std::unique_ptr<Statement> object_;
        std::string method_; // name
        std::vector<std::unique_ptr<Statement>> args_;

    public:
        MethodCall(std::unique_ptr<Statement> object, std::string method, std::vector<std::unique_ptr<Statement>> args);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    /*
    Creates new class class_ instance passing list of params args. If method has no __init__ with args size then instance is made without constructor calling -- instance fields won't be initialized:
    class Person:
      def set_name(name):
        self.name = name
        p = Person() # Field will have a value only within set_name method calling
        p.set_name("TheName")
    */
    class NewInstance : public Statement {
    private:
        runtime::ClassInstance class_instance_;
        std::vector<std::unique_ptr<Statement>> args_;

    public:
        explicit NewInstance(const runtime::Class& class_);
        NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args);

        // returns object that has value of ClassInstance type
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Unary operations base class
    class UnaryOperation : public Statement {
    protected:
        std::unique_ptr<Statement> argument_;
    public:
        explicit UnaryOperation(std::unique_ptr<Statement> argument): argument_(std::move(argument)) {}
    };

    // Operation str returns value of own arg
    class Stringify : public UnaryOperation {
    public:
        using UnaryOperation::UnaryOperation;
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Parent class Binary operation with lhs & rhs args
    class BinaryOperation : public Statement {
    public:
        BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs):
        lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

    protected:
        std::unique_ptr<Statement> lhs_;
        std::unique_ptr<Statement> rhs_;
    };

    // Returns lhs and rhs + result
    class Add : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Supports addition: number + number; string + string; obj1 + obj if obj1 has class with _add__(rhs) method
        // Else runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Returns lhs and rhs - result
    class Sub : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Supports subtraction: number - number; if lhs or rhs aren't numbers - runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Returns lhs and rhs multiplication result
    class Mult : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Supports multiplication: number * number; if lhs or rhs aren't numbers throws  runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Returns lhs and rhs division result
    class Div : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Supports division: number / number; if lhs or rhs aren't numbers throws runtime_error
        // If rhs = 0 throws runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Returns logical OR expression result over rhs and rhs
    class Or : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // rhs value rhs is calculated if lhs value within cast to Bool is False
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Returns logical AND expression result over rhs and rhs
    class And : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // rhs value rhs is calculated if lhs value within cast to Bool is True
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Returns logical NOT expression result over single arg
    class Not : public UnaryOperation {
    public:
        using UnaryOperation::UnaryOperation;
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Compound instruction --example: method body, contents of if or else branch
    class Compound : public Statement {
    public:
        // Build Compound from few instruction with type of unique_ptr<Statement>
        template <typename... Args>
        explicit Compound(Args&&... args) {
            if constexpr (sizeof...(args) > 0) {
                AddNewArgument(std::forward<Args>(args)...);
            }
        }

        // Adds next instruction to the query of compound instruction
        void AddStatement(std::unique_ptr<Statement> stmt) {
            args_.push_back(std::move(stmt));
        }

        // Executes the added instruction within query. Returns None
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

    private:
        std::vector<std::unique_ptr<Statement>> args_;

        template <typename Arg_null, typename... Args>
        void AddNewArgument(Arg_null&& arg, Args&&... args) {
            args_.push_back(std::forward<Arg_null>(arg));
            if constexpr (sizeof...(args) > 0) {
                AddNewArgument(std::forward<Args>(args)...);
            }
        }
    };

    // Method Body. As usual, contains compound instruction
    class MethodBody : public Statement {
    private:
        std::unique_ptr<Statement> body_;

    public:
        explicit MethodBody(std::unique_ptr<Statement>&& body);

        // Calculates instruction that passed as body. If body has return, returns result. Else returns None
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Executes return instruction with the statement
    class Return : public Statement {
    private:
        std::unique_ptr<Statement> statement_;

    public:
        explicit Return(std::unique_ptr<Statement> statement): statement_(std::move(statement)) {}

        // Stops the method. When executed the method within it was called has to return expression statement result.
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Declares the class
    class ClassDefinition : public Statement {
    private:
        runtime::ObjectHolder cls_;
    public:
        // ObjectHolder has an object with type of runtime::Class
        explicit ClassDefinition(runtime::ObjectHolder cls);

        // Creates a new obj inside closure that matches with name of the class and value that were passed to the constructor
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Instruction if <condition> <if_body> else <else_body>
    class IfElse : public Statement {
        std::unique_ptr<Statement> condition_;
        std::unique_ptr<Statement> if_body_;
        std::unique_ptr<Statement> else_body_;

    public:
        // Param else_body might be nullptr
        IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body, std::unique_ptr<Statement> else_body);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // Comparison operation
    class Comparison : public BinaryOperation {
    public:
        // Comparator defines a func that compares the values of the arguments
        using Comparator = std::function<bool(const runtime::ObjectHolder&, const runtime::ObjectHolder&, runtime::Context&)>;

        Comparison(Comparator cmp, std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs);

        // Calculates an expression lhs and rhs and returns a result of comparator expression that cast to the type of runtime::Bool
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

    private:
        Comparator cmp_;
    };

}  // namespace ast