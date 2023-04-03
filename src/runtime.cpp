#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
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
        // Заглушка. Реализуйте метод самостоятельно
        using namespace std::literals;
        if (!object) {
            return false;
        }

        String* str_ptr = object.TryAs<String>();
        if (str_ptr) {
            return !str_ptr->GetValue().empty();
        }

        Number* num_ptr = object.TryAs<Number>();
        if (num_ptr) {
            return num_ptr->GetValue();
        }
        
        Bool* bool_ptr = object.TryAs<Bool>();
        if (bool_ptr) {
            return bool_ptr->GetValue();
        }

        return false;
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {

        if (HasMethod("__str__"s, 0)) {
            ObjectHolder str_res = Call("__str__"s, {}, context);
            if (String* str_ptr = str_res.TryAs<String>(); str_ptr) {
                os << str_ptr->GetValue();
                return;
            }
            else if (Number* num_ptr = str_res.TryAs<Number>(); num_ptr) {
                os << num_ptr->GetValue();
                return;
            }
            else if (Bool* bool_ptr = str_res.TryAs<Bool>(); bool_ptr) {
                os << bool_ptr->GetValue();
                return;
            }
        }
        os << this;
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {

        if (!cls_) {
            return false;
        }

        const Method* searched_method = cls_->GetMethod(method);

        if (searched_method == nullptr) {
            return false;
        }

        return searched_method->formal_params.size() == argument_count;
    }

    Closure& ClassInstance::Fields() {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class& cls) : cls_(&cls){

    }

    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args,
        Context& context) {

        if (HasMethod(method, actual_args.size())) {
            const Method* method_for_call = cls_->GetMethod(method);
            Closure method_closure;
            const auto& args = method_for_call->formal_params;

            for (size_t i = 0; i < actual_args.size(); ++i) {
                method_closure[args[i]] = actual_args[i];
            }
            method_closure["self"s] = ObjectHolder::Share(*this);
            auto result = method_for_call->body->Execute(method_closure, context);
            return result;
        }
        throw std::runtime_error("Incorrect call"s);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent) : name_(std::move(name)), methods_(std::move(methods)), parent_(parent) {
    }

    const Method* Class::GetMethod(const std::string& name) const {

        for (const Method& method : methods_) {
            if (method.name == name) {
                return &method;
            }
        }

        if (parent_) {
            for (const Method& method : parent_->methods_) {
                if (method.name == name) {
                    return &method;
                }
            }
        }
        
        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, Context& context) {
        if (!os) {
            auto& output = context.GetOutputStream();
            output << "Class " << name_;
            return;
        }
        os << "Class " << name_;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

        if (!lhs && !rhs) {
            return true;
        }

        if (!lhs || !rhs) {
            throw std::runtime_error("Cannot compare objects for equality"s);
        }

        String* lhs_ptr_str = lhs.TryAs<String>();
        String* rhs_ptr_str = rhs.TryAs<String>();
        if (lhs_ptr_str && rhs_ptr_str) {
            return lhs_ptr_str->GetValue() == rhs_ptr_str->GetValue();
        }

        Number* lhs_ptr_num = lhs.TryAs<Number>();
        Number* rhs_ptr_num = rhs.TryAs<Number>();
        if (lhs_ptr_num && rhs_ptr_num) {
            return lhs_ptr_num->GetValue() == rhs_ptr_num->GetValue();
        }

        Bool* lhs_ptr_bool = lhs.TryAs<Bool>();
        Bool* rhs_ptr_bool = rhs.TryAs<Bool>();
        if (lhs_ptr_bool && rhs_ptr_bool) {
            return lhs_ptr_bool->GetValue() == rhs_ptr_bool->GetValue();
        }

        ClassInstance* lhs_instance = lhs.TryAs<ClassInstance>();
        ClassInstance* rhs_instance = rhs.TryAs<ClassInstance>();

        if (!lhs_instance || !rhs_instance) {
            throw std::runtime_error("Cannot compare objects for equality"s);
        }

        if (lhs_instance->HasMethod("__eq__"s, 1)) {
            return IsTrue(lhs_instance->Call("__eq__"s, { ObjectHolder::Share(*rhs_instance) }, context));
        }

        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

        if (!lhs || !rhs) {
            throw std::runtime_error("Cannot compare objects for less"s);
        }

        String* lhs_ptr_str = lhs.TryAs<String>();
        String* rhs_ptr_str = rhs.TryAs<String>();
        if (lhs_ptr_str && rhs_ptr_str) {
            return lhs_ptr_str->GetValue() < rhs_ptr_str->GetValue();
        }

        Number* lhs_ptr_num = lhs.TryAs<Number>();
        Number* rhs_ptr_num = rhs.TryAs<Number>();
        if (lhs_ptr_num && rhs_ptr_num) {
            return lhs_ptr_num->GetValue() < rhs_ptr_num->GetValue();
        }

        Bool* lhs_ptr_bool = lhs.TryAs<Bool>();
        Bool* rhs_ptr_bool = rhs.TryAs<Bool>();
        if (lhs_ptr_bool && rhs_ptr_bool) {
            return lhs_ptr_bool->GetValue() < rhs_ptr_bool->GetValue();
        }

        
        ClassInstance* lhs_instance = lhs.TryAs<ClassInstance>();
        ClassInstance* rhs_instance = rhs.TryAs<ClassInstance>();

        if (!lhs_instance || !rhs_instance) {
            throw std::runtime_error("Cannot compare objects for equality"s);
        }

        if (lhs_instance->HasMethod("__lt__"s, 1)) {
            return IsTrue(lhs_instance->Call("__lt__"s, { ObjectHolder::Share(*rhs_instance) }, context));
        }

        throw std::runtime_error("Cannot compare objects for less"s);
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