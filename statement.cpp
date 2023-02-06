#include "statement.h"

#include <iostream>
#include <utility>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        [[maybe_unused]] const string ADD_METHOD = "__add__"s;
        [[maybe_unused]] const string INIT_METHOD = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        return closure[var_] = std::move(rv_->Execute(closure, context));
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv): var_(std::move(var)), rv_(move(rv)){}

    VariableValue::VariableValue(std::string  var_name) {
        dotted_ids_.push_back(std::move(var_name));
    }

    VariableValue::VariableValue(std::vector<std::string>  dotted_ids): dotted_ids_(std::move(dotted_ids)) {}

    ObjectHolder VariableValue::Execute(Closure& closure, Context& context) {
        Closure* local_closer = &closure;
        auto itr_runner = closure.begin();
        for (const auto& val : dotted_ids_) {
            itr_runner = local_closer->find(val);
            if (itr_runner == local_closer->end()) {
                throw std::runtime_error("runtime_error VariableValue::Execute()");
            }
            auto cls = itr_runner->second.TryAs<runtime::ClassInstance>();
            if (cls != nullptr) {
                local_closer = &cls->Fields();
            }
        }
        return itr_runner->second;
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(std::vector<std::unique_ptr<Statement>> args):args_(std::move(args))  {}

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        bool first_flag = true;
        auto& output = context.GetOutputStream();
        ObjectHolder object_holder;
        for (const auto& arg : args_) {
            if (!first_flag) {
                output << " "s;
            }
            object_holder = arg->Execute(closure, context);
            if (object_holder.operator bool()) {
                object_holder->Print(output, context);
            } else {
                output << "None"s;
            }
            first_flag = false;
        }
        output << "\n"s;
        return object_holder;
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method, std::vector<std::unique_ptr<Statement>> args):
        object_(move(object)), method_(std::move(method)), args_(std::move(args)) {}

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        std::vector<runtime::ObjectHolder> actual_args;
        for (const auto& arg : args_) {
            actual_args.push_back(arg->Execute(closure, context));
        }
        return object_->Execute(closure, context).TryAs< runtime::ClassInstance>()->Call(method_, actual_args, context);
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        auto arg = argument_->Execute(closure, context);
        if (!arg.operator bool()) {return ObjectHolder::Own(runtime::String{ "None"s });}
        std::ostringstream for_print;
        arg->Print(for_print, context);
        return ObjectHolder::Own(runtime::String{ for_print.str() });
        //return argument_->Execute(closure, context);
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        auto obj_lhs = lhs_->Execute(closure, context);
        auto obj_rhs = rhs_->Execute(closure, context);
        if (obj_lhs.TryAs<runtime::Number>() != nullptr && obj_rhs.TryAs<runtime::Number>() != nullptr) {
            return ObjectHolder::Own(runtime::Number(obj_lhs.TryAs<runtime::Number>()->GetValue() + obj_rhs.TryAs<runtime::Number>()->GetValue()));
        }
        if (obj_lhs.TryAs<runtime::String>() != nullptr && obj_rhs.TryAs<runtime::String>() != nullptr) {
            return ObjectHolder::Own(runtime::String(obj_lhs.TryAs<runtime::String>()->GetValue() + obj_rhs.TryAs<runtime::String>()->GetValue()));
        }
        if (lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>() != nullptr) {
            if (lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1)) {
                return lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, { rhs_->Execute(closure, context) }, context);
            }
        }
        throw std::runtime_error("Execute(): ADD --runtime_error"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        auto obj_lhs = lhs_->Execute(closure, context);
        auto obj_rhs = rhs_->Execute(closure, context);
        if (obj_lhs.TryAs<runtime::Number>() != nullptr && obj_rhs.TryAs<runtime::Number>() != nullptr) {
            return ObjectHolder::Own(runtime::Number(obj_lhs.TryAs<runtime::Number>()->GetValue() - obj_rhs.TryAs<runtime::Number>()->GetValue()));
        }
        throw std::runtime_error("Execute(): SUB --runtime_error"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        auto obj_lhs = lhs_->Execute(closure, context);
        auto obj_rhs = rhs_->Execute(closure, context);
        if (obj_lhs.TryAs<runtime::Number>() != nullptr && obj_rhs.TryAs<runtime::Number>() != nullptr) {
            return ObjectHolder::Own(runtime::Number(obj_lhs.TryAs<runtime::Number>()->GetValue() * obj_rhs.TryAs<runtime::Number>()->GetValue()));
        }
        throw std::runtime_error("Execute(): MULT --runtime_error"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        auto obj_lhs = lhs_->Execute(closure, context);
        auto obj_rhs = rhs_->Execute(closure, context);
        if (obj_lhs.TryAs<runtime::Number>() != nullptr && obj_rhs.TryAs<runtime::Number>() != nullptr) {
            if (obj_rhs.TryAs<runtime::Number>() != nullptr){ //??
                return ObjectHolder::Own(runtime::Number(obj_lhs.TryAs<runtime::Number>()->GetValue() / obj_rhs.TryAs<runtime::Number>()->GetValue()));
            } else {
                throw std::runtime_error("Execute(): DIV --runtime_error --division by zero"s);
            }
        }
        throw std::runtime_error("Execute(): DIV --runtime_error"s);
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (const auto& arg : args_) {
            arg->Execute(closure, context);
        }
        return {};
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        throw statement_->Execute(closure, context);
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls): cls_(std::move(cls)) {}

    ObjectHolder ClassDefinition::Execute(Closure& closure, Context& context) {
        closure[cls_.TryAs<runtime::Class>()->GetName()] = std::move(cls_);
        return closure[cls_.TryAs<runtime::Class>()->GetName()];
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv):
    object_(std::move(object)), field_name_(std::move(field_name)), rv_(std::move(rv)){}

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        int a = 1;
        return object_.Execute(closure, context).TryAs<runtime::ClassInstance>()->Fields()[field_name_] = std::move(rv_->Execute(closure, context));
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body, std::unique_ptr<Statement> else_body):
    condition_(std::move(condition)), if_body_(std::move(if_body)), else_body_(std::move(else_body)) {}

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        } else if (else_body_) {
            return else_body_->Execute(closure, context);
        }
        return {};
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue (lhs_->Execute(closure, context)) || runtime::IsTrue (rhs_->Execute(closure, context))){
            return ObjectHolder::Own(runtime::Bool(true));
        }
        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue (lhs_->Execute(closure, context)) && runtime::IsTrue (rhs_->Execute(closure, context))){
            return ObjectHolder::Own(runtime::Bool(true));
        }
        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool(!IsTrue(argument_->Execute(closure, context))));
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs):
    cmp_(std::move(cmp)), BinaryOperation(std::move(lhs), std::move(rhs)) {}

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        bool result = cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context);
        return ObjectHolder::Own(runtime::Bool(result));
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args):
    class_instance_(class_), args_(std::move(args)){}

    NewInstance::NewInstance(const runtime::Class& class_): class_instance_(class_) {}

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        if (class_instance_.HasMethod(INIT_METHOD, args_.size())) {
            std::vector <runtime::ObjectHolder> args;
            for (const auto& arg : args_) {
                args.push_back(arg->Execute(closure, context));
            }
            class_instance_.Call(INIT_METHOD, args, context);
        }
        return runtime::ObjectHolder::Share(class_instance_);
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body): body_(std::move(body)) {}

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_->Execute(closure, context);
        }
        catch (runtime::ObjectHolder obj) {
            return obj;
        }
        return {};
    }

}  // namespace ast