#include "goalc/compiler/Compiler.h"

using namespace goos;

bool Compiler::try_getting_macro_from_goos(const goos::Object& macro_name, goos::Object* dest) {
  Object macro_obj;
  bool got_macro = false;
  try {
    macro_obj = m_goos.eval_symbol(macro_name, m_goos.goal_env.as_env());
    if (macro_obj.is_macro()) {
      got_macro = true;
    }
  } catch (std::runtime_error& e) {
    got_macro = false;
  }

  if (got_macro) {
    *dest = macro_obj;
  }
  return got_macro;
}

Val* Compiler::compile_goos_macro(const goos::Object& o,
                                  const goos::Object& macro_obj,
                                  const goos::Object& rest,
                                  Env* env) {
  auto macro = macro_obj.as_macro();
  Arguments args = m_goos.get_args(o, rest, macro->args);
  auto mac_env_obj = EnvironmentObject::make_new();
  auto mac_env = mac_env_obj.as_env();
  m_goos.set_args_in_env(o, args, macro->args, mac_env);
  m_goos.goal_to_goos.enclosing_method_type =
      get_parent_env_of_type<FunctionEnv>(env)->method_of_type_name;
  auto goos_result = m_goos.eval_list_return_last(macro->body, macro->body, mac_env);
  m_goos.goal_to_goos.reset();
  return compile_error_guard(goos_result, env);
}

Val* Compiler::compile_gscond(const goos::Object& form, const goos::Object& rest, Env* env) {
  if (!rest.is_pair()) {
    throw_compile_error(form, "#cond must have at least one clause, which must be a form");
  }
  Val* result = nullptr;

  Object lst = rest;
  for (;;) {
    if (lst.is_pair()) {
      Object current_case = lst.as_pair()->car;
      if (!current_case.is_pair()) {
        throw_compile_error(lst, "Bad case in #cond");
      }

      // check condition:
      Object condition_result =
          m_goos.eval_with_rewind(current_case.as_pair()->car, m_goos.global_environment.as_env());
      if (m_goos.truthy(condition_result)) {
        if (current_case.as_pair()->cdr.is_empty_list()) {
          return get_none();
        }
        // got a match!
        result = get_none();

        for_each_in_list(current_case.as_pair()->cdr,
                         [&](Object o) { result = compile_error_guard(o, env); });
        return result;
      } else {
        // no match, continue.
        lst = lst.as_pair()->cdr;
      }
    } else if (lst.is_empty_list()) {
      return get_none();
    } else {
      throw_compile_error(form, "malformed #cond");
    }
  }
}

Val* Compiler::compile_quote(const goos::Object& form, const goos::Object& rest, Env* env) {
  auto args = get_va(form, rest);
  va_check(form, args, {{}}, {});
  auto thing = args.unnamed.at(0);
  switch (thing.type) {
    case goos::ObjectType::SYMBOL:
      return compile_get_sym_obj(thing.as_symbol()->name, env);
    default:
      throw_compile_error(form, "Can't quote this");
  }
  return get_none();
}