#include "removeNestedFunctions.h"
#include "symscope.h"
#include "symtab.h"
#include "findEnclosingScopeVarUses.h"
#include "alist.h"


RemoveNestedFunctions::RemoveNestedFunctions() {
  _nested_func_args_map = new Map<FnSymbol*,Vec<Symbol*>*>();
  _nested_func_sym_map = new Map<BaseAST*, BaseAST*>();
  _fn_call_worklist = new Vec<FnSymbol*>();
  _fn_stmts_completed_so_far = new Vec<Stmt*>();
 
  enclVarUsesHelper(); 
}

void RemoveNestedFunctions::postProcessStmt(Stmt* stmt) {
  if (ExprStmt* expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
    if (DefExpr* defExpr = dynamic_cast<DefExpr*>(expr_stmt->expr)) {
      //function definition
      if (FnSymbol* fn_sym = dynamic_cast<FnSymbol*>(defExpr->sym)){
        //nested function definition
        if(hasEnclosingFunction(defExpr)) {
          Vec<Symbol*>* encl_func_var_uses = _nested_func_args_map->get(fn_sym);

          //add to global scope
          ModuleSymbol* curr_module = fn_sym->argScope->getModule();
          AList<Stmt>* module_stmts = curr_module->stmts;
          ExprStmt* fn_copy = expr_stmt->copy(true);
          //add formal arguments to copied nested function
          addNestedFuncFormals(fn_copy->expr, encl_func_var_uses, fn_sym);
          module_stmts->insertAtTail(fn_copy);
          _fn_stmts_completed_so_far->add_exclusive(fn_copy);
          //update fn calls symbols seen before new function def was created
          if (_fn_call_worklist->in(fn_sym)) {
            forv_Vec(Stmt, stmt, *_fn_stmts_completed_so_far) {
              stmt->traverse(new UpdateSymbols(_nested_func_sym_map, NULL));
            }
          }
          expr_stmt->remove();
        }
      }
    } 
  }
}

void RemoveNestedFunctions::postProcessExpr(Expr* expr) {
  if (CallExpr* paren_op = dynamic_cast<CallExpr*>(expr)) {
    if (FnSymbol* fn_sym = paren_op->findFnSymbol()) {
      Vec<Symbol*>* encl_func_var_uses = _nested_func_args_map->get(fn_sym);
      //nested function call
      if (encl_func_var_uses)
        addNestedFuncActuals(paren_op, encl_func_var_uses, fn_sym);
    }
  } 
}

void RemoveNestedFunctions::findEnclScopeVarUses(FnSymbol* fn_sym, Map<FnSymbol*,Vec<Symbol*>*>* nested_func_args_map, Vec<FnSymbol*>* in_process_fns) {
  Vec<Symbol*>* encl_func_var_uses = nested_func_args_map->get(fn_sym);
  in_process_fns->add(fn_sym);
  encl_func_var_uses = getEnclosingFuncVarUses(fn_sym, nested_func_args_map,in_process_fns);
  //store nested function actual arg info
  nested_func_args_map->put(fn_sym, encl_func_var_uses);
  in_process_fns->pop();
}

FnSymbol* RemoveNestedFunctions::hasEnclosingFunction(DefExpr* fn_def) {
    return (dynamic_cast<FnSymbol*>(fn_def->parentSymbol));
  return NULL;
}

Vec<Symbol*>* RemoveNestedFunctions::getEnclosingFuncVarUses(FnSymbol* fn_sym, Map<FnSymbol*,Vec<Symbol*>*>* nested_func_args_map, Vec<FnSymbol*>* in_process_fns) {
  FindEnclosingScopeVarUses* fesv = new FindEnclosingScopeVarUses(fn_sym->parentScope, nested_func_args_map, in_process_fns);
  fn_sym->defPoint->parentStmt->traverse(fesv);
  return fesv->getVarUses();
}

void RemoveNestedFunctions::addNestedFuncFormals(Expr* expr, Vec<Symbol*>* encl_var_uses, FnSymbol* old_func_sym) {
  if (DefExpr* defExpr = dynamic_cast<DefExpr*>(expr)) {
    if (FnSymbol* fn_sym = dynamic_cast<FnSymbol*>(defExpr->sym)) {
      //update old nested function symbol to new function symbol map
      _nested_func_sym_map->put(old_func_sym, fn_sym);
      
      Map<BaseAST*,BaseAST*>* update_map = new Map<BaseAST*,BaseAST*>();
      forv_Vec(Symbol, sym, *encl_var_uses) {
        if (sym) {
          //create formal and add to nested function
          DefExpr* formal = Symboltable::defineParam(INTENT_INOUT,sym->name,NULL,NULL);
          formal->sym->type = sym->type;
          fn_sym->formals->insertAtTail(formal);
          //will need to perform an update to map enclosing variables to formals
          update_map->put(sym, formal->sym);
        }
      }
      
      //not empty, added formals to nested function definition, perform symbol mapping
      if (encl_var_uses->n)
        fn_sym->body->traverse(new UpdateSymbols(update_map, NULL));
    }
  } 
}

void RemoveNestedFunctions::addNestedFuncActuals(CallExpr* paren_op, Vec<Symbol*>* encl_var_uses, FnSymbol* old_func_sym) {
  //replace original nested function call with call to non-nested function
  FnSymbol* new_func_sym = dynamic_cast<FnSymbol*>(_nested_func_sym_map->get(old_func_sym));

  //build nested function actuals list
  forv_Vec(Symbol, sym, *encl_var_uses) {
    if (sym) 
      paren_op->argList->insertAtTail(new SymExpr(sym));
  }
  
  //already moved the nested function out
  if (new_func_sym) {
    paren_op->baseExpr->replace(new SymExpr(new_func_sym));
  }
  //haven't created new function definition to replace nested function call yet
  else {
    _fn_call_worklist->add_exclusive(old_func_sym);
  }
}

void RemoveNestedFunctions::enclVarUsesHelper() {
  Vec<FnSymbol*>* all_functions = new Vec<FnSymbol*>();
  collect_functions(all_functions);
  //find all nested functions
  Vec<FnSymbol*>* all_nested_functions = new Vec<FnSymbol*>();
  forv_Vec(FnSymbol, fn_sym, *all_functions) {
    if (hasEnclosingFunction(fn_sym->defPoint)) {
      //printf("nested function: %s\n", fn_sym->name);
      all_nested_functions->add_exclusive(fn_sym);
    }
  }
  
  //build initial enclosing var use nested function map
  forv_Vec(FnSymbol, fn_sym, *all_nested_functions) {
    _nested_func_args_map->put(fn_sym, new Vec<Symbol*>());
  } 
  bool change = false;
  Map<FnSymbol*,Vec<Symbol*>*>* encl_var_use_map = new Map<FnSymbol*,Vec<Symbol*>*>();
  do {
    change = false;
    //build enclosing var use map for nested functions, iteratively!!!
    forv_Vec(FnSymbol, fn_sym, *all_nested_functions) { 
      findEnclScopeVarUses(fn_sym, encl_var_use_map, new Vec<FnSymbol*>());
    }  
    //compare for changes
    forv_Vec(FnSymbol, fn_sym, *all_nested_functions) {
      Vec<Symbol*>* iteration_encl_var_vec = encl_var_use_map->get(fn_sym);
      Vec<Symbol*>* global_encl_var_vec = _nested_func_args_map->get(fn_sym);
      //found change, must iterate again
      if (global_encl_var_vec->length() != iteration_encl_var_vec->length()) {
        forv_Vec(Symbol, sym, *iteration_encl_var_vec) {
          //add changes
          global_encl_var_vec->add_exclusive(sym);
        }
        change = true;
      }
    }
  } while(change);
}
