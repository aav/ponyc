#include "pass.h"
#include "../codegen/codegen.h"
#include "parsefix.h"
#include "sugar.h"
#include "scope.h"
#include "names.h"
#include "flatten.h"
#include "traits.h"
#include "expr.h"
#include "../../libponyrt/mem/pool.h"

#include <string.h>
#include <stdbool.h>


bool limit_passes(pass_opt_t* opt, const char* pass)
{
  for(pass_id i = PASS_PARSE; i <= PASS_ALL; i++)
  {
    if(strcmp(pass, pass_name(i)) == 0)
    {
      opt->limit = i;
      return true;
    }
  }

  return false;
}


const char* pass_name(pass_id pass)
{
  switch(pass)
  {
    case PASS_PARSE:    return "parse";
    case PASS_PARSE_FIX:return "parsefix";
    case PASS_SUGAR:    return "sugar";
    case PASS_SCOPE:    return "scope";
    case PASS_NAME_RESOLUTION: return "name";
    case PASS_FLATTEN:  return "flatten";
    case PASS_TRAITS:   return "traits";
    case PASS_EXPR:     return "expr";
    case PASS_AST:      return "ast";
    case PASS_LLVM_IR:  return "ir";
    case PASS_BITCODE:  return "bitcode";
    case PASS_ASM:      return "asm";
    case PASS_OBJ:      return "obj";
    case PASS_ALL:      return "all";
    default:            return "error";
  }
}


void pass_opt_init(pass_opt_t* options)
{
  // Start with an empty typechecker frame.
  memset(options, 0, sizeof(pass_opt_t));
  options->limit = PASS_ALL;
  frame_push(&options->check, NULL);
}


void pass_opt_done(pass_opt_t* options)
{
  // Pop all the typechecker frames.
  while(options->check.frame != NULL)
    frame_pop(&options->check);
}



// Do a single pass, if the limit allows
static bool do_pass(ast_t** astp, bool* out_result, pass_opt_t* options,
  pass_id pass, ast_visit_t pre_fn, ast_visit_t post_fn)
{
  if(options->limit < pass)
  {
    *out_result = true;
    return true;
  }

  if(ast_visit(astp, pre_fn, post_fn, options) != AST_OK)
  {
    *out_result = false;
    return true;
  }

  return false;
}


bool type_passes(ast_t* type, pass_opt_t* options)
{
  ast_t* module = ast_parent(type);
  ast_t* package = ast_parent(module);

  frame_push(&options->check, NULL);
  frame_push(&options->check, package);
  frame_push(&options->check, module);

  bool ok = package_passes(type, options);

  frame_pop(&options->check);
  frame_pop(&options->check);
  frame_pop(&options->check);

  return ok;
}


bool package_passes(ast_t* package, pass_opt_t* options)
{
  if(options->limit == PASS_PARSE)
    return true;

  bool r;

  if(do_pass(&package, &r, options, PASS_PARSE_FIX, pass_parse_fix, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_SUGAR, pass_sugar, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_SCOPE, pass_scope, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_NAME_RESOLUTION, NULL, pass_names))
    return r;

  if(do_pass(&package, &r, options, PASS_FLATTEN, NULL, pass_flatten))
    return r;

  if(do_pass(&package, &r, options, PASS_TRAITS, pass_traits, NULL))
    return r;

  if(do_pass(&package, &r, options, PASS_EXPR, NULL, pass_expr))
    return r;

  return true;
}


bool program_passes(ast_t* program, pass_opt_t* opt)
{
  if(opt->limit < PASS_LLVM_IR)
    return true;

  return codegen(program, opt);
}
