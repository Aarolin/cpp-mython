#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_.get()->Execute(closure, context);
    return closure[var_];
}

Assignment::Assignment(string var, unique_ptr<Statement> rv) : var_(move(var)), rv_(move(rv)) {
}

VariableValue::VariableValue(const string& var_name) : var_name_(var_name) {
}

VariableValue::VariableValue(vector<string> dotted_ids) : dotted_ids_(move(dotted_ids)) {
}

ObjectHolder VariableValue::Execute(Closure& closure, Context&) {
    
    if (!dotted_ids_.empty()) {

        Closure closure_for_search = closure;
        ObjectHolder result;

        for (const string& id : dotted_ids_) {
            if (closure_for_search.count(id) == 0) {
                throw std::runtime_error("Unable to evaluate a variable with the given name"s);
            }
            ObjectHolder& field = closure_for_search.at(id);
            result = field;
            runtime::ClassInstance* field_value_ptr = field.TryAs<runtime::ClassInstance>();
            if (!field_value_ptr) {
                break;
            }
            closure_for_search = field_value_ptr->Fields();
            
        }
        return result;
    }

    std::string var_name = std::string{ var_name_ };
    
    if (closure.count(var_name) == 0) {
        throw std::runtime_error("Unable to evaluate a variable with the given name"s);
    }

    return closure.at(var_name);
}

unique_ptr<Print> Print::Variable(const string& name) {
    return std::make_unique<Print>(std::make_unique<StringConst>(name));
}

Print::Print(unique_ptr<Statement> argument) : argument_(move(argument)) {    
}

Print::Print(vector<unique_ptr<Statement>> args) : args_(move(args)) {
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {

    std::ostream& os = context.GetOutputStream();

    if (!args_.empty()) {
        PrintArgs(os, closure, context);
        os << std::endl;
        return {};
    }

    PrintArgument(os, argument_, closure, context);
    os << std::endl;
    return {};
}

void Print::PrintArgument(ostream& os, const unique_ptr<Statement>& arg, Closure& closure, Context& context) const {
    if (arg.get()) {
        auto arg_obj = arg->Execute(closure, context);
        PrintObj(os, arg_obj, closure, context);
    }
}


void Print::PrintArgs(std::ostream& os, Closure& closure, Context& context) const {

    bool first_arg = true;

    for (const auto& arg : args_) {
        if (first_arg) {
            first_arg = false;
            PrintArgument(os, arg, closure, context);
            continue;
        }
        os << ' ';
        PrintArgument(os, arg, closure, context);
    }
    return;
}

void Print::PrintObj(ostream& os, ObjectHolder obj, Closure& closure, Context& context) const {

    if (runtime::String* rv = obj.TryAs<runtime::String>(); rv) {
        const string& rv_value = rv->GetValue();
        if (closure.count(rv_value) != 0) {
            const auto& value_obj = closure.at(rv_value);
            PrintObj(os, value_obj, closure, context);
            return;
        }
        os << rv->GetValue();
    }
    else if (runtime::Number* rv = obj.TryAs<runtime::Number>(); rv) {
        os << rv->GetValue();
    }
    else if (runtime::Bool* rv = obj.TryAs<runtime::Bool>(); rv) {
        bool value = rv->GetValue();
        if (value) {
            os << "True"s;
        }
        else {
            os << "False"s;
        }
    }
    else if (runtime::ClassInstance* rv = obj.TryAs<runtime::ClassInstance>(); rv) {
        rv->Print(os, context);
    }
    else {
        os << "None"s;
    }
    return;

}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args) : object_(move(object)), method_(move(method)), args_(move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {

    auto inst_holder = object_.get()->Execute(closure, context);
    auto inst_ptr = inst_holder.TryAs<runtime::ClassInstance>();
    if (inst_ptr) {
        vector<ObjectHolder> args_holders;
        args_holders.reserve(args_.size());
        for (const auto& arg : args_) {
            auto arg_holder = arg.get()->Execute(closure, context);
            args_holders.push_back(move(arg_holder));
        }
        try {
            auto result = inst_ptr->Call(method_, args_holders, context);
            return result;
        }
        catch (...) {

        }
        
    }

    return {};
}


ObjectHolder Stringify::Execute(Closure& closure, Context& context) {

    auto arg_obj = arg_.get()->Execute(closure, context);

    if (runtime::String* var = arg_obj.TryAs<runtime::String>(); var) {
        std::string value = var->GetValue();
        return ObjectHolder::Own(runtime::String(move(value)));
    }
    else if (runtime::Number* var = arg_obj.TryAs<runtime::Number>(); var) {
        int value = var->GetValue();
        ObjectHolder result = ObjectHolder::Own(runtime::String(to_string(value)));
        return ObjectHolder::Own(runtime::String(to_string(value)));
    }
    else if (runtime::Bool* var = arg_obj.TryAs<runtime::Bool>(); var) {
        bool value = var->GetValue();
        if (value) {
            return ObjectHolder::Own(runtime::String("True"s));
        }
        return ObjectHolder::Own(runtime::String("False"s));;
    }
    else if (runtime::ClassInstance* var = arg_obj.TryAs<runtime::ClassInstance>(); var) {
        runtime::DummyContext context;
        try {
            auto result_of_str = var->Call("__str__"s, {}, context);
            return ConverResultStrToObjectHolder(result_of_str);
        }
        catch (...) {

        }
        std::ostringstream os;
        os << var;
        std::string class_ptr = os.str();

        return ObjectHolder::Own(runtime::String(move(class_ptr)));
    }

    return ObjectHolder::Own(runtime::String("None"s));
}

ObjectHolder Stringify::ConverResultStrToObjectHolder(ObjectHolder& result_str) {

    if (runtime::String* var = result_str.TryAs<runtime::String>(); var) {
        std::string value = var->GetValue();
        return ObjectHolder::Own(runtime::String(move(value)));
    }
    else if (runtime::Number* var = result_str.TryAs<runtime::Number>(); var) {
        int value = var->GetValue();
        return ObjectHolder::Own(runtime::String(to_string(value)));
    }
    else if (runtime::Bool* var = result_str.TryAs<runtime::Bool>(); var) {
        bool value = var->GetValue();
        if (value) {
            return ObjectHolder::Own(runtime::String("True"s));
        }
        return ObjectHolder::Own(runtime::String("False"s));;
    }
    else if (runtime::ClassInstance* var = result_str.TryAs<runtime::ClassInstance>(); var) {
        runtime::DummyContext context;
        try {
            auto result_of_str = var->Call("__str__"s, {}, context);
            return ConverResultStrToObjectHolder(result_of_str);
        }
        catch (...) {

        }
        std::ostringstream os;
        os << var;
        std::string class_ptr = os.str();
        return ObjectHolder::Own(runtime::String(move(class_ptr)));
    }

    return ObjectHolder::Own(runtime::String("None"s));
}


ObjectHolder Add::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);

    ValueType lhs_type = GetValueTypeOfObjHolder(lhs_holder);
    ValueType rhs_type = GetValueTypeOfObjHolder(rhs_holder);

    if (lhs_type == ValueType::Number && rhs_type == ValueType::Number) {
        auto lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        int lhs_value = lhs_ptr->GetValue();
        auto rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        int rhs_value = rhs_ptr->GetValue();
        int result_sum = lhs_value + rhs_value;
        return ObjectHolder::Own(runtime::Number(result_sum));
    }
    else if (lhs_type == ValueType::String && rhs_type == ValueType::String) {
        auto lhs_ptr = lhs_holder.TryAs<runtime::String>();
        string lhs_value = lhs_ptr->GetValue();
        auto rhs_ptr = rhs_holder.TryAs<runtime::String>();
        string rhs_value = rhs_ptr->GetValue();
        string result_str = lhs_value + rhs_value;
        return ObjectHolder::Own(runtime::String(result_str));

    }
    else if (lhs_type == ValueType::ClassInstance) {
        auto lhs_ptr = lhs_holder.TryAs<runtime::ClassInstance>();
        if (lhs_ptr->HasMethod(ADD_METHOD, 1)) {
            return lhs_ptr->Call(ADD_METHOD, { rhs_holder }, context);
        }
        throw std::runtime_error("lhs does not have method __add__"s);
    }

    throw std::runtime_error("Can't add arguments with given types"s);

    return {};
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {

    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);

    ValueType lhs_type = GetValueTypeOfObjHolder(lhs_holder);
    ValueType rhs_type = GetValueTypeOfObjHolder(rhs_holder);

    if (lhs_type == ValueType::Number && rhs_type == ValueType::Number) {
        auto lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        int lhs_value = lhs_ptr->GetValue();
        auto rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        int rhs_value = rhs_ptr->GetValue();
        int result = lhs_value - rhs_value;
        return ObjectHolder::Own(runtime::Number(result));
    }

    throw std::runtime_error("Can't sub given types"s);
    return {};
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);

    ValueType lhs_type = GetValueTypeOfObjHolder(lhs_holder);
    ValueType rhs_type = GetValueTypeOfObjHolder(rhs_holder);

    if (lhs_type == ValueType::Number && rhs_type == ValueType::Number) {
        auto lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        int lhs_value = lhs_ptr->GetValue();
        auto rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        int rhs_value = rhs_ptr->GetValue();
        int result = lhs_value * rhs_value;
        return ObjectHolder::Own(runtime::Number(result));
    }

    throw std::runtime_error("Can't multiply given types"s);

    return {};
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);

    ValueType lhs_type = GetValueTypeOfObjHolder(lhs_holder);
    ValueType rhs_type = GetValueTypeOfObjHolder(rhs_holder);

    if (lhs_type == ValueType::Number && rhs_type == ValueType::Number) {
        auto lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        int lhs_value = lhs_ptr->GetValue();
        auto rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        int rhs_value = rhs_ptr->GetValue();
        if (rhs_value == 0) {
            throw std::runtime_error("Can't divide by 0"s);
        }
        int result = lhs_value / rhs_value;
        return ObjectHolder::Own(runtime::Number(result));
    }

    throw std::runtime_error("Can't divide given types"s);

    return {};
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {

    for (const auto& command : commands_) {
        command.get()->Execute(closure, context);
    }

    return {};
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw return_except(statement_.get()->Execute(closure, context));
    return {};
}

ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(move(cls)) {

}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context&) {
    auto cls_ptr = cls_.TryAs<runtime::Class>();
    auto cls_name = cls_ptr->GetName();
    closure.insert({cls_name, cls_});
    return cls_;
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv) : object_(std::move(object)), assign_var_(move(field_name), move(rv)) {

}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    auto rv = assign_var_.Execute(closure, context);
    auto obj_holder = object_.Execute(closure, context);
    auto instance = obj_holder.TryAs<runtime::ClassInstance>();
    instance->Fields()[assign_var_.GetVarName()] = rv;
    return rv;
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body) : condition_(move(condition)), if_body_(move(if_body)), else_body_(move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {

    auto condit_obj = condition_.get()->Execute(closure, context);

    if (runtime::IsTrue(condit_obj)) {
        return if_body_.get()->Execute(closure, context);
    }
    if (else_body_.get()) {
        return else_body_.get()->Execute(closure, context);
    }
    return {};
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {

    auto lhs_holder = lhs_.get()->Execute(closure, context);
    bool lhs_result = runtime::IsTrue(lhs_holder);

    if (!lhs_result) {
        auto rhs_holder = rhs_.get()->Execute(closure, context);
        bool rhs_result = runtime::IsTrue(rhs_holder);
        return ObjectHolder::Own(runtime::Bool(rhs_result));
    }

    return ObjectHolder::Own(runtime::Bool(lhs_result));
}

ObjectHolder And::Execute(Closure& closure, Context& context) {

    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);
    bool lhs_result = runtime::IsTrue(lhs_holder);
    bool rhs_result = runtime::IsTrue(rhs_holder);

    return ObjectHolder::Own(runtime::Bool(lhs_result && rhs_result));
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    auto arg_holder = arg_.get()->Execute(closure, context);
    return ObjectHolder::Own(runtime::Bool(!runtime::IsTrue(arg_holder)));
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(move(cmp)) {
    
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);
    return ObjectHolder::Own(runtime::Bool(cmp_(lhs_holder, rhs_holder, context)));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) : inst_(class_), args_(move(args)) {
}

NewInstance::NewInstance(const runtime::Class& class_) : inst_(class_) {
    
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (inst_.HasMethod(INIT_METHOD, args_.size())) {
        vector<ObjectHolder> arg_holders;
        arg_holders.reserve(args_.size());
        for (const auto& arg : args_) {
            auto arg_holder = arg.get()->Execute(closure, context);
            arg_holders.push_back(arg_holder);
        }
        inst_.Call(INIT_METHOD, arg_holders, context);
    }
    
    return ObjectHolder::Share(inst_);

}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(move(body)) {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {

    try {
        body_.get()->Execute(closure, context);
    }
    catch (return_except& except) {
        return except.GetInfo();
    }

    return {};
}

ValueType GetValueTypeOfObjHolder(ObjectHolder& holder) {

    if (runtime::String* var = holder.TryAs<runtime::String>(); var) {
        return ValueType::String;
    }
    else if (runtime::Number* var = holder.TryAs<runtime::Number>(); var) {
        return ValueType::Number;
    }
    else if (runtime::Bool* var = holder.TryAs<runtime::Bool>(); var) {
        return ValueType::Bool;
    }
    else if (runtime::ClassInstance* var = holder.TryAs<runtime::ClassInstance>(); var) {
        return ValueType::ClassInstance;
    }
    return ValueType::None;
}

}  // namespace ast