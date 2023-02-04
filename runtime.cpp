#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <utility>

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data): data_(std::move(data)) { }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Return shared_ptr (deleter does nothing)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return {};
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (const auto* ptr_as_number = object.TryAs<Number>()) {
            return ptr_as_number->GetValue() != 0;
        } else if (const auto* ptr_as_string = object.TryAs<String>()) {
            return !ptr_as_string->GetValue().empty();
        } else if (const auto* ptr_as_bool = object.TryAs<Bool>()) {
            return ptr_as_bool->GetValue();
        } else {
            return false;
        }
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (HasMethod("__str__", 0)) {
            Call("__str__", {}, context).Get()->Print(os, context);
        } else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        if (cls_.GetMethod(method) != nullptr) {
            if (cls_.GetMethod(method)->formal_params.size() == argument_count) {
                return true;
            }
        }
        return false;
    }

    Closure& ClassInstance::Fields() {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class&  cls): cls_(cls){}

    ObjectHolder ClassInstance::Call(const std::string& method, const std::vector<ObjectHolder>& actual_args, Context& context) {
        if (HasMethod(method, actual_args.size())) {
            Closure args;
            args["self"s] = ObjectHolder::Share(*this);
            const Method* method_ptr = cls_.GetMethod(method);
            for (size_t i = 0; i < actual_args.size(); ++i) {
                args[method_ptr->formal_params[i]] = actual_args[i];
            }
            return method_ptr->body->Execute(args, context);
        }
        throw std::runtime_error("Not implemented"s);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent):
    name_(std::move(name)), methods_(std::move(methods)){
        if (parent) {
            parent_ = parent;
        } else {
            parent_ = this;
        }
    }

    const Method* Class::GetMethod(const std::string& name) const {
        for (const auto & method : methods_){
            if (method.name == name) {
                return &method;
            }
        }
        if (this->parent_ != this){
            return parent_->GetMethod(name);
        } else {
            return nullptr;
        }
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        if (name_.empty()){
            throw std::runtime_error("Not implemented"s);
        }
        return name_;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class " << GetName();
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (lhs.TryAs<Number>() && rhs.TryAs<Number>()){
            return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
        } else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
            return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
        } else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()){
            return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
        } else if (!lhs && !rhs) {
            return true;
        } else if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod("__eq__"s, 1)) {
            return lhs.TryAs<ClassInstance>()->Call("__eq__"s, {rhs}, context).TryAs<Bool>()->GetValue();
        }
        throw std::runtime_error("Cannot compare objects for equality"s);

    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (lhs.TryAs<Number>() && rhs.TryAs<Number>()){
            return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
        } else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
            return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
        } else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()){
            return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
        } else if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod("__lt__"s, 1)) {
            return lhs.TryAs<ClassInstance>()->Call("__lt__"s, {rhs}, context).TryAs<Bool>()->GetValue();
        }
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Greater(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime