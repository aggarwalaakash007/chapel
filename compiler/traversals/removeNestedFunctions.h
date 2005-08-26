#ifndef _REMOVE_NESTED_FUNCTIONS_
#define _REMOVE_NESTED_FUNCTIONS_

#include "traversal.h"
#include "stmt.h"
#include "expr.h"
#include "symbol.h"
#include "baseAST.h"

class RemoveNestedFunctions : public Traversal {
  Map<FnSymbol*,Vec<Symbol*>*>* _nested_func_args_map;
  Map<BaseAST*,BaseAST*>* _nested_func_sym_map;
  Vec<FnSymbol*>* _fn_call_worklist;
  Vec<Stmt*>* _fn_stmts_completed_so_far;
  

public :
  RemoveNestedFunctions();
  void postProcessStmt(Stmt* stmt);
  void postProcessExpr(Expr* expr);
  static FnSymbol* hasEnclosingFunction(DefExpr* fn_def);
  static Vec<Symbol*>* getEnclosingFuncVarUses(FnSymbol* fn_sym, Map<FnSymbol*,Vec<Symbol*>*>* nested_func_args_map, 
                                               Vec<FnSymbol*>* in_process_fns);
  static void findEnclScopeVarUses(FnSymbol* fn_sym, Map<FnSymbol*,Vec<Symbol*>*>* nested_func_args_map, 
                                   Vec<FnSymbol*>* in_process_fns);
  void addNestedFuncFormals(Expr* expr, Vec<Symbol*>* encl_var_uses, FnSymbol* old_func_sym);
  void addNestedFuncActuals(CallExpr* paren_op, Vec<Symbol*>* encl_var_uses, FnSymbol* old_func_sym);
  void enclVarUsesHelper();
 
};

#endif
