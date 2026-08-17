/*
 * Copyright (c) 2026 OceanBase.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"
#if CPP_STANDARD == 11
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#elif CPP_STANDARD == 20
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#endif

using namespace std;
using namespace llvm;

namespace {
  llvm::Function *get_abort_func(llvm::Module *M)
  {
    const char *func_name = "memory_sanity_abort";
    llvm::Function *func = M->getFunction(func_name);
    if (func == nullptr)
    {
      llvm::FunctionType *func_type = llvm::FunctionType::get(Type::getVoidTy(M->getContext()), {}, false);
      func = llvm::Function::Create(func_type, llvm::GlobalValue::ExternalLinkage, func_name, M);
    }
    return func;
  }

  AllocaInst *alloa_gv_copy_for_int64(Module *M, Function *F, const char *gv_name)
  {
    AllocaInst *gv_copy = nullptr;
    GlobalVariable *gv = M->getGlobalVariable(gv_name, true);
    if (!gv) {
      gv = new GlobalVariable(
          *M,
          Type::getInt64Ty(M->getContext()),
          false,
          GlobalValue::ExternalLinkage,
          nullptr,
          gv_name);
    }
    BasicBlock &entry_block = F->getEntryBlock();
    IRBuilder<> builder(&entry_block, entry_block.begin());
    gv_copy = builder.CreateAlloca(Type::getInt64Ty(M->getContext()), nullptr, gv_name);
    builder.CreateStore(builder.CreateLoad(gv->getValueType(), gv), gv_copy);
    return gv_copy;
  }

  bool is_stack_pointer(Value *v)
  {
    if (isa<AllocaInst>(v)) {
      return true;
    }
    if (auto *I = dyn_cast<BitCastInst>(v)) {
      return is_stack_pointer(I->getOperand(0));
    } else if (auto *I = dyn_cast<PtrToIntInst>(v)) {
      return is_stack_pointer(I->getOperand(0));
    } else if (auto *I= dyn_cast<GetElementPtrInst>(v)) {
      return is_stack_pointer(I->getPointerOperand());
    }
    return false;
  }

  bool is_global_pointer(Value *v)
  {
    v = v->stripPointerCasts();
    if (isa<GlobalValue>(v)) {
      return true;
    }
    if (auto *I = dyn_cast<BitCastInst>(v)) {
      return is_global_pointer(I->getOperand(0));
    } else if (auto *I = dyn_cast<PtrToIntInst>(v)) {
      return is_global_pointer(I->getOperand(0));
    } else if (auto *I = dyn_cast<GetElementPtrInst>(v)) {
      return is_global_pointer(I->getPointerOperand());
    } else if (auto *I = dyn_cast<SelectInst>(v)) {
      return (is_global_pointer(I->getTrueValue()) && is_global_pointer(I->getFalseValue()));
    }
    return false;
  }

  /* LoadInst or StoreInst */
  template<typename T>
  void inject_checker(T *I,
                      Type *access_type,
                      Module *M,
                      AllocaInst *min_addr,
                      AllocaInst *max_addr,
                      Function *abort_func)
  {
    auto &ctx = M->getContext();
    // get opration pointer
    Value *op_ptr = I->getPointerOperand();
    // get operation size
    int64_t op_size = M->getDataLayout().getTypeStoreSize(access_type);

    /* check in range */
    IRBuilder<> builder(ctx);
    builder.SetInsertPoint(I);
    auto int64_type = Type::getInt64Ty(ctx);
    Value *op_addr = builder.CreatePointerCast(op_ptr, int64_type);
    Value *lower_bound = builder.CreateLoad(min_addr->getAllocatedType(), min_addr);
    Value *upper_bound = builder.CreateLoad(max_addr->getAllocatedType(), max_addr);
    Value *lower_cmp = builder.CreateICmpUGT(op_addr, lower_bound);
    Value *upper_cmp = builder.CreateICmpULT(op_addr, upper_bound);
    Value *in_range_cond = builder.CreateAnd(upper_cmp, lower_cmp);
    BranchInst *in_range_bi = cast<BranchInst>(SplitBlockAndInsertIfThen(in_range_cond, I, false, MDBuilder(ctx).createBranchWeights(1000, 1)));

    /* check non-zero shadow bytes */
    IRBuilder<> in_range_builder(in_range_bi);
    Value *shadow_addr = in_range_builder.CreateLShr(op_addr, in_range_builder.getInt64(0x3));
    Value *shadow_ptr = in_range_builder.CreateIntToPtr(shadow_addr, PointerType::get(IntegerType::getInt8Ty(ctx), 0));
    LoadInst *shadow_byte = in_range_builder.CreateLoad(Type::getInt8Ty(ctx), shadow_ptr);
    Value *non_zero_cond = in_range_builder.CreateICmpNE(shadow_byte, in_range_builder.getInt8(0x0));
    BranchInst *non_zero_cond_bi = cast<BranchInst>(SplitBlockAndInsertIfThen(non_zero_cond, in_range_bi, false, MDBuilder(ctx).createBranchWeights(1, 1000)));

    /* check access overflow */
    IRBuilder<> non_zero_builder(non_zero_cond_bi);
    Value *op_addr_and = non_zero_builder.CreateAnd(op_addr, non_zero_builder.getInt64(0x7));
    Value *op_addr_add = non_zero_builder.CreateAdd(op_addr_and, non_zero_builder.getInt64(op_size));
    Value *access_overflow_cond =
      non_zero_builder.CreateICmpSGT(non_zero_builder.CreateIntCast(op_addr_add, Type::getInt8Ty(ctx),
            true/*is_signed*/), shadow_byte);
    BranchInst *access_overflow_bi = cast<BranchInst>(SplitBlockAndInsertIfThen(access_overflow_cond, non_zero_cond_bi, false, MDBuilder(ctx).createBranchWeights(1, 1000)));

    /* abort */
    IRBuilder<> access_overflow_builder(access_overflow_bi);
    access_overflow_builder.CreateCall(abort_func);
  }
#if CPP_STANDARD == 11
  struct SanityPass : public FunctionPass
  {
    static char ID;
    SanityPass() : FunctionPass(ID) {}
    virtual bool runOnFunction(Function &F)
    {
      return inner_run(F);
    }
#elif CPP_STANDARD == 20
  struct SanityPass : PassInfoMixin<SanityPass>
  {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &)
    {
      return inner_run(F)? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
#endif
    bool inner_run(Function &F)
    {
      if (F.isDeclaration()) return false;

      Module *M = F.getParent();
      vector<Instruction*> inst_list;
      for (auto &B : F) {
        auto I = B.begin();
        for (; I != B.end(); ++I) {
          Value *op_ptr = nullptr;
          if (auto *si = dyn_cast<StoreInst>(I)) {
            op_ptr = si->getPointerOperand();
          } else if (auto *li = dyn_cast<LoadInst>(I)) {
            op_ptr = li->getPointerOperand();
          }
          if (op_ptr && !is_stack_pointer(op_ptr) && !is_global_pointer(op_ptr)) {
            inst_list.push_back(&*I);
          }
        }
      }

      if (!inst_list.empty()) {
        AllocaInst *min_addr_copy = alloa_gv_copy_for_int64(M, &F, "sanity_min_addr");
        AllocaInst *max_addr_copy = alloa_gv_copy_for_int64(M, &F, "sanity_max_addr");
        Function *abort_func = get_abort_func(M);
        for (auto &I : inst_list) {
          if (auto *si = dyn_cast<StoreInst>(I)) {
            inject_checker(si, si->getValueOperand()->getType(), M, min_addr_copy, max_addr_copy, abort_func);
          } else if (auto *li = dyn_cast<LoadInst>(I)) {
            inject_checker(li, li->getType(), M, min_addr_copy, max_addr_copy, abort_func);
          }
        }
      }

      bool modified = !inst_list.empty();
      return modified;
    }
  };
}

#if CPP_STANDARD == 11
char SanityPass::ID = 0;

static void register_sanitypass(const PassManagerBuilder &,
    legacy::PassManagerBase &PM)
{
  PM.add(new SanityPass());
}

static RegisterStandardPasses pass(PassManagerBuilder::EP_EarlyAsPossible,
    register_sanitypass);
#elif CPP_STANDARD == 20
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "sanity", "v1.0",
    [](PassBuilder &PB) {
      PB.registerPipelineStartEPCallback(
        [](ModulePassManager &MPM, OptimizationLevel Level) {
          MPM.addPass(createModuleToFunctionPassAdaptor(SanityPass()));
        }
      );
    }
  };
}
#else
  #error "unexpected error"
#endif
