#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace runtime {

    // Mython directions context
    class Context {
    public:
        // Returns stream for the print command
        [[maybe_unused]] virtual std::ostream& GetOutputStream() = 0;

    protected:
        ~Context() = default;
    };

    // Base class for all Mython objects
    class Object {
    public:
        virtual ~Object() = default;
        // Outputs object string representation
        virtual void Print(std::ostream& os, Context& context) = 0;
    };

    // Wrapper-class for the object storage
    class ObjectHolder {
    public:
        // Creates an empty value
        ObjectHolder() = default;

        // Returns ObjectHolder, that owns object of T type - which is specific child class of object
        // The object is copied and moves to heap
        template <typename T>
        [[nodiscard]] static ObjectHolder Own(T&& object) {
            return ObjectHolder(std::make_shared<T>(std::forward<T>(object)));
        }

        // Creates ObjectHolder that doesn't own the object (weak pointer)
        [[nodiscard]] static ObjectHolder Share(Object& object);
        // Creates an empty ObjectHolder that equals None
        [[nodiscard]] static ObjectHolder None();

        // Returns an object pointer inside the ObjectHolder
        // ObjectHolder has has a value
        Object& operator*() const;
        Object* operator->() const;
        [[nodiscard]] Object* Get() const;

        // Returns T-type pointer or nullptr if no ObjectHolder inside T-type object
        template <typename T>
        [[nodiscard]] T* TryAs() const {
            return dynamic_cast<T*>(this->Get());
        }

        // Returns true, if ObjectHolder isn't empty
        explicit operator bool() const;

    private:
        explicit ObjectHolder(std::shared_ptr<Object> data);
        void AssertIsValid() const;
        std::shared_ptr<Object> data_;
    };

    // Object with a value of T-type
    template <typename T>
    class ValueObject : public Object {
    public:
        ValueObject(T v): value_(v) {}  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

        void Print(std::ostream& os, [[maybe_unused]] Context& context) override {
            os << value_;
        }

        [[nodiscard]] const T& GetValue() const {
            return value_;
        }

    private:
        T value_;
    };

    // Symbol table linking an object's name to its value
    using Closure = std::unordered_map<std::string, ObjectHolder>;

    // Checks, if the object has the value, that reduced to True
    // If value is not zero, True and not empty string - returns true, otherwise - false.
    bool IsTrue(const ObjectHolder& object);

    // Interface for the execution with Mython objects
    class Executable {
    public:
        virtual ~Executable() = default;

        // Execute an action over objects inside closure, using context
        // Returns a summary or None
        virtual ObjectHolder Execute(Closure& closure, Context& context) = 0;
    };

    // String value
    using String = ValueObject<std::string>;
    // Number
    using Number = ValueObject<int>;
    // Boolean

    class Bool : public ValueObject<bool> {
    public:
        using ValueObject<bool>::ValueObject;
        void Print(std::ostream& os, Context& context) override;
    };

    // Class method
    struct Method {
        std::string name;
        // Formal params name
        std::vector<std::string> formal_params;
        // Method's body
        std::unique_ptr<Executable> body;
    };

    // Class
    class Class : public Object {
    public:
        // Creates class with name and set of methods, implemented from parent class
        // If parent is nullptr then base class is crated
        explicit Class(std::string name, std::vector<Method> methods, const Class* parent);

        // Returns method pointer or nullptr, if no method name was found
        [[nodiscard]] const Method* GetMethod(const std::string& name) const;

        // Returns class name
        [[nodiscard]] const std::string& GetName() const;

        // Output string "Class <class name>", example: "Class cat"
        void Print(std::ostream& os, Context& context) override;

    private:
        std::string name_;
        std::vector<Method> methods_;
        const Class* parent_;
    };

    // Class instance
    class ClassInstance : public Object {
    public:
        explicit ClassInstance(const Class&  cls);

        // If object has __str__ method, output result to os, that was returned by the method
        // or object address is returned to os
        void Print(std::ostream& os, Context& context) override;

        // Calls object method, with actual_args params. Param context set the context for the method execution.
        // If class or parents contain no such method, it throws exception runtime_error
        ObjectHolder Call(const std::string& method, const std::vector<ObjectHolder>& actual_args,  Context& context);

        // Returns true, if object has --method, that accepts argument_count params
        [[nodiscard]] bool HasMethod(const std::string& method, size_t argument_count) const;

        // Returns Closure pointer, that contains object's fields
        [[nodiscard]] Closure& Fields();

        // Returns const Closure pointer, that contains object's fields
        [[nodiscard]] const Closure& Fields() const;

    private:
        Closure fields_;
        const Class& cls_;
    };

    //Returns true, if lhs & rhs are same numbers, strings or Bool values.
    //If lhs - with __eq__ method, func returns lhs.__eq__(rhs) call result as Bool.
    //If lhs & rhs are None, func returns true. Otherwise, throws runtime_error.
    // Context param defines context for __eq__ method executing
    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // If lhs & rhs numbers, strings or Bool values funcs returns < compilation result
    // If lhs is object that has __lt__ method returns lhs.__lt__(rhs) call result reduced to bool-type
    // Otherwise, throws runtime_error.
    // Context param defines context for __lt__ method executing
    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // Returns opposite to Equal(lhs, rhs, context) result
    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // Returns lhs>rhs result, based on Equal и Less funcs
    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // Returns lhs<=rhs result, based on Equal и Less funcs
    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // Returns opposite to Less(lhs, rhs, context) result
    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // Dummy context for the test executing
    struct DummyContext : Context {
        std::ostream& GetOutputStream() override {
            return output;
        }

        std::ostringstream output;
    };

    // Simple context that does output from the constructor
    class [[maybe_unused]] SimpleContext : public runtime::Context {
    public:
        [[maybe_unused]] explicit SimpleContext(std::ostream& output): output_(output) { }

        std::ostream& GetOutputStream() override {
            return output_;
        }

    private:
        std::ostream& output_;
    };

}  // namespace runtime